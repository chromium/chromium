// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/delegated_ink_point_renderer_base.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"
#include "components/viz/service/display_embedder/vsync_parameter_listener.h"
#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/gpu_vsync_begin_frame_source.h"
#include "components/viz/service/hit_test/hit_test_aggregator.h"

#if defined(OS_ANDROID)
#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"
#endif

namespace viz {

// static
std::unique_ptr<RootCompositorFrameSinkImpl>
RootCompositorFrameSinkImpl::Create(
    mojom::RootCompositorFrameSinkParamsPtr params,
    FrameSinkManagerImpl* frame_sink_manager,
    OutputSurfaceProvider* output_surface_provider,
    uint32_t restart_id,
    bool run_all_compositor_stages_before_draw,
    const DebugRendererSettings* debug_settings) {
  // First create an output surface.
  mojo::Remote<mojom::DisplayClient> display_client(
      std::move(params->display_client));
  auto display_controller = output_surface_provider->CreateGpuDependency(
      params->gpu_compositing, params->widget, params->renderer_settings);
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

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // For X11, we need notify client about swap completion after resizing, so the
  // client can use it for synchronize with X11 WM.
  output_surface->SetNeedsSwapSizeNotifications(true);
#endif

  // Create some sort of a BeginFrameSource, depending on the platform and
  // |params|.
  std::unique_ptr<ExternalBeginFrameSource> external_begin_frame_source;
  std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source;
  ExternalBeginFrameSourceMojo* external_begin_frame_source_mojo = nullptr;
  bool hw_support_for_multiple_refresh_rates = false;
  bool wants_vsync_updates = false;

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
#if defined(OS_ANDROID)
    hw_support_for_multiple_refresh_rates = true;
    external_begin_frame_source =
        std::make_unique<ExternalBeginFrameSourceAndroid>(restart_id,
                                                          params->refresh_rate);
#else
    if (params->disable_frame_rate_limit) {
      synthetic_begin_frame_source =
          std::make_unique<BackToBackBeginFrameSource>(
              std::make_unique<DelayBasedTimeSource>(
                  base::ThreadTaskRunnerHandle::Get().get()));
    } else if (output_surface->capabilities().supports_gpu_vsync) {
#if defined(OS_WIN)
      hw_support_for_multiple_refresh_rates =
          output_surface->capabilities().supports_dc_layers &&
          params->set_present_duration_allowed;
#endif
      // Vsync updates are required to update the FrameRateDecider with
      // supported refresh rates.
      wants_vsync_updates = params->use_preferred_interval_for_video;
      external_begin_frame_source = std::make_unique<GpuVSyncBeginFrameSource>(
          restart_id, output_surface.get());
    } else {
      synthetic_begin_frame_source =
          std::make_unique<DelayBasedBeginFrameSource>(
              std::make_unique<DelayBasedTimeSource>(
                  base::ThreadTaskRunnerHandle::Get().get()),
              restart_id);
    }
#endif  // OS_ANDROID
  }

  BeginFrameSource* begin_frame_source = synthetic_begin_frame_source.get();
  if (external_begin_frame_source)
    begin_frame_source = external_begin_frame_source.get();
  DCHECK(begin_frame_source);

  auto task_runner = base::ThreadTaskRunnerHandle::Get();

  int max_frames_pending = output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  auto scheduler = std::make_unique<DisplayScheduler>(
      begin_frame_source, task_runner.get(), max_frames_pending,
      run_all_compositor_stages_before_draw);

#if !defined(OS_APPLE)
  auto* output_surface_ptr = output_surface.get();
#endif
  gpu::SharedImageInterface* sii = nullptr;
  if (output_surface->context_provider())
    sii = output_surface->context_provider()->SharedImageInterface();
  else if (display_controller)
    sii = display_controller->shared_image_interface();

  auto overlay_processor = OverlayProcessorInterface::CreateOverlayProcessor(
      output_surface.get(), output_surface->GetSurfaceHandle(),
      output_surface->capabilities(),
      display_controller.get(), sii, params->renderer_settings, debug_settings);

  auto display = std::make_unique<Display>(
      frame_sink_manager->shared_bitmap_manager(), params->renderer_settings,
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
      params->use_preferred_interval_for_video,
      hw_support_for_multiple_refresh_rates));

#if !defined(OS_APPLE)
  // On Mac vsync parameter updates come from the browser process. We don't need
  // to provide a callback to the OutputSurface since it should never use it.
  if (wants_vsync_updates || impl->synthetic_begin_frame_source_) {
    // |impl| owns and outlives display, and display owns the output surface so
    // unretained is safe.
    output_surface_ptr->SetUpdateVSyncParametersCallback(base::BindRepeating(
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

void RootCompositorFrameSinkImpl::SetDisplayVisible(bool visible) {
  display_->SetVisible(visible);
}

#if defined(OS_WIN)
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
  display_->SetColorMatrix(color_matrix.matrix());
}

void RootCompositorFrameSinkImpl::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_->SetDisplayColorSpaces(display_color_spaces);
}

void RootCompositorFrameSinkImpl::SetOutputIsSecure(bool secure) {
  display_->SetOutputIsSecure(secure);
}

void RootCompositorFrameSinkImpl::SetDisplayVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  // If |use_preferred_interval_| is true, we should decide wheter
  // to update the |supported_intervals_| and timebase here.
  // Otherwise, just update the display parameters (timebase & interval)
  if (use_preferred_interval_) {
    // If the incoming display interval changes, we should update the
    // |supported_intervals_| in FrameRateDecider
    if (display_frame_interval_ != interval) {
      display_->SetSupportedFrameIntervals({interval, interval * 2});
      display_frame_interval_ = interval;
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

#if defined(OS_ANDROID)
void RootCompositorFrameSinkImpl::SetVSyncPaused(bool paused) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void RootCompositorFrameSinkImpl::UpdateRefreshRate(float refresh_rate) {
  if (external_begin_frame_source_)
    external_begin_frame_source_->UpdateRefreshRate(refresh_rate);
}

void RootCompositorFrameSinkImpl::SetSupportedRefreshRates(
    const std::vector<float>& supported_refresh_rates) {
  std::vector<base::TimeDelta> supported_frame_intervals(
      supported_refresh_rates.size());
  for (size_t i = 0; i < supported_refresh_rates.size(); ++i) {
    supported_frame_intervals[i] =
        base::TimeDelta::FromSecondsD(1 / supported_refresh_rates[i]);
  }

  display_->SetSupportedFrameIntervals(supported_frame_intervals);
}

void RootCompositorFrameSinkImpl::PreserveChildSurfaceControls() {
  display_->PreserveChildSurfaceControls();
}

#endif  // defined(OS_ANDROID)

void RootCompositorFrameSinkImpl::AddVSyncParameterObserver(
    mojo::PendingRemote<mojom::VSyncParameterObserver> observer) {
  vsync_listener_ =
      std::make_unique<VSyncParameterListener>(std::move(observer));
}

void RootCompositorFrameSinkImpl::SetDelegatedInkPointRenderer(
    mojo::PendingReceiver<mojom::DelegatedInkPointRenderer> receiver) {
  if (auto* ink_renderer = display_->GetDelegatedInkPointRenderer())
    ink_renderer->InitMessagePipeline(std::move(receiver));
}

void RootCompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  support_->SetNeedsBeginFrame(needs_begin_frame);
}

void RootCompositorFrameSinkImpl::SetWantsAnimateOnlyBeginFrames() {
  support_->SetWantsAnimateOnlyBeginFrames();
}

void RootCompositorFrameSinkImpl::SubmitCompositorFrame(
    const LocalSurfaceId& local_surface_id,
    CompositorFrame frame,
    base::Optional<HitTestRegionList> hit_test_region_list,
    uint64_t submit_time) {
  if (support_->last_activated_local_surface_id() != local_surface_id) {
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
    base::Optional<HitTestRegionList> hit_test_region_list,
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
    bool use_preferred_interval_for_video,
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
      display_(std::move(display)) {
  DCHECK(display_);
  DCHECK(begin_frame_source());
  frame_sink_manager->RegisterBeginFrameSource(begin_frame_source(),
                                               support_->frame_sink_id());
  display_->Initialize(this, support_->frame_sink_manager()->surface_manager(),
                       Display::kEnableSharedImages,
                       hw_support_for_multiple_refresh_rates);
  support_->SetUpHitTest(display_.get());
  if (use_preferred_interval_for_video &&
      !hw_support_for_multiple_refresh_rates) {
    display_->SetSupportedFrameIntervals(
        {display_frame_interval_, display_frame_interval_ * 2});
    use_preferred_interval_ = true;
  }
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
  support_->GetHitTestAggregator()->Aggregate(display_->CurrentSurfaceId(),
                                              render_passes);
}

base::ScopedClosureRunner RootCompositorFrameSinkImpl::GetCacheBackBufferCb() {
  return display_->GetCacheBackBufferCb();
}

void RootCompositorFrameSinkImpl::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
#if defined(OS_APPLE)
  // If |ca_layer_params| should have content only when there exists a client
  // to send it to.
  DCHECK(ca_layer_params.is_empty || display_client_);
  if (display_client_)
    display_client_->OnDisplayReceivedCALayerParams(ca_layer_params);
#else
  NOTREACHED();
  ALLOW_UNUSED_LOCAL(display_client_);
#endif
}

void RootCompositorFrameSinkImpl::DisplayDidCompleteSwapWithSize(
    const gfx::Size& pixel_size) {
#if defined(OS_ANDROID)
  if (display_client_)
    display_client_->DidCompleteSwapWithSize(pixel_size);
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (display_client_ && pixel_size != last_swap_pixel_size_) {
    last_swap_pixel_size_ = pixel_size;
    display_client_->DidCompleteSwapWithNewSize(last_swap_pixel_size_);
  }
#else
  NOTREACHED();
  ALLOW_UNUSED_LOCAL(display_client_);
#endif
}

void RootCompositorFrameSinkImpl::SetWideColorEnabled(bool enabled) {
#if defined(OS_ANDROID)
  if (display_client_)
    display_client_->SetWideColorEnabled(enabled);
#endif
}

void RootCompositorFrameSinkImpl::SetPreferredFrameInterval(
    base::TimeDelta interval) {
#if defined(OS_ANDROID)
  float refresh_rate =
      interval.InSecondsF() == 0 ? 0 : (1 / interval.InSecondsF());
  if (display_client_)
    display_client_->SetPreferredRefreshRate(refresh_rate);
#else
  preferred_frame_interval_ = interval;
  UpdateVSyncParameters();
#endif
}

base::TimeDelta
RootCompositorFrameSinkImpl::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id,
    mojom::CompositorFrameSinkType* type) {
  return support_->frame_sink_manager()
      ->GetPreferredFrameIntervalForFrameSinkId(id, type);
}

void RootCompositorFrameSinkImpl::DisplayDidDrawAndSwap() {}

BeginFrameSource* RootCompositorFrameSinkImpl::begin_frame_source() {
  if (external_begin_frame_source_)
    return external_begin_frame_source_.get();
  return synthetic_begin_frame_source_.get();
}

}  // namespace viz
