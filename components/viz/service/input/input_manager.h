// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/input/render_input_router.h"
#include "components/input/render_input_router.mojom.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/utils.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"
#include "components/viz/service/input/render_input_router_delegate_impl.h"
#include "components/viz/service/input/render_input_router_support_base.h"
#include "gpu/ipc/common/surface_handle.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/input/android/input_receiver_data.h"
#include "components/viz/service/input/android_state_transfer_handler.h"
#include "components/viz/service/input/fling_scheduler_android.h"
#include "components/viz/service/input/render_input_router_support_android.h"
#endif

namespace input {
class TouchEmulator;
}

namespace viz {

struct FrameSinkMetadata {
  explicit FrameSinkMetadata(
      base::UnguessableToken grouping_id,
      std::unique_ptr<RenderInputRouterSupportBase> support,
      std::unique_ptr<RenderInputRouterDelegateImpl> delegate);

  FrameSinkMetadata(const FrameSinkMetadata&) = delete;
  FrameSinkMetadata& operator=(const FrameSinkMetadata&) = delete;

  FrameSinkMetadata(FrameSinkMetadata&& other);
  FrameSinkMetadata& operator=(FrameSinkMetadata&& other);

  ~FrameSinkMetadata();

  base::UnguessableToken grouping_id;
  std::unique_ptr<RenderInputRouterSupportBase> rir_support;
  std::unique_ptr<RenderInputRouterDelegateImpl> rir_delegate;
};

class VIZ_SERVICE_EXPORT InputManager
    : public FrameSinkObserver,
      public input::RenderWidgetHostInputEventRouter::Delegate,
#if BUILDFLAG(IS_ANDROID)
      public FlingSchedulerAndroid::Delegate,
      public AndroidStateTransferHandlerClient,
#endif
      public RenderInputRouterSupportBase::Delegate,
      public RenderInputRouterDelegateImpl::Delegate,
      public input::mojom::RenderInputRouterDelegate {
 public:
  explicit InputManager(FrameSinkManagerImpl* frame_sink_manager);

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  ~InputManager() override;

  void OnCreateCompositorFrameSink(
      const FrameSinkId& frame_sink_id,
      bool is_root,
      input::mojom::RenderInputRouterConfigPtr render_input_router_config,
      bool create_input_receiver,
      gpu::SurfaceHandle surface_handle);

  // FrameSinkObserver overrides.
  void OnDestroyedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) override;
  void OnFrameSinkDeviceScaleFactorChanged(const FrameSinkId& frame_sink_id,
                                           float device_scale_factor) override;

  // RenderWidgetHostInputEventRouter::Delegate implementation.
  input::TouchEmulator* GetTouchEmulator(bool create_if_necessary) override;

  // RenderInputRouterSupportBase::Delegate implementation.
  const DisplayHitTestQueryMap& GetDisplayHitTestQuery() const override;
  float GetDeviceScaleFactorForId(const FrameSinkId& frame_sink_id) override;
  FrameSinkId GetRootCompositorFrameSinkId(
      const FrameSinkId& child_frame_sink_id) override;
  RenderInputRouterSupportBase* GetParentRenderInputRouterSupport(
      const FrameSinkId& frame_sink_id) override;
  RenderInputRouterSupportBase* GetRootRenderInputRouterSupport(
      const FrameSinkId& frame_sink_id) override;
  const CompositorFrameMetadata* GetLastActivatedFrameMetadata(
      const FrameSinkId& frame_sink_id) override;

#if BUILDFLAG(IS_ANDROID)
  // FlingSchedulerAndroid::Delegate implementation.
  BeginFrameSource* GetBeginFrameSourceForFrameSink(
      const FrameSinkId& id) override;

  // AndroidStateTransferHandlerClient implementation.
  bool TransferInputBackToBrowser() override;
#endif

  // RenderInputRouterDelegateImpl::Delegate implementation.
  std::unique_ptr<input::RenderInputRouterIterator>
  GetEmbeddedRenderInputRouters(const FrameSinkId& id) override;
  void NotifyObserversOfInputEvent(
      const FrameSinkId& frame_sink_id,
      const base::UnguessableToken& grouping_id,
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      bool dispatched_to_renderer) override;
  void NotifyObserversOfInputEventAcks(
      const FrameSinkId& frame_sink_id,
      const base::UnguessableToken& grouping_id,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      std::unique_ptr<blink::WebCoalescedInputEvent> event) override;
  void OnInvalidInputEventSource(
      const FrameSinkId& frame_sink_id,
      const base::UnguessableToken& grouping_id) override;
  std::optional<bool> IsDelegatedInkHovering(
      const FrameSinkId& frame_sink_id) override;
  GpuServiceImpl* GetGpuService() override;

  // input::mojom::RenderInputRouterDelegate implementation.
  void StateOnTouchTransfer(input::mojom::TouchTransferStatePtr state) override;
  void NotifySiteIsMobileOptimized(bool is_mobile_optimized,
                                   const FrameSinkId& frame_sink_id) override;
  void ForceEnableZoomStateChanged(
      bool force_enable_zoom,
      const std::vector<FrameSinkId>& frame_sink_ids) override;

  void SetupRenderInputRouterDelegateConnection(
      const base::UnguessableToken& grouping_id,
      mojo::PendingRemote<input::mojom::RenderInputRouterDelegateClient>
          rir_delegate_client_remote,
      mojo::PendingReceiver<input::mojom::RenderInputRouterDelegate>
          rir_delegate_receiver);

  void NotifyRendererBlockStateChanged(bool blocked,
                                       const std::vector<FrameSinkId>& rirs);

  input::RenderInputRouter* GetRenderInputRouterFromFrameSinkId(
      const FrameSinkId& id);

  bool ReturnInputBackToBrowser();

 private:
  std::unique_ptr<RenderInputRouterSupportBase> MakeRenderInputRouterSupport(
      input::RenderInputRouter* rir,
      const FrameSinkId& frame_sink_id);

  void OnRIRDelegateClientDisconnected(
      const base::UnguessableToken& grouping_id);

  void SetupRenderInputRouter(
      input::RenderInputRouter* render_input_router,
      const FrameSinkId& frame_sink_id,
      mojo::PendingRemote<blink::mojom::RenderInputRouterClient> rir_client,
      bool force_enable_zoom);

  std::unique_ptr<input::FlingSchedulerBase> MakeFlingScheduler(
      input::RenderInputRouter* rir,
      const FrameSinkId& frame_sink_id);

#if BUILDFLAG(IS_ANDROID)
  // Android input receiver is created only for the very first root compositor
  // frame sink creation notification that InputManager receives.
  // Due to an Android platform bug(b/368251173) which causes crash on calling
  // AInputReceiver_release, the input receiver is reused for any future root
  // compositors.
  void CreateOrReuseAndroidInputReceiver(
      const FrameSinkId& frame_sink_id,
      const gpu::SurfaceHandle& surface_handle);

  AndroidStateTransferHandler android_state_transfer_handler_;

  std::unique_ptr<input::InputReceiverData> receiver_data_;
#endif  // BUILDFLAG(IS_ANDROID)

  friend class MockInputManager;

  // Keeps track of InputEventRouter corresponding to FrameSinkIds using a
  // CompositorFrameSink grouping_id sent from the browser, allowing mirroring
  // 1:1 relationship in browser between WebContentsImpl and
  // RenderWidgetHostInputEventRouter to Viz.
  base::flat_map</*grouping_id=*/base::UnguessableToken,
                 scoped_refptr<input::RenderWidgetHostInputEventRouter>>
      rwhier_map_;

  // Keeps track of metadata related to FrameSinkIds which are 1:1 to
  // RenderInputRouters.
  base::flat_map<FrameSinkId, FrameSinkMetadata> frame_sink_metadata_map_;

  // RenderInputRouter is created only for non-root layer tree frame sinks, i.e.
  // the layer tree frame sinks requested by renderers.
  base::flat_map<FrameSinkId, std::unique_ptr<input::RenderInputRouter>>
      rir_map_;

  // Keeps track of RIRDelegateClient connections, which are between
  // WebContentsImpl (in the Browser) and InputManager (in Viz) using a
  // CompositorFrameSink grouping_id sent from the browser. This interface is
  // used by Viz to update browser's state of input event handling in Viz.
  base::flat_map</*grouping_id=*/base::UnguessableToken,
                 mojo::Remote<input::mojom::RenderInputRouterDelegateClient>>
      rir_delegate_remote_map_;
  mojo::ReceiverSet<input::mojom::RenderInputRouterDelegate>
      rir_delegate_receivers_;

  raw_ptr<FrameSinkManagerImpl> frame_sink_manager_;

  base::WeakPtrFactory<InputManager> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
