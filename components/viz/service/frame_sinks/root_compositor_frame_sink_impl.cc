// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/root_compositor_frame_sink_impl.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
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
    bool run_all_compositor_stages_before_draw) {
  // First create an output surface.
  mojo::Remote<mojom::DisplayClient> display_client(
      std::move(params->display_client));
  auto output_surface = output_surface_provider->CreateOutputSurface(
      params->widget, params->gpu_compositing, display_client.get(),
      params->renderer_settings);

  // Creating output surface failed. The host can send a new request, possibly
  // with a different compositing mode.
  if (!output_surface)
    return nullptr;

  // If we need swap size notifications tell the output surface now.
  output_surface->SetNeedsSwapSizeNotifications(
      params->send_swap_size_notifications);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // For X11, we need notify client about swap completion after resizing, so the
  // client can use it for synchronize with X11 WM.
  output_surface->SetNeedsSwapSizeNotifications(true);
#endif

  // Create some sort of a BeginFrameSource, depending on the platform and
  // |params|.
  std::unique_ptr<ExternalBeginFrameSource> external_begin_frame_source;
  std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source;
  ExternalBeginFrameSourceMojo* external_begin_frame_source_mojo = nullptr;

  if (params->external_begin_frame_controller.is_pending()) {
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

  auto* output_surface_ptr = output_surface.get();

  auto display = std::make_unique<Display>(
      frame_sink_manager->shared_bitmap_manager(), params->renderer_settings,
      params->frame_sink_id, std::move(output_surface), std::move(scheduler),
      std::move(task_runner));

  if (external_begin_frame_source_mojo)
    external_begin_frame_source_mojo->SetDisplay(display.get());

  // base::WrapUnique instead of std::make_unique because the ctor is private.
  auto impl = base::WrapUnique(new RootCompositorFrameSinkImpl(
      frame_sink_manager, params->frame_sink_id,
      std::move(params->compositor_frame_sink),
      std::move(params->compositor_frame_sink_client),
      std::move(params->display_private), std::move(display_client),
      std::move(synthetic_begin_frame_source),
      std::move(external_begin_frame_source), std::move(display)));

#if defined(OS_MACOSX)
  // On Mac vsync parameter updates come from the browser process. We don't need
  // to provide a callback to the OutputSurface since it should never use it.
  constexpr bool wants_vsync_updates = false;
#else
  constexpr bool wants_vsync_updates = true;
#endif
  if (wants_vsync_updates && impl->synthetic_begin_frame_source_) {
    // |impl| owns and outlives display, and display owns the output surface so
    // unretained is safe.
    output_surface_ptr->SetUpdateVSyncParametersCallback(base::BindRepeating(
        &RootCompositorFrameSinkImpl::SetDisplayVSyncParameters,
        base::Unretained(impl.get())));
  }

  return impl;
}

RootCompositorFrameSinkImpl::~RootCompositorFrameSinkImpl() {
  support_->frame_sink_manager()->UnregisterBeginFrameSource(
      begin_frame_source());
}

void RootCompositorFrameSinkImpl::SetDisplayVisible(bool visible) {
  display_->SetVisible(visible);
}

void RootCompositorFrameSinkImpl::DisableSwapUntilResize(
    DisableSwapUntilResizeCallback callback) {
  display_->DisableSwapUntilResize(std::move(callback));
}

void RootCompositorFrameSinkImpl::Resize(const gfx::Size& size) {
  display_->Resize(size);
}

void RootCompositorFrameSinkImpl::SetDisplayColorMatrix(
    const gfx::Transform& color_matrix) {
  display_->SetColorMatrix(color_matrix.matrix());
}

void RootCompositorFrameSinkImpl::SetDisplayColorSpace(
    const gfx::ColorSpace& device_color_space,
    float sdr_white_level) {
  display_->SetColorSpace(device_color_space, sdr_white_level);
}

void RootCompositorFrameSinkImpl::SetOutputIsSecure(bool secure) {
  display_->SetOutputIsSecure(secure);
}

void RootCompositorFrameSinkImpl::SetDisplayVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (synthetic_begin_frame_source_) {
    synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
    if (vsync_listener_)
      vsync_listener_->OnVSyncParametersUpdated(timebase, interval);
  }
}

void RootCompositorFrameSinkImpl::ForceImmediateDrawAndSwapIfPossible() {
  display_->ForceImmediateDrawAndSwapIfPossible();
}

void RootCompositorFrameSinkImpl::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_->SetDisplayTransformHint(transform);
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

#endif  // defined(OS_ANDROID)

void RootCompositorFrameSinkImpl::AddVSyncParameterObserver(
    mojo::PendingRemote<mojom::VSyncParameterObserver> observer) {
  vsync_listener_ =
      std::make_unique<VSyncParameterListener>(std::move(observer));
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
  if (support_->last_activated_local_surface_id() != local_surface_id)
    display_->SetLocalSurfaceId(local_surface_id, frame.device_scale_factor());

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
    std::unique_ptr<Display> display)
    : compositor_frame_sink_client_(std::move(frame_sink_client)),
      compositor_frame_sink_receiver_(this, std::move(frame_sink_receiver)),
      display_client_(std::move(display_client)),
      display_private_receiver_(this, std::move(display_receiver)),
      support_(std::make_unique<CompositorFrameSinkSupport>(
          compositor_frame_sink_client_.get(),
          frame_sink_manager,
          frame_sink_id,
          /*is_root=*/true,
          /*needs_sync_points=*/true)),
      synthetic_begin_frame_source_(std::move(synthetic_begin_frame_source)),
      external_begin_frame_source_(std::move(external_begin_frame_source)),
      display_(std::move(display)) {
  DCHECK(display_);
  DCHECK(begin_frame_source());
  frame_sink_manager->RegisterBeginFrameSource(begin_frame_source(),
                                               support_->frame_sink_id());
  display_->Initialize(this, support_->frame_sink_manager()->surface_manager());
  support_->SetUpHitTest(display_.get());
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
    RenderPassList* render_passes) {
  DCHECK(support_->GetHitTestAggregator());
  support_->GetHitTestAggregator()->Aggregate(display_->CurrentSurfaceId(),
                                              render_passes);
}

base::ScopedClosureRunner RootCompositorFrameSinkImpl::GetCacheBackBufferCb() {
  return display_->GetCacheBackBufferCb();
}

void RootCompositorFrameSinkImpl::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
#if defined(OS_MACOSX)
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
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (display_client_ && pixel_size != last_swap_pixel_size_) {
    last_swap_pixel_size_ = pixel_size;
    display_client_->DidCompleteSwapWithNewSize(last_swap_pixel_size_);
  }
#else
  NOTREACHED();
  ALLOW_UNUSED_LOCAL(display_client_);
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
  NOTREACHED();
#endif
}

base::TimeDelta
RootCompositorFrameSinkImpl::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id) {
  return support_->frame_sink_manager()
      ->GetPreferredFrameIntervalForFrameSinkId(id);
}

void RootCompositorFrameSinkImpl::DisplayDidDrawAndSwap() {}

BeginFrameSource* RootCompositorFrameSinkImpl::begin_frame_source() {
  if (external_begin_frame_source_)
    return external_begin_frame_source_.get();
  return synthetic_begin_frame_source_.get();
}

}  // namespace viz
