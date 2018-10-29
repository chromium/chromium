// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/software_output_device_client.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/latest_local_surface_id_lookup_delegate.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"
#include "ui/latency/latency_info.h"

namespace gfx {
class Size;
}

namespace viz {
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
                                   public SoftwareOutputDeviceClient {
 public:
  // The |begin_frame_source| and |scheduler| may be null (together). In that
  // case, DrawAndSwap must be called externally when needed.
  // The |current_task_runner| may be null if the Display is on a thread without
  // a MessageLoop.
  // TODO(penghuang): Remove skia_output_surface when all DirectRenderer
  // subclasses are replaced by SkiaRenderer.
  Display(SharedBitmapManager* bitmap_manager,
          const RendererSettings& settings,
          const FrameSinkId& frame_sink_id,
          std::unique_ptr<OutputSurface> output_surface,
          std::unique_ptr<DisplayScheduler> scheduler,
          scoped_refptr<base::SingleThreadTaskRunner> current_task_runner,
          SkiaOutputSurface* skia_output_surface = nullptr);

  ~Display() override;

  void Initialize(DisplayClient* client, SurfaceManager* surface_manager);

  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  // device_scale_factor is used to communicate to the external window system
  // what scale this was rendered at.
  void SetLocalSurfaceId(const LocalSurfaceId& id, float device_scale_factor);
  void SetVisible(bool visible);
  void Resize(const gfx::Size& new_size);

  // Sets the color matrix that will be used to transform the output of this
  // display. This is only supported for GPU compositing.
  void SetColorMatrix(const SkMatrix44& matrix);

  void SetColorSpace(const gfx::ColorSpace& blending_color_space,
                     const gfx::ColorSpace& device_color_space);
  void SetOutputIsSecure(bool secure);

  const SurfaceId& CurrentSurfaceId();

  // DisplaySchedulerClient implementation.
  bool DrawAndSwap() override;
  bool SurfaceHasUndrawnFrame(const SurfaceId& surface_id) const override;
  bool SurfaceDamaged(const SurfaceId& surface_id,
                      const BeginFrameAck& ack) override;
  void SurfaceDiscarded(const SurfaceId& surface_id) override;
  void DidFinishFrame(const BeginFrameAck& ack) override;

  // OutputSurfaceClient implementation.
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void DidReceiveSwapBuffersAck() override;
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;
  void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
  void DidSwapWithSize(const gfx::Size& pixel_size) override;
  void DidReceivePresentationFeedback(
      const gfx::PresentationFeedback& feedback) override;
  void DidFinishLatencyInfo(
      const std::vector<ui::LatencyInfo>& latency_info) override;

  // LatestLocalSurfaceIdLookupDelegate implementation.
  LocalSurfaceId GetSurfaceAtAggregation(
      const FrameSinkId& frame_sink_id) const override;

  // SoftwareOutputDeviceClient implementation
  void SoftwareDeviceUpdatedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;

  bool has_scheduler() const { return !!scheduler_; }
  DirectRenderer* renderer_for_testing() const { return renderer_.get(); }

  void ForceImmediateDrawAndSwapIfPossible();
  void SetNeedsOneBeginFrame();
  void RemoveOverdrawQuads(CompositorFrame* frame);

 private:
  void InitializeRenderer();
  void UpdateRootFrameMissing();

  // ContextLostObserver implementation.
  void OnContextLost() override;

  SharedBitmapManager* const bitmap_manager_;
  const RendererSettings settings_;

  DisplayClient* client_ = nullptr;
  base::ObserverList<DisplayObserver>::Unchecked observers_;
  SurfaceManager* surface_manager_ = nullptr;
  const FrameSinkId frame_sink_id_;
  SurfaceId current_surface_id_;
  gfx::Size current_surface_size_;
  float device_scale_factor_ = 1.f;
  gfx::ColorSpace blending_color_space_ = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace device_color_space_ = gfx::ColorSpace::CreateSRGB();
  bool visible_ = false;
  bool swapped_since_resize_ = false;
  bool output_is_secure_ = false;

  SkiaOutputSurface* skia_output_surface_;
  std::unique_ptr<OutputSurface> output_surface_;
  std::unique_ptr<DisplayScheduler> scheduler_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  // This may be null if the Display is on a thread without a MessageLoop.
  scoped_refptr<base::SingleThreadTaskRunner> current_task_runner_;
  std::unique_ptr<DirectRenderer> renderer_;
  SoftwareRenderer* software_renderer_ = nullptr;
  std::vector<ui::LatencyInfo> stored_latency_info_;
  std::vector<SurfaceId> surfaces_to_ack_on_next_draw_;

  base::circular_deque<
      std::pair<base::TimeTicks, std::vector<Surface::PresentedCallback>>>
      pending_presented_callbacks_;

  int64_t swapped_trace_id_ = 0;
  int64_t last_acked_trace_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_H_
