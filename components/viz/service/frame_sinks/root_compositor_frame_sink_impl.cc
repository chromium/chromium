// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"
#include "components/viz/service/display_embedder/vsync_parameter_listener.h"
#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/geometry/skia_conversions.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "components/viz/service/frame_sinks/external_begin_frame_source_ios.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/feature_list.h"
#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/frame_sinks/external_begin_frame_source_win.h"
#endif

namespace viz {

class RootCompositorFrameSinkImpl::StandaloneBeginFrameObserver
    : public BeginFrameObserverBase {
 public:
  StandaloneBeginFrameObserver(
      mojo::PendingRemote<mojom::BeginFrameObserver> observer,
      BeginFrameSource* begin_frame_source)
      : remote_observer_(std::move(observer)),
        begin_frame_source_(begin_frame_source) {
    remote_observer_.set_disconnect_handler(base::BindOnce(
        &StandaloneBeginFrameObserver::StopObserving, base::Unretained(this)));
    begin_frame_source_->AddObserver(this);
  }

  ~StandaloneBeginFrameObserver() override { StopObserving(); }

  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    remote_observer_->OnStandaloneBeginFrame(args);
    return true;
  }

  void OnBeginFrameSourcePausedChanged(bool paused) override {}
  bool IsRoot() const override { return true; }

 private:
  void StopObserving() {
    if (!begin_frame_source_)
      return;
    begin_frame_source_->RemoveObserver(this);
    begin_frame_source_ = nullptr;
  }

  mojo::Remote<mojom::BeginFrameObserver> remote_observer_;
  raw_ptr<BeginFrameSource> begin_frame_source_;
};

// static
std::unique_ptr<RootCompositorFrameSinkImpl>
RootCompositorFrameSinkImpl::Create(
    mojom::RootCompositorFrameSinkParamsPtr params,
    FrameSinkManagerImpl* frame_sink_manager,
    OutputSurfaceProvider* output_surface_provider,
    uint32_t restart_id,
    bool run_all_compositor_stages_before_draw,
    const DebugRendererSettings* debug_settings,
    HintSessionFactory* hint_session_factory) {
  // First create an output surface.
  mojo::Remote<mojom::DisplayClient> display_client(
      std::move(params->display_client));
  auto display_controller = output_surface_provider->CreateGpuDependency(
      params->gpu_compositing, params->widget);
  auto output_surface = output_surface_provider->CreateOutputSurface(
      params->widget, params->gpu_compositing, display_client.get(),
      display_controller.get(), params->renderer_settings, debug_settings);

  // Creating output surface failed. The host can send a new request, possibly
  // with a different compositing mode.
  if (!output_surface)
    return nullptr;

  // If we need swap size notifications tell the output surface now.
  output_surface->SetNeedsSwapSizeNotifications(
      params->send_swap_size_notifications);

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  // For X11, we need notify client about swap completion after resizing, so the
  // client can use it for synchronize with X11 WM.
  output_surface->SetNeedsSwapSizeNotifications(true);
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

  // Create some sort of a BeginFrameSource, depending on the platform and
  // |params|.
  std::unique_ptr<ExternalBeginFrameSource> external_begin_frame_source;
  std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source;
  ExternalBeginFrameSourceMojo* external_begin_frame_source_mojo = nullptr;
  bool hw_support_for_multiple_refresh_rates = false;
#if BUILDFLAG(IS_MAC)
  bool created_external_begin_frame_source_mac = false;
#endif
#if !BUILDFLAG(IS_APPLE)
  bool wants_vsync_updates = false;
#endif

  if (params->external_begin_frame_controller) {
    auto owned_external_begin_frame_source_mojo =
        std::make_unique<ExternalBeginFrameSourceMojo>(
            frame_sink_manager,
            std::move(params->external_begin_frame_controller), restart_id);
    external_begin_frame_source_mojo =
        owned_external_begin_frame_source_mojo.get();
    external_begin_frame_source =
        std::move(owned_external_begin_frame_source_mojo);
  } else {
#if BUILDFLAG(IS_ANDROID)
    hw_support_for_multiple_refresh_rates = true;
    external_begin_frame_source =
        std::make_unique<ExternalBeginFrameSourceAndroid>(
            restart_id, params->refresh_rate,
            /*requires_align_with_java=*/false);
#elif BUILDFLAG(IS_IOS)
    hw_support_for_multiple_refresh_rates = true;
    external_begin_frame_source =
        std::make_unique<ExternalBeginFrameSourceIOS>(restart_id);
#else
#if BUILDFLAG(IS_CHROMEOS_ASH)
    hw_support_for_multiple_refresh_rates =
        features::IsCrosContentAdjustedRefreshRateEnabled();
#endif
    if (params->disable_frame_rate_limit) {
      synthetic_begin_frame_source =
          std::make_unique<BackToBackBeginFrameSource>(
              std::make_unique<DelayBasedTimeSource>(
                  base::SingleThreadTaskRunner::GetCurrentDefault().get()));
    } else {
#if BUILDFLAG(IS_WIN)
      // ExternalBeginFrameSourceWin also uses the D3D11 device used by dcomp.
      if (output_surface->capabilities().dc_support_level !=
          OutputSurface::DCSupportLevel::kNone) {
        // Vsync updates are required to update the FrameRateDecider with
        // supported refresh rates.
        wants_vsync_updates = true;
        external_begin_frame_source =
            std::make_unique<ExternalBeginFrameSourceWin>(
                restart_id, base::SingleThreadTaskRunner::GetCurrentDefault());
      }
#elif BUILDFLAG(IS_MAC)
      if (features::IsCVDisplayLinkBeginFrameSourceEnabled()) {
        external_begin_frame_source =
            std::make_unique<ExternalBeginFrameSourceMac>(
                restart_id, params->renderer_settings.display_id,
                output_surface.get());
        created_external_begin_frame_source_mac = true;
      } else {
        auto time_source = std::make_unique<DelayBasedTimeSource>(
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
        synthetic_begin_frame_source =
            std::make_unique<DelayBasedBeginFrameSourceMac>(
                std::move(time_source), restart_id);
      }
#endif
      if (!external_begin_frame_source && !synthetic_begin_frame_source) {
        auto time_source = std::make_unique<DelayBasedTimeSource>(
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
        synthetic_begin_frame_source =
            std::make_unique<DelayBasedBeginFrameSource>(std::move(time_source),
                                                         restart_id);
      }
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }

  BeginFrameSource* begin_frame_source = synthetic_begin_frame_source.get();
  if (external_begin_frame_source)
    begin_frame_source = external_begin_frame_source.get();
  DCHECK(begin_frame_source);

  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();

  const auto& capabilities = output_surface->capabilities();
  DCHECK_GT(capabilities.pending_swap_params.max_pending_swaps, 0);
  auto scheduler = std::make_unique<DisplayScheduler>(
      begin_frame_source, task_runner.get(), capabilities.pending_swap_params,
      hint_session_factory, run_all_compositor_stages_before_draw);

#if !BUILDFLAG(IS_APPLE)
  auto* output_surface_ptr = output_surface.get();
#endif
  gpu::SharedImageInterface* sii = nullptr;
  if (display_controller)
    sii = display_controller->shared_image_interface();

  auto overlay_processor = OverlayProcessorInterface::CreateOverlayProcessor(
      output_surface.get(), output_surface->GetSurfaceHandle(),
      output_surface->capabilities(),
      display_controller.get(), sii, params->renderer_settings, debug_settings);

  auto display = std::make_unique<Display>(
      frame_sink_manager->shared_bitmap_manager(),
      output_surface_provider->GetSharedImageManager(),
      output_surface_provider->GetSyncPointManager(),
      output_surface_provider->GetGpuScheduler(), params->renderer_settings,
      debug_settings, params->frame_sink_id, std::move(display_controller),
      std::move(output_surface), std::move(overlay_processor),
      std::move(scheduler), std::move(task_runner));

  if (external_begin_frame_source_mojo)
    external_begin_frame_source_mojo->SetDisplay(display.get());

  // base::WrapUnique instead of std::make_unique because the ctor is private.
  auto impl = base::WrapUnique(new RootCompositorFrameSinkImpl(
      frame_sink_manager, params->frame_sink_id,
      std::move(params->compositor_frame_sink),
      std::move(params->compositor_frame_sink_client),
      std::move(params->display_private), std::move(display_client),
      std::move(synthetic_begin_frame_source),
      std::move(external_begin_frame_source), std::move(display),
      hw_support_for_multiple_refresh_rates));

  // Set up the callback for updating VSyncParameters.
  // The new VSyncParameters will be sent to viz FrameRateDecider through viz
  // display_->SetSupportedFrameIntervals() in SetDisplayVSyncParameters().
  // |FrameRateDecider| decides the preferred_frame_interval and calls
  // RootCompositorFrameSinkImpl::SetPreferredFrameInterval().
#if !BUILDFLAG(IS_APPLE)
  // On Mac vsync parameter updates does not come from OutputSurface.
  if (wants_vsync_updates || impl->synthetic_begin_frame_source_) {
    // |impl| owns and outlives display, and display owns the output surface so
    // unretained is safe.
    output_surface_ptr->SetUpdateVSyncParametersCallback(base::BindRepeating(
        &RootCompositorFrameSinkImpl::SetDisplayVSyncParameters,
        base::Unretained(impl.get())));
  }
#elif BUILDFLAG(IS_MAC)
  if (impl->external_begin_frame_source_) {
    impl->external_begin_frame_source()->SetUpdateVSyncParametersCallback(
        base::BindRepeating(
            &RootCompositorFrameSinkImpl::SetDisplayVSyncParameters,
            base::Unretained(impl.get())));

    if (created_external_begin_frame_source_mac) {
      static_cast<ExternalBeginFrameSourceMac*>(
          impl->external_begin_frame_source())
          ->SetMultipleHWRefreshRatesCallback(base::BindRepeating(
              &RootCompositorFrameSinkImpl::SetHwSupportForMultipleRefreshRates,
              base::Unretained(impl.get())));
    }
  } else if (impl->synthetic_begin_frame_source_) {
    impl->synthetic_begin_frame_source_->SetUpdateVSyncParametersCallback(
        base::BindRepeating(
            &RootCompositorFrameSinkImpl::SetDisplayVSyncParameters,
            base::Unretained(impl.get())));
  }
#endif

  return impl;
}

RootCompositorFrameSinkImpl::~RootCompositorFrameSinkImpl() {
  support_->frame_sink_manager()->UnregisterBeginFrameSource(
      begin_frame_source());
}

bool RootCompositorFrameSinkImpl::WillEvictSurface(
    const SurfaceId& surface_id) {
  return eviction_handler_.WillEvictSurface(surface_id);
}

const SurfaceId& RootCompositorFrameSinkImpl::CurrentSurfaceId() const {
  return display_->CurrentSurfaceId();
}

void RootCompositorFrameSinkImpl::SetDisplayVisible(bool visible) {
  if (visible) {
    // If the display has been explicitly set to visible, no need to push any
    // eviction content.
    eviction_handler_.MaybeFinishEvictionProcess();
  }
  display_->SetVisible(visible);
}

#if BUILDFLAG(IS_WIN)
void RootCompositorFrameSinkImpl::DisableSwapUntilResize(
    DisableSwapUntilResizeCallback callback) {
  display_->DisableSwapUntilResize(std::move(callback));
}
#endif

void RootCompositorFrameSinkImpl::Resize(const gfx::Size& size) {
  if (!display_->resize_based_on_root_surface())
    display_->Resize(size);
}

void RootCompositorFrameSinkImpl::SetDisplayColorMatrix(
    const gfx::Transform& color_matrix) {
  display_->SetColorMatrix(gfx::TransformToSkM44(color_matrix));
}

void RootCompositorFrameSinkImpl::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_->SetDisplayColorSpaces(display_color_spaces);
}

#if BUILDFLAG(IS_MAC)
void RootCompositorFrameSinkImpl::SetVSyncDisplayID(int64_t display_id) {
  begin_frame_source()->SetVSyncDisplayID(display_id);
}
#endif

void RootCompositorFrameSinkImpl::SetOutputIsSecure(bool secure) {
  display_->SetOutputIsSecure(secure);
}

void RootCompositorFrameSinkImpl::SetDisplayVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  // If |use_preferred_interval_| is true, we should decide whether
  // to update the |supported_intervals_| and timebase here.
  // Otherwise, just update the display parameters (timebase & interval)
  if (use_preferred_interval_) {
    // If the incoming display interval changes, we should update the
    // |supported_intervals_| in FrameRateDecider
    if (display_frame_interval_ != interval) {
      display_frame_interval_ = interval;
      display_->SetSupportedFrameIntervals(GetSupportedFrameIntervals());
      UpdateFrameIntervalDeciderSettings();
    }

    // If there is a meaningful |preferred_frame_interval_|, firstly
    // determine the delta of next tick time using the current timebase
    // and incoming timebase.
    if (preferred_frame_interval_ !=
        FrameRateDecider::UnspecifiedFrameInterval()) {
      auto time = base::TimeTicks();
      base::TimeDelta timebase_delta =
          (time.SnappedToNextTick(timebase, display_frame_interval_) -
           time.SnappedToNextTick(display_frame_timebase_,
                                  display_frame_interval_))
              .magnitude();
      timebase_delta %= display_frame_interval_;
      timebase_delta =
          std::min(timebase_delta, display_frame_interval_ - timebase_delta);

      // If delta is more than |kMaxTimebaseDelta| of the display interval,
      // we update the timebase.
      constexpr float kMaxTimebaseDelta = 0.05;
      if (timebase_delta > display_frame_interval_ * kMaxTimebaseDelta)
        display_frame_timebase_ = timebase;
    } else {
      // |display_frame_timebase_| should be still updated as normal in
      // preferred interval mode without a meaningful
      // |preferred_frame_interval_|
      display_frame_timebase_ = timebase;
    }
  } else {
    display_frame_timebase_ = timebase;
    display_frame_interval_ = interval;
  }

  UpdateVSyncParameters();
}

base::flat_set<base::TimeDelta>
RootCompositorFrameSinkImpl::GetSupportedFrameIntervals() {
  if (!exact_supported_refresh_rates_.empty()) {
    base::flat_set<base::TimeDelta> supported_frame_intervals;
    for (auto& [supported_interval, rate] : exact_supported_refresh_rates_) {
      supported_frame_intervals.insert(supported_interval);
    }
    return supported_frame_intervals;
  }
  if (external_begin_frame_source_) {
    return external_begin_frame_source_->GetSupportedFrameIntervals(
        display_frame_interval_);
  }

  return {display_frame_interval_, display_frame_interval_ * 2};
}

void RootCompositorFrameSinkImpl::UpdateVSyncParameters() {
  base::TimeTicks timebase = display_frame_timebase_;

  // Overwrite the interval with a meaningful one here if
  // |use_preferred_interval_|
  base::TimeDelta interval =
      use_preferred_interval_ &&
              preferred_frame_interval_ !=
                  FrameRateDecider::UnspecifiedFrameInterval()
          ? preferred_frame_interval_
          : display_frame_interval_;

  if (synthetic_begin_frame_source_) {
    synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
    if (vsync_listener_)
      vsync_listener_->OnVSyncParametersUpdated(timebase, interval);
  }
  if (external_begin_frame_source_)
    external_begin_frame_source_->SetPreferredInterval(interval);
}

void RootCompositorFrameSinkImpl::ForceImmediateDrawAndSwapIfPossible() {
  display_->ForceImmediateDrawAndSwapIfPossible();
}

#if BUILDFLAG(IS_ANDROID)
void RootCompositorFrameSinkImpl::SetVSyncPaused(bool paused) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void RootCompositorFrameSinkImpl::UpdateRefreshRate(float refresh_rate) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->UpdateRefreshRate(refresh_rate);
}

void RootCompositorFrameSinkImpl::PreserveChildSurfaceControls() {
  display_->PreserveChildSurfaceControls();
}

void RootCompositorFrameSinkImpl::SetSwapCompletionCallbackEnabled(
    bool enable) {
  enable_swap_completion_callback_ = enable;
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
void RootCompositorFrameSinkImpl::SetSupportedRefreshRates(
    const std::vector<float>& supported_refresh_rates) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK_NE(use_preferred_interval_,
           features::IsCrosContentAdjustedRefreshRateEnabled());
  if (use_preferred_interval_) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  exact_supported_refresh_rates_.clear();
  for (float rate : supported_refresh_rates) {
    const base::TimeDelta interval = base::Hertz(rate);
    exact_supported_refresh_rates_[interval] = rate;
  }

  display_->SetSupportedFrameIntervals(GetSupportedFrameIntervals());
  UpdateFrameIntervalDeciderSettings();
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)

void RootCompositorFrameSinkImpl::AddVSyncParameterObserver(
    mojo::PendingRemote<mojom::VSyncParameterObserver> observer) {
  vsync_listener_ =
      std::make_unique<VSyncParameterListener>(std::move(observer));
}

void RootCompositorFrameSinkImpl::SetDelegatedInkPointRenderer(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver) {
  display_->InitDelegatedInkPointRendererReceiver(std::move(receiver));
}

void RootCompositorFrameSinkImpl::SetStandaloneBeginFrameObserver(
    mojo::PendingRemote<mojom::BeginFrameObserver> observer) {
  standalone_begin_frame_observer_ =
      std::make_unique<StandaloneBeginFrameObserver>(std::move(observer),
                                                     begin_frame_source());
}

void RootCompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void RootCompositorFrameSinkImpl::SetWantsAnimateOnlyBeginFrames() {
  support_->SetWantsAnimateOnlyBeginFrames();
}

void RootCompositorFrameSinkImpl::SetWantsBeginFrameAcks() {
  support_->SetWantsBeginFrameAcks();
}

void RootCompositorFrameSinkImpl::SetAutoNeedsBeginFrame() {
  support_->SetAutoNeedsBeginFrame();
}

void RootCompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    std::optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time) {
  if (support_->last_activated_local_surface_id() != local_surface_id &&
      !support_->IsEvicted(local_surface_id)) {
    display_->SetLocalSurfaceId(local_surface_id, frame.device_scale_factor());
    // Resize the |display_| to the root compositor frame |output_rect| so that
    // we won't show root surface gutters.
    if (display_->resize_based_on_root_surface())
      display_->Resize(frame.render_pass_list.back()->output_rect.size());
  }

  const auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id, std::move(frame), std::move(hit_test_region_list),
      submit_time, SubmitCompositorFrameSyncCallback());
  if (result == SubmitResult::ACCEPTED)
    return;

  const char* reason =
      CompositorFrameSinkSupport::GetSubmitResultAsString(result);
  DLOG(ERROR) << "SubmitCompositorFrame failed for " << local_surface_id
              << " because " << reason;
  compositor_frame_sink_receiver_.ResetWithReason(static_cast<uint32_t>(result),
                                                  reason);
}

void RootCompositorFrameSinkImpl::SubmitCompositorFrameSync(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    std::optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    SubmitCompositorFrameSyncCallback callback) {
  NOTIMPLEMENTED();
}

void RootCompositorFrameSinkImpl::DidNotProduceFrame(
    const BeginFrameAck& begin_frame_ack) {
  support_->DidNotProduceFrame(begin_frame_ack);
}

void RootCompositorFrameSinkImpl::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const SharedBitmapId& id) {
  if (!support_->DidAllocateSharedBitmap(std::move(region), id)) {
    DLOG(ERROR) << "DidAllocateSharedBitmap failed for duplicate "
                << "SharedBitmapId";
    compositor_frame_sink_receiver_.reset();
  }
}

void RootCompositorFrameSinkImpl::DidDeleteSharedBitmap(
    const SharedBitmapId& id) {
  support_->DidDeleteSharedBitmap(id);
}

void RootCompositorFrameSinkImpl::InitializeCompositorFrameSinkType(
    mojom::CompositorFrameSinkType type) {
  support_->InitializeCompositorFrameSinkType(type);
}

void RootCompositorFrameSinkImpl::BindLayerContext(
    mojom::PendingLayerContextPtr context) {
  support_->BindLayerContext(*context);
}

#if BUILDFLAG(IS_ANDROID)
void RootCompositorFrameSinkImpl::SetThreadIds(
    const std::vector<int32_t>& thread_ids) {
  support_->SetThreadIds(/*from_untrusted_client=*/false,
                         base::MakeFlatSet<base::PlatformThreadId>(thread_ids));
}
#endif

RootCompositorFrameSinkImpl::RootCompositorFrameSinkImpl(
    FrameSinkManagerImpl* frame_sink_manager,
    const FrameSinkId& frame_sink_id,
    mojo::PendingAssociatedReceiver<mojom::CompositorFrameSink>
        frame_sink_receiver,
    mojo::PendingRemote<mojom::CompositorFrameSinkClient> frame_sink_client,
    mojo::PendingAssociatedReceiver<mojom::DisplayPrivate> display_receiver,
    mojo::Remote<mojom::DisplayClient> display_client,
    std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source,
    std::unique_ptr<ExternalBeginFrameSource> external_begin_frame_source,
    std::unique_ptr<Display> display,
    bool hw_support_for_multiple_refresh_rates)
    : compositor_frame_sink_client_(std::move(frame_sink_client)),
      compositor_frame_sink_receiver_(this, std::move(frame_sink_receiver)),
      display_client_(std::move(display_client)),
      display_private_receiver_(this, std::move(display_receiver)),
      support_(std::make_unique<CompositorFrameSinkSupport>(
          compositor_frame_sink_client_.get(),
          frame_sink_manager,
          frame_sink_id,
          /*is_root=*/true)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      external_begin_frame_source_(std::move(external_begin_frame_source)),
      display_(std::move(display)),
      eviction_handler_(display_.get(),
                        support_.get(),
                        frame_sink_manager->reserved_resource_id_tracker()) {
  DCHECK(display_);
  DCHECK(begin_frame_source());
  frame_sink_manager->RegisterBeginFrameSource(begin_frame_source(),
                                               support_->frame_sink_id());
  display_->Initialize(this, support_->frame_sink_manager()->surface_manager(),
                       hw_support_for_multiple_refresh_rates);
  support_->SetUpHitTest(display_.get());
#if BUILDFLAG(IS_IOS)
  // iOS supports preferred refresh rate interval set as a hint how often a
  // client wants to refresh the content. It works two ways - a client setting a
  // preferred refresh rate and the system throttling the refresh rate in case
  // of battery saving or any other events.
  DCHECK(hw_support_for_multiple_refresh_rates);
  use_preferred_interval_ = true;
#else
  if (!hw_support_for_multiple_refresh_rates) {
    display_->SetSupportedFrameIntervals(GetSupportedFrameIntervals());
    use_preferred_interval_ = true;
  }
#endif

  if (external_begin_frame_source_) {
    display_frame_interval_ =
        external_begin_frame_source_->GetMaximumRefreshFrameInterval();
  }

#if BUILDFLAG(IS_ANDROID)
  interval_decider_use_fixed_intervals_ =
      !display_->OutputSurfaceSupportsSetFrameRate();
#endif
  UpdateFrameIntervalDeciderSettings();
}

void RootCompositorFrameSinkImpl::UpdateFrameIntervalDeciderSettings() {
  FrameIntervalDecider* decider = display_->frame_interval_decider();
  if (!decider) {
    return;
  }

  // TODO(crbug.com/346732738): This is only correctly configured for Android
  // and ChromeOS. Support other platforms / configurations.

  // Note that matcher order defines precedence.
  std::vector<std::unique_ptr<FrameIntervalMatcher>> matchers;
  matchers.push_back(std::make_unique<InputBoostMatcher>());

#if BUILDFLAG(IS_ANDROID)
  matchers.push_back(std::make_unique<OnlyVideoMatcher>());
  matchers.push_back(std::make_unique<OnlyAnimatingImageMatcher>());
  matchers.push_back(std::make_unique<OnlyScrollBarFadeOutAnimationMatcher>());
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(features::kSingleVideoFrameRateThrottling)) {
    matchers.push_back(std::make_unique<OnlyVideoMatcher>());
  }
  // Only desktop platforms get VideoConferenceMatcher.
  matchers.push_back(std::make_unique<VideoConferenceMatcher>());
#endif

  FrameIntervalDecider::Settings settings = decider->settings();
  if (interval_decider_use_fixed_intervals_) {
    FrameIntervalMatcher::FixedIntervalSettings fixed_interval_settings;
    fixed_interval_settings.supported_intervals = GetSupportedFrameIntervals();
    fixed_interval_settings.default_interval =
        *fixed_interval_settings.supported_intervals.begin();
    settings.interval_settings = fixed_interval_settings;
  } else if (max_vsync_interval_.has_value()) {
    FrameIntervalMatcher::ContinuousRangeSettings continuous_range_settings;
    continuous_range_settings.min_interval =
        *GetSupportedFrameIntervals().begin();
    continuous_range_settings.max_interval = max_vsync_interval_.value();
    settings.interval_settings = continuous_range_settings;
  } else {
    settings.interval_settings = {};
  }

  // Unretained is safe since this owns Display which owns FrameIntervalDecider.
  settings.result_callback = base::BindRepeating(
      &RootCompositorFrameSinkImpl::FrameIntervalDeciderResultCallback,
      base::Unretained(this));
  decider->UpdateSettings(std::move(settings), std::move(matchers));
}

void RootCompositorFrameSinkImpl::FrameIntervalDeciderResultCallback(
    FrameIntervalDecider::Result result,
    FrameIntervalMatcherType matcher_type) {
  // TODO(crbug.com/346732738): This is only correctly configured for Android
  // and ChromeOS. Support other platforms / configurations.

  base::TimeDelta interval = absl::visit(
      base::Overloaded(
          [](FrameIntervalDecider::FrameIntervalClass frame_interval_class) {
            // For now, setting 0 implies no preference, and
            // allow the OS to use its own heuristics to
            // estimate.
            return base::Milliseconds(0);
          },
          [](base::TimeDelta interval) { return interval; }),
      result);

  if (decided_display_interval_ == interval) {
    return;
  }
  decided_display_interval_ = interval;

#if BUILDFLAG(IS_ANDROID)
  if (display_->OutputSurfaceSupportsSetFrameRate()) {
    display_->SetFrameIntervalOnOutputSurface(interval);
    return;
  }
#endif
  SetPreferredFrameInterval(interval);
}

void RootCompositorFrameSinkImpl::DisplayOutputSurfaceLost() {
  // |display_| has encountered an error and needs to be recreated. Reset
  // message pipes from the client, the client will see the connection error and
  // recreate the CompositorFrameSink+Display.
  compositor_frame_sink_receiver_.reset();
  display_private_receiver_.reset();
}

void RootCompositorFrameSinkImpl::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    AggregatedRenderPassList* render_passes) {
  DCHECK(support_->GetHitTestAggregator());
  support_->GetHitTestAggregator()->Aggregate(display_->CurrentSurfaceId());
}

#if BUILDFLAG(IS_ANDROID)
base::ScopedClosureRunner RootCompositorFrameSinkImpl::GetCacheBackBufferCb() {
  return display_->GetCacheBackBufferCb();
}
#endif

void RootCompositorFrameSinkImpl::SetHwSupportForMultipleRefreshRates(
    bool support) {
  display_->SetHwSupportForMultipleRefreshRates(support);
}

void RootCompositorFrameSinkImpl::StartOverdrawTracking(
    int interval_length_in_seconds) {
  display_->StartTrackingOverdraw(interval_length_in_seconds);
}

OverdrawTracker::OverdrawTimeSeries
RootCompositorFrameSinkImpl::StopOverdrawTracking() {
  return display_->StopTrackingOverdraw();
}

void RootCompositorFrameSinkImpl::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
#if BUILDFLAG(IS_APPLE)
  // If |ca_layer_params| should have content only when there exists a client
  // to send it to.
  DCHECK(ca_layer_params.is_empty || display_client_);
  if (last_ca_layer_params_ == ca_layer_params &&
      base::TimeTicks::Now() < next_forced_ca_layer_params_update_time_) {
    return;
  }
  last_ca_layer_params_ = ca_layer_params;
  // OnDisplayReceivedCALayerParams() is ultimately responsible for triggering
  // updates to vsync. VSync may change dynamically. To ensure the value is
  // updated correctly, OnDisplayReceivedCALayerParams() is periodically called,
  // even if the params haven't changed. The value here matches that of
  // DisplayLinkMac, which is responsible for querying for vsync updates.
  next_forced_ca_layer_params_update_time_ =
      base::TimeTicks::Now() + base::Seconds(10);
  if (display_client_)
    display_client_->OnDisplayReceivedCALayerParams(ca_layer_params);
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void RootCompositorFrameSinkImpl::DisplayDidCompleteSwapWithSize(
    const gfx::Size& pixel_size) {
#if BUILDFLAG(IS_ANDROID)
  if (display_client_ && enable_swap_completion_callback_) {
    display_client_->DidCompleteSwapWithSize(pixel_size);
  }
#elif BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  if (display_client_ && pixel_size != last_swap_pixel_size_) {
    last_swap_pixel_size_ = pixel_size;
    display_client_->DidCompleteSwapWithNewSize(last_swap_pixel_size_);
  }
#else  // !BUILDFLAG(IS_ANDROID) && !(BUILDFLAG(IS_LINUX) &&
       // BUILDFLAG(IS_OZONE_X11))
  NOTREACHED_IN_MIGRATION();
#endif
}

void RootCompositorFrameSinkImpl::DisplayAddChildWindowToBrowser(
    gpu::SurfaceHandle child_window) {
#if BUILDFLAG(IS_WIN)
  if (display_client_) {
    display_client_->AddChildWindowToBrowser(child_window);
  }
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void RootCompositorFrameSinkImpl::SetWideColorEnabled(bool enabled) {
#if BUILDFLAG(IS_ANDROID)
  if (display_client_)
    display_client_->SetWideColorEnabled(enabled);
#endif
}

void RootCompositorFrameSinkImpl::SetPreferredFrameInterval(
    base::TimeDelta interval) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK_NE(use_preferred_interval_,
           features::IsCrosContentAdjustedRefreshRateEnabled());
  if (use_preferred_interval_) {
    preferred_frame_interval_ = interval;
    UpdateVSyncParameters();
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH))

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  if (display_client_) {
    float refresh_rate;
    if (interval.is_zero()) {
      refresh_rate = 0;
    } else {
      auto it = exact_supported_refresh_rates_.find(interval);
      if (it != exact_supported_refresh_rates_.end()) {
        refresh_rate = it->second;
      } else {
        refresh_rate = 1 / interval.InSecondsF();
        LOG_IF(WARNING, interval_decider_use_fixed_intervals_)
            << "Requested unsupported preferred frame interval " << interval
            << " (=" << refresh_rate << "Hz)";
      }
    }
    display_client_->SetPreferredRefreshRate(refresh_rate);
  }
#else
  preferred_frame_interval_ = interval;
  UpdateVSyncParameters();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
}

base::TimeDelta
RootCompositorFrameSinkImpl::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id,
    mojom::CompositorFrameSinkType* type) {
  return support_->frame_sink_manager()
      ->GetPreferredFrameIntervalForFrameSinkId(id, type);
}

void RootCompositorFrameSinkImpl::DisplayDidDrawAndSwap() {
  eviction_handler_.DisplayDidDrawAndSwap();
}

BeginFrameSource* RootCompositorFrameSinkImpl::begin_frame_source() {
  if (external_begin_frame_source_) {
    return external_begin_frame_source_.get();
  }
  return synthetic_begin_frame_source_.get();
}

void RootCompositorFrameSinkImpl::SetMaxVSyncAndVrr(
    std::optional<base::TimeDelta> max_vsync_interval,
    display::VariableRefreshRateState vrr_state) {
  max_vsync_interval_ = max_vsync_interval;

  if (synthetic_begin_frame_source_) {
    synthetic_begin_frame_source_->SetMaxVrrInterval(
        vrr_state == display::VariableRefreshRateState::kVrrEnabled
            ? max_vsync_interval
            : std::nullopt);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!use_preferred_interval_) {
    interval_decider_use_fixed_intervals_ = !max_vsync_interval.has_value();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  UpdateFrameIntervalDeciderSettings();
}

}  // namespace viz
