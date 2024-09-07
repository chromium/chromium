// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_ROOT_COMPOSITOR_FRAME_SINK_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_ROOT_COMPOSITOR_FRAME_SINK_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/frame_interval_decider.h"
#include "components/viz/service/display/frame_rate_decider.h"
#include "components/viz/service/display/overdraw_tracker.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/eviction_handler.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/begin_frame_observer.mojom.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/ca_layer_params.h"

namespace viz {

class Display;
class OutputSurfaceProvider;
class ExternalBeginFrameSource;
class FrameSinkManagerImpl;
class HintSessionFactory;
class SyntheticBeginFrameSource;
class VSyncParameterListener;

// The viz portion of a root CompositorFrameSink. Holds the Binding/InterfacePtr
// for the mojom::CompositorFrameSink interface and owns the Display.
class VIZ_SERVICE_EXPORT RootCompositorFrameSinkImpl
    : public mojom::CompositorFrameSink,
      public mojom::DisplayPrivate,
      public DisplayClient {
 public:
  // Creates a new RootCompositorFrameSinkImpl.
  static std::unique_ptr<RootCompositorFrameSinkImpl> Create(
      mojom::RootCompositorFrameSinkParamsPtr params,
      FrameSinkManagerImpl* frame_sink_manager,
      OutputSurfaceProvider* output_surface_provider,
      uint32_t restart_id,
      bool run_all_compositor_stages_before_draw,
      const DebugRendererSettings* debug_settings,
      HintSessionFactory* hint_session_factory);

  RootCompositorFrameSinkImpl(const RootCompositorFrameSinkImpl&) = delete;
  RootCompositorFrameSinkImpl& operator=(const RootCompositorFrameSinkImpl&) =
      delete;

  ~RootCompositorFrameSinkImpl() override;

  // Returns true iff it is okay to evict the root surface immediately.
  bool WillEvictSurface(const SurfaceId& surface_id);

  const SurfaceId& CurrentSurfaceId() const;

  // mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override;
#if BUILDFLAG(IS_WIN)
  void DisableSwapUntilResize(DisableSwapUntilResizeCallback callback) override;
#endif
  void Resize(const gfx::Size& size) override;
  void SetDisplayColorMatrix(const gfx::Transform& color_matrix) override;
  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& display_color_spaces) override;
#if BUILDFLAG(IS_MAC)
  void SetVSyncDisplayID(int64_t display_id) override;
#endif
  void SetOutputIsSecure(bool secure) override;
  void SetDisplayVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval) override;
  void ForceImmediateDrawAndSwapIfPossible() override;
#if BUILDFLAG(IS_ANDROID)
  void SetVSyncPaused(bool paused) override;
  void UpdateRefreshRate(float refresh_rate) override;
  void PreserveChildSurfaceControls() override;
  void SetSwapCompletionCallbackEnabled(bool enable) override;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSupportedRefreshRates(
      const std::vector<float>& supported_refresh_rates) override;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  void AddVSyncParameterObserver(
      mojo::PendingRemote<mojom::VSyncParameterObserver> observer) override;
  void SetDelegatedInkPointRenderer(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver)
      override;
  void SetStandaloneBeginFrameObserver(
      mojo::PendingRemote<mojom::BeginFrameObserver> observer) override;
  void SetMaxVSyncAndVrr(std::optional<base::TimeDelta> max_vsync_interval,
                         display::VariableRefreshRateState vrr_state) override;

  // mojom::CompositorFrameSink:
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SetWantsAnimateOnlyBeginFrames() override;
  void SetWantsBeginFrameAcks() override;
  void SetAutoNeedsBeginFrame() override;
  void SubmitCompositorFrame(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      std::optional<HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override;
  void DidNotProduceFrame(const BeginFrameAck& begin_frame_ack) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const SharedBitmapId& id) override;
  void SubmitCompositorFrameSync(
      const LocalSurfaceId& local_surface_id,
      CompositorFrame frame,
      std::optional<HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      SubmitCompositorFrameSyncCallback callback) override;
  void InitializeCompositorFrameSinkType(
      mojom::CompositorFrameSinkType type) override;
  void BindLayerContext(mojom::PendingLayerContextPtr context) override;
#if BUILDFLAG(IS_ANDROID)
  void SetThreadIds(const std::vector<int32_t>& thread_ids) override;
#endif

#if BUILDFLAG(IS_ANDROID)
  base::ScopedClosureRunner GetCacheBackBufferCb();
#endif
  ExternalBeginFrameSource* external_begin_frame_source() {
    return external_begin_frame_source_.get();
  }

  void SetHwSupportForMultipleRefreshRates(bool support);

  void StartOverdrawTracking(int interval_length_in_seconds);
  OverdrawTracker::OverdrawTimeSeries StopOverdrawTracking();

 private:
  class StandaloneBeginFrameObserver;

  RootCompositorFrameSinkImpl(
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
      bool hw_support_for_multiple_refresh_rates);

  void UpdateFrameIntervalDeciderSettings();
  void FrameIntervalDeciderResultCallback(
      FrameIntervalDecider::Result result,
      FrameIntervalMatcherType matcher_type);

  // DisplayClient:
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              AggregatedRenderPassList* render_passes) override;
  void DisplayDidDrawAndSwap() override;
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override;
  void DisplayAddChildWindowToBrowser(gpu::SurfaceHandle child_window) override;
  void SetWideColorEnabled(bool enabled) override;
  void SetPreferredFrameInterval(base::TimeDelta interval) override;
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id,
      mojom::CompositorFrameSinkType* type) override;

  void UpdateVSyncParameters();
  BeginFrameSource* begin_frame_source();

  base::flat_set<base::TimeDelta> GetSupportedFrameIntervals();

  mojo::Remote<mojom::CompositorFrameSinkClient> compositor_frame_sink_client_;
  mojo::AssociatedReceiver<mojom::CompositorFrameSink>
      compositor_frame_sink_receiver_;
  // |display_client_| may be NullRemote on platforms that do not use it.
  [[maybe_unused]] mojo::Remote<mojom::DisplayClient> display_client_;
  mojo::AssociatedReceiver<mojom::DisplayPrivate> display_private_receiver_;

  std::unique_ptr<VSyncParameterListener> vsync_listener_;

  // Must be destroyed before |compositor_frame_sink_client_|. This must never
  // change for the lifetime of RootCompositorFrameSinkImpl.
  const std::unique_ptr<CompositorFrameSinkSupport> support_;

  // FrameIntervalDecider related members.
  // True indicates FrameIntervalDecider uses FixedIntervalSettings.
  bool interval_decider_use_fixed_intervals_ = true;
  // The current display frame interval that FrameIntervalDecider decided on.
  base::TimeDelta decided_display_interval_;

  // RootCompositorFrameSinkImpl holds a Display and a BeginFrameSource if it
  // was created with a non-null gpu::SurfaceHandle. The source can either be a
  // |synthetic_begin_frame_source_| or an |external_begin_frame_source_|.
  std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source_;
  // If non-null, |synthetic_begin_frame_source_| will not exist.
  std::unique_ptr<ExternalBeginFrameSource> external_begin_frame_source_;
  // Should be destroyed before begin frame sources since it can issue callbacks
  // to the BFS.
  std::unique_ptr<Display> display_;

  std::unique_ptr<StandaloneBeginFrameObserver>
      standalone_begin_frame_observer_;

  // |use_preferred_interval_| indicates if we should use the preferred interval
  // from FrameRateDecider to tick.
  bool use_preferred_interval_ = false;
  base::TimeTicks display_frame_timebase_;
  base::TimeDelta display_frame_interval_ = BeginFrameArgs::DefaultInterval();
  base::TimeDelta preferred_frame_interval_ =
      FrameRateDecider::UnspecifiedFrameInterval();

  // See comments on `EvictionHandler`.
  EvictionHandler eviction_handler_;

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  gfx::Size last_swap_pixel_size_;
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

#if BUILDFLAG(IS_APPLE)
  gfx::CALayerParams last_ca_layer_params_;

  // Used to force a call to OnDisplayReceivedCALayerParams() even if the params
  // did not change.
  base::TimeTicks next_forced_ca_layer_params_update_time_;
#endif

#if BUILDFLAG(IS_ANDROID)
  // Let client control whether it wants `DidCompleteSwapWithSize`.
  bool enable_swap_completion_callback_ = false;
#endif

  // Map which retains the exact supported refresh rates, keyed by their
  // interval conversion value which may be subject to precision loss.
  base::flat_map<base::TimeDelta, float> exact_supported_refresh_rates_;
  // The maximum interval that the display supports. Used for VRR (variable
  // refresh rate) or continuous range framerate selection in
  // `FrameIntervalDecider`. Absent if the display does not support those
  // features.
  std::optional<base::TimeDelta> max_vsync_interval_ = std::nullopt;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_ROOT_COMPOSITOR_FRAME_SINK_IMPL_H_
