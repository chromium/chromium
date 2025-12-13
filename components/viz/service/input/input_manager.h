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
#include "mojo/public/cpp/bindings/associated_receiver_set.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/cancelable_callback.h"
#include "components/input/android/input_receiver_data.h"
#include "components/viz/service/input/android_state_transfer_handler.h"
#include "components/viz/service/input/render_input_router_support_android.h"
#include "components/viz/service/input/viz_touch_state_handler.h"
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
  bool is_mobile_optimized = false;
};

class VIZ_SERVICE_EXPORT InputManager
    : public FrameSinkObserver,
      public input::RenderWidgetHostInputEventRouter::Delegate,
#if BUILDFLAG(IS_ANDROID)
      public AndroidStateTransferHandlerClient,
#endif
      public RenderInputRouterSupportBase::Delegate,
      public RenderInputRouterDelegateImpl::Delegate,
      public input::mojom::RenderInputRouterDelegate,
      public mojom::RendererInputRouterDelegateRegistry {
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
  void OnRegisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void OnUnregisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) override;
  void OnFrameSinkDeviceScaleFactorChanged(const FrameSinkId& frame_sink_id,
                                           float device_scale_factor) override;
  void OnFrameSinkMobileOptimizedChanged(const FrameSinkId& frame_sink_id,
                                         bool is_mobile_optimized) override;

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
  // AndroidStateTransferHandlerClient implementation.
  bool TransferInputBackToBrowser() override;
#endif

  // RenderInputRouterDelegateImpl::Delegate implementation.
  std::unique_ptr<input::RenderInputRouterIterator>
  GetEmbeddedRenderInputRouters(const FrameSinkId& id) override;
  input::mojom::RenderInputRouterDelegateClient* GetRIRDelegateClientRemote(
      const FrameSinkId& frame_sink_id) override;
  std::optional<bool> IsDelegatedInkHovering(
      const FrameSinkId& frame_sink_id) override;
  GpuServiceImpl* GetGpuService() override;

  // input::mojom::RenderInputRouterDelegate implementation.
  void StateOnTouchTransfer(input::mojom::TouchTransferStatePtr state) override;
  void ForceEnableZoomStateChanged(bool force_enable_zoom,
                                   const FrameSinkId& frame_sink_id) override;
  void StopFlingingOnViz(const FrameSinkId& frame_sink_id) override;
  void RestartInputEventAckTimeoutIfNecessary(
      const FrameSinkId& frame_sink_id) override;
  void NotifyVisibilityChanged(const FrameSinkId& frame_sink_id,
                               bool is_hidden) override;
  void ResetGestureDetection(
      const FrameSinkId& root_widget_frame_sink_id) override;

  // mojom::RendererInputRouterDelegateRegistry implementation.
  void SetupRenderInputRouterDelegateConnection(
      const FrameSinkId& frame_sink_id,
      mojo::PendingAssociatedRemote<
          input::mojom::RenderInputRouterDelegateClient> rir_delegate_remote,
      mojo::PendingAssociatedReceiver<input::mojom::RenderInputRouterDelegate>
          rir_delegate_receiver) override;

  void SetupRendererInputRouterDelegateRegistry(
      mojo::PendingReceiver<mojom::RendererInputRouterDelegateRegistry>
          receiver);

  void NotifyRendererBlockStateChanged(bool blocked,
                                       const std::vector<FrameSinkId>& rirs);

  input::RenderInputRouter* GetRenderInputRouterFromFrameSinkId(
      const FrameSinkId& id);

  bool ReturnInputBackToBrowser();

  void SetBeginFrameSource(const FrameSinkId& frame_sink_id,
                           BeginFrameSource* begin_frame_source);

  base::ReadOnlySharedMemoryRegion DuplicateVizTouchStateRegion() const;

 private:
  // Recreates RenderInputRouterSupport in cases where Viz receives a
  // |CreateCompositorFrameSink| call before |CreateRootCompositorFrameSink|
  // call which can cause incorrect construction of type
  // RenderInputRouterSupportAndroid as RenderInputRouterSupportChildFrame.
  void MaybeRecreateRootRenderInputRouterSupports(
      const FrameSinkId& root_frame_sink_id);

  void RecreateRenderInputRouterSupport(const FrameSinkId& child_frame_sink_id,
                                        FrameSinkMetadata& frame_sink_metadata);

  std::unique_ptr<RenderInputRouterSupportBase> MakeRenderInputRouterSupport(
      input::RenderInputRouter* rir,
      const FrameSinkId& frame_sink_id);

  void OnRIRDelegateClientDisconnected(const FrameSinkId& frame_sink_id);

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
  VizTouchStateHandler viz_touch_state_handler_;

  // There's a platform bug on Android 16 which keeps the input surface control
  // lingering around unless the app explicitly does a `System.gc()` call to
  // clean it up : https://crbug.com/436302937#comment5.
  // Since the the input surface control doesn't have any associate buffers
  // `System.gc()` is called on every 100th destruction.
  int pending_surface_controls_ = 0;
  std::unique_ptr<input::InputReceiverData> receiver_data_;

  // Allow cancelling the creation task, since it's possible for
  // DestroyCompositorFrameSink call to come before the callback is ran.
  base::flat_map<FrameSinkId, std::unique_ptr<base::CancelableOnceClosure>>
      pending_create_input_receiver_callback_;
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

  mojo::Receiver<mojom::RendererInputRouterDelegateRegistry> registry_receiver_{
      this};

  // Keeps track of RIRDelegateClient connections, which are between
  // RenderWidgetHosts (in the Browser) and InputManager (in Viz) using the
  // FrameSinkId associated with the RenderWidgetHost sent from the browser.
  // This interface is used by Viz to update browser's state of input event
  // handling in Viz.
  base::flat_map<
      FrameSinkId,
      mojo::AssociatedRemote<input::mojom::RenderInputRouterDelegateClient>>
      rir_delegate_remote_map_;
  mojo::AssociatedReceiverSet<input::mojom::RenderInputRouterDelegate>
      rir_delegate_receivers_;

  raw_ptr<FrameSinkManagerImpl> frame_sink_manager_;

  base::WeakPtrFactory<InputManager> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_INPUT_MANAGER_H_
