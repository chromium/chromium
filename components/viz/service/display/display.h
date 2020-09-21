// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/frame_rate_decider.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/software_output_device_client.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/latest_local_surface_id_lookup_delegate.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"
#include "ui/latency/latency_info.h"

namespace gfx {
class Size;
}

namespace gpu {
class ScopedAllowScheduleGpuTask;
}

namespace viz {
class AggregatedFrame;
class DelegatedInkPointRendererBase;
class DirectRenderer;
class DisplayClient;
class DisplayResourceProvider;
class OutputSurface;
class RendererSettings;
class SharedBitmapManager;
class SkiaOutputSurface;
class SoftwareRenderer;

class VIZ_SERVICE_EXPORT DisplayObserver {
 public:
  virtual ~DisplayObserver() {}

  virtual void OnDisplayDidFinishFrame(const BeginFrameAck& ack) = 0;
  virtual void OnDisplayDestroyed() = 0;
};

// A Display produces a surface that can be used to draw to a physical display
// (OutputSurface). The client is responsible for creating and sizing the
// surface IDs used to draw into the display and deciding when to draw.
class VIZ_SERVICE_EXPORT Display : public DisplaySchedulerClient,
                                   public OutputSurfaceClient,
                                   public ContextLostObserver,
                                   public LatestLocalSurfaceIdLookupDelegate,
                                   public SoftwareOutputDeviceClient,
                                   public FrameRateDecider::Client {
 public:
  // The |begin_frame_source| and |scheduler| may be null (together). In that
  // case, DrawAndSwap must be called externally when needed.
  // The |current_task_runner| may be null if the Display is on a thread without
  // a MessageLoop.
  // TODO(penghuang): Remove skia_output_surface when all DirectRenderer
  // subclasses are replaced by SkiaRenderer.
  Display(SharedBitmapManager* bitmap_manager,
          const RendererSettings& settings,
          const DebugRendererSettings* debug_settings,
          const FrameSinkId& frame_sink_id,
          std::unique_ptr<OutputSurface> output_surface,
          std::unique_ptr<OverlayProcessorInterface> overlay_processor,
          std::unique_ptr<DisplaySchedulerBase> scheduler,
          scoped_refptr<base::SingleThreadTaskRunner> current_task_runner);

  ~Display() override;

  static constexpr base::TimeDelta kDrawToSwapMin =
      base::TimeDelta::FromMicroseconds(5);
  static constexpr base::TimeDelta kDrawToSwapMax =
      base::TimeDelta::FromMilliseconds(50);
  static constexpr uint32_t kDrawToSwapUsBuckets = 50;

  // TODO(cblume, crbug.com/900973): |enable_shared_images| is a temporary
  // solution that unblocks us until SharedImages are threadsafe in WebView.
#if defined(ANDROID)
  static constexpr bool kEnableSharedImages = false;
#else
  static constexpr bool kEnableSharedImages = true;
#endif
  void Initialize(DisplayClient* client,
                  SurfaceManager* surface_manager,
                  bool enable_shared_images = kEnableSharedImages,
                  bool hw_support_for_multiple_refresh_rates = false,
                  size_t num_of_frames_to_toggle_interval = 60);

  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  // device_scale_factor is used to communicate to the external window system
  // what scale this was rendered at.
  void SetLocalSurfaceId(const LocalSurfaceId& id, float device_scale_factor);
  void SetVisible(bool visible);
  void Resize(const gfx::Size& new_size);

  // This disallows resource provider to access GPU thread to unlock resources
  // outside of Initialize, DrawAndSwap and dtor.
  void DisableGPUAccessByDefault();

  // Stop drawing until Resize() is called with a new size. If the display
  // hasn't drawn a frame at the current size *and* it's possible to immediately
  // draw then this will run DrawAndSwap() first.
  //
  // |no_pending_swaps_callback| will be run there are no more swaps pending and
  // may be run immediately.
  void DisableSwapUntilResize(base::OnceClosure no_pending_swaps_callback);

  // Sets the color matrix that will be used to transform the output of this
  // display. This is only supported for GPU compositing.
  void SetColorMatrix(const SkMatrix44& matrix);

  void SetDisplayColorSpaces(
      const gfx::DisplayColorSpaces& display_color_spaces);
  void SetOutputIsSecure(bool secure);

  const SurfaceId& CurrentSurfaceId();

  // DisplaySchedulerClient implementation.
  bool DrawAndSwap(base::TimeTicks expected_display_time) override;
  void DidFinishFrame(const BeginFrameAck& ack) override;

  // OutputSurfaceClient implementation.
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void DidReceiveSwapBuffersAck(const gfx::SwapTimings& timings) override;
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;
  void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
  void DidSwapWithSize(const gfx::Size& pixel_size) override;
  void DidReceivePresentationFeedback(
      const gfx::PresentationFeedback& feedback) override;
  void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) override;

  // LatestLocalSurfaceIdLookupDelegate implementation.
  LocalSurfaceId GetSurfaceAtAggregation(
      const FrameSinkId& frame_sink_id) const override;

  // SoftwareOutputDeviceClient implementation
  void SoftwareDeviceUpdatedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;

  // FrameRateDecider::Client implementation
  void SetPreferredFrameInterval(base::TimeDelta interval) override;
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id,
      mojom::CompositorFrameSinkType* type) override;

  bool has_scheduler() const { return !!scheduler_; }
  DirectRenderer* renderer_for_testing() const { return renderer_.get(); }

  void ForceImmediateDrawAndSwapIfPossible();
  void SetNeedsOneBeginFrame();
  void RemoveOverdrawQuads(AggregatedFrame* frame);

  void SetSupportedFrameIntervals(std::vector<base::TimeDelta> intervals);

  base::ScopedClosureRunner GetCacheBackBufferCb();

  bool IsRootFrameMissing() const;
  bool HasPendingSurfaces(const BeginFrameArgs& args) const;

  // Return the delegated ink point renderer from |renderer_|, creating it if
  // one doesn't exist. Should only be used when the delegated ink trails web
  // API has been used.
  DelegatedInkPointRendererBase* GetDelegatedInkPointRenderer();

 private:
  friend class DisplayTest;
  // PresentationGroupTiming stores rendering pipeline stage timings associated
  // with a call to Display::DrawAndSwap along with a list of
  // Surface::PresentationHelper's for each aggregated Surface that will be
  // presented.
  class PresentationGroupTiming {
   public:
    PresentationGroupTiming();
    PresentationGroupTiming(PresentationGroupTiming&& other);
    ~PresentationGroupTiming();

    void AddPresentationHelper(
        std::unique_ptr<Surface::PresentationHelper> helper);
    void OnDraw(base::TimeTicks draw_start_timestamp);
    void OnSwap(gfx::SwapTimings timings);
    bool HasSwapped() const { return !swap_timings_.is_null(); }
    void OnPresent(const gfx::PresentationFeedback& feedback);

    base::TimeTicks draw_start_timestamp() const {
      return draw_start_timestamp_;
    }

   private:
    base::TimeTicks draw_start_timestamp_;
    gfx::SwapTimings swap_timings_;
    std::vector<std::unique_ptr<Surface::PresentationHelper>>
        presentation_helpers_;

    DISALLOW_COPY_AND_ASSIGN(PresentationGroupTiming);
  };

  // TODO(cblume, crbug.com/900973): |enable_shared_images| is a temporary
  // solution that unblocks us until SharedImages are threadsafe in WebView.
  void InitializeRenderer(bool enable_shared_images = true);

  // ContextLostObserver implementation.
  void OnContextLost() override;

  SharedBitmapManager* const bitmap_manager_;
  const RendererSettings settings_;

  // Points to the viz-global singleton.
  const DebugRendererSettings* const debug_settings_;

  DisplayClient* client_ = nullptr;
  base::ObserverList<DisplayObserver>::Unchecked observers_;
  SurfaceManager* surface_manager_ = nullptr;
  const FrameSinkId frame_sink_id_;
  SurfaceId current_surface_id_;
  gfx::Size current_surface_size_;
  float device_scale_factor_ = 1.f;
  gfx::DisplayColorSpaces display_color_spaces_;
  bool visible_ = false;
  bool swapped_since_resize_ = false;
  bool output_is_secure_ = false;

#if DCHECK_IS_ON()
  std::unique_ptr<gpu::ScopedAllowScheduleGpuTask>
      allow_schedule_gpu_task_during_destruction_;
#endif
  std::unique_ptr<OutputSurface> output_surface_;
  SkiaOutputSurface* const skia_output_surface_;
  std::unique_ptr<DisplayDamageTracker> damage_tracker_;
  std::unique_ptr<DisplaySchedulerBase> scheduler_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  bool last_wide_color_enabled_ = false;
  std::unique_ptr<FrameRateDecider> frame_rate_decider_;
  // This may be null if the Display is on a thread without a MessageLoop.
  scoped_refptr<base::SingleThreadTaskRunner> current_task_runner_;
  std::unique_ptr<DirectRenderer> renderer_;
  SoftwareRenderer* software_renderer_ = nullptr;
  // Currently, this OverlayProcessor takes raw pointer to memory tracker, which
  // is owned by the OutputSurface. This OverlayProcessor also takes resource
  // locks which contains raw pointers to DisplayResourceProvider. Make sure
  // both the OutputSurface and the DisplayResourceProvider outlive the
  // Overlay Processor.
  std::unique_ptr<OverlayProcessorInterface> overlay_processor_;
  std::vector<ui::LatencyInfo> stored_latency_info_;
  std::vector<gfx::Rect> cached_visible_region_;

  // |pending_presentation_group_timings_| stores a
  // Display::PresentationGroupTiming for each group currently waiting for
  // Display::DidReceivePresentationFeedack()
  base::circular_deque<Display::PresentationGroupTiming>
      pending_presentation_group_timings_;

  bool disable_swap_until_resize_ = true;

  // Callback that will be run after all pending swaps have acked.
  base::OnceClosure no_pending_swaps_callback_;

  int64_t swapped_trace_id_ = 0;
  int64_t last_swap_ack_trace_id_ = 0;
  int64_t last_presented_trace_id_ = 0;
  int pending_swaps_ = 0;

  // The height of the top-controls in the previously drawn frame.
  float last_top_controls_visible_height_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_
