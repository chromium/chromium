// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_INPUT_ROUTER_H_
#define COMPONENTS_INPUT_RENDER_INPUT_ROUTER_H_

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/input/fling_scheduler_base.h"
#include "components/input/input_disposition_handler.h"
#include "components/input/input_router_impl.h"
#include "components/input/render_input_router_delegate.h"
#include "components/input/render_input_router_iterator.h"
#include "components/input/render_input_router_latency_tracker.h"
#include "components/viz/common/resources/peak_gpu_memory_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"

namespace content {
class MockRenderInputRouter;
} // namespace content

namespace input {

class RenderInputRouterClient;

// RenderInputRouter is currently owned by RenderWidgetHostImpl and is being
// used for forwarding input events. It maintains mojo connections
// with renderers to do so. In future, this class will be used to handle acks
// from renderers and with Input on Viz project
// (https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE),
// this will also be used to handle input events on VizCompositorThread (GPU
// process).
class COMPONENT_EXPORT(INPUT) RenderInputRouter
    : public InputRouterClient,
      public InputDispositionHandler {
 public:
  RenderInputRouter(const RenderInputRouter&) = delete;
  RenderInputRouter& operator=(const RenderInputRouter&) = delete;

  ~RenderInputRouter() override;

  RenderInputRouter(RenderInputRouterClient* host,
                    std::unique_ptr<FlingSchedulerBase> fling_scheduler,
                    RenderInputRouterDelegate* delegate,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void SetupInputRouter(float device_scale_factor);
  void SetFlingScheduler(std::unique_ptr<FlingSchedulerBase> fling_scheduler);

  void BindRenderInputRouterInterfaces(
      mojo::PendingRemote<blink::mojom::RenderInputRouterClient> remote);

  void RendererWidgetCreated(bool for_frame_widget, bool is_in_viz);

  InputRouter* input_router() { return input_router_.get(); }
  RenderInputRouterDelegate* delegate() { return delegate_; }

  void SetForceEnableZoom(bool);
  void SetDeviceScaleFactor(float device_scale_factor);

  void ProgressFlingIfNeeded(base::TimeTicks current_time);
  void StopFling();

  bool IsAnyScrollGestureInProgress() const;

  blink::mojom::FrameWidgetInputHandler* GetFrameWidgetInputHandler();

  void SetView(RenderWidgetHostViewInput* view);

  void SetBeginFrameSourceForFlingScheduler(
      viz::BeginFrameSource* begin_frame_source);

  // InputRouterClient overrides.
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override;
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::optional<std::vector<gfx::Rect>>& character_bounds) override;
  void OnImeCancelComposition() override;
  StylusInterface* GetStylusInterface() override;
  void OnStartStylusWriting() override;
  bool IsWheelScrollInProgress() override;
  bool IsAutoscrollInProgress() override;
  void SetMouseCapture(bool capture) override;
  void SetAutoscrollSelectionActiveInMainFrame(
      bool autoscroll_selection) override;
  void RequestMouseLock(
      bool from_user_gesture,
      bool unadjusted_movement,
      InputRouterImpl::RequestMouseLockCallback response) override;
  gfx::Size GetRootWidgetViewportSize() override;
  void OnUnconfirmedTapConvertedToTap() override;

  // InputRouterImplClient overrides.
  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount() override;
  void DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource ack_source) override;
  void DidOverscroll(blink::mojom::DidOverscrollParamsPtr params) override;
  void DidStartScrollingViewport() override;
  void OnSetCompositorAllowedTouchAction(cc::TouchAction) override {}
  void OnInvalidInputEventSource() override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency_info) override;
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency_info) override;
  DispatchToRendererCallback GetDispatchToRendererCallback() override;

  // InputDispositionHandler
  void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       blink::mojom::InputEventResultSource ack_source,
                       blink::mojom::InputEventResultState ack_result) override;
  void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override;

  // Dispatch input events with latency information
  void DispatchInputEventWithLatencyInfo(
      const blink::WebInputEvent& event,
      ui::LatencyInfo* latency,
      ui::EventLatencyMetadata* event_latency_metadata);

  virtual void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& latency);  // Virtual for testing

  void ForwardGestureEvent(const blink::WebGestureEvent& gesture_event);

  // Retrieve an iterator over any RenderInputRouters that are
  // immediately embedded within this one. This does not return
  // RenderInputRouters that are embedded indirectly (i.e. nested within
  // embedded hosts).
  std::unique_ptr<RenderInputRouterIterator> GetEmbeddedRenderInputRouters();

  // |point| specifies the location in RenderWidget's coordinates for invoking
  // the context menu.
  void ShowContextMenuAtPoint(const gfx::Point& point,
                              const ui::mojom::MenuSourceType source_type);

  void SendGestureEventWithLatencyInfo(
      const GestureEventWithLatencyInfo& gesture_with_latency,
      DispatchToRendererCallback& dispatch_callback);

  // Signals if this host has forwarded a GestureScrollBegin without yet having
  // forwarded a matching GestureScrollEnd/GestureFlingStart.
  bool is_in_touchscreen_gesture_scroll() const {
    return is_in_gesture_scroll_[static_cast<int>(
        blink::WebGestureDevice::kTouchscreen)];
  }

  void DidStopFlinging();

  RenderInputRouterLatencyTracker* GetLatencyTracker() {
    return latency_tracker_.get();
  }

  void set_is_currently_scrolling_viewport(
      bool is_currently_scrolling_viewport) {
    is_currently_scrolling_viewport_ = is_currently_scrolling_viewport;
  }

  bool is_currently_scrolling_viewport() {
    return is_currently_scrolling_viewport_;
  }

  void FlushForTesting() {
    if (widget_input_handler_) {
      return widget_input_handler_.FlushForTesting();
    }
  }

  bool GetForceEnableZoom() { return force_enable_zoom_; }
  void ResetFrameWidgetInputInterfaces();
  void ResetWidgetInputInterfaces();

  mojo::Remote<viz::mojom::InputTargetClient>& input_target_client() {
    return input_target_client_;
  }

  size_t in_flight_event_count() const { return in_flight_event_count_; }

  void SetInputTargetClientForTesting(
      mojo::Remote<viz::mojom::InputTargetClient> input_target_client);
  void SetWidgetInputHandlerForTesting(
      mojo::Remote<blink::mojom::WidgetInputHandler> widget_input_handler);
  FlingSchedulerBase* GetFlingSchedulerForTesting() {
    return fling_scheduler_.get();
  }

  void RenderProcessBlockedStateChanged(bool blocked);

  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopInputEventAckTimeout();
  void RestartInputEventAckTimeoutIfNecessary();

  void StartInputEventAckTimeoutForTesting() { StartInputEventAckTimeout(); }

 private:
  friend content::MockRenderInputRouter;

  // Called when an input event gets finally dispatched to renderer or ended up
  // getting filtered.
  void OnInputDispatchedToRendererResult(const blink::WebInputEvent& event,
                                         DispatchToRendererResult result);

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartInputEventAckTimeout();

  // Called by |input_event_ack_timeout_| when an input event timed out without
  // getting an ack from the renderer.
  void OnInputEventAckTimeout();

  bool is_currently_scrolling_viewport_ = false;

  // We access this value quite a lot, so we cache switches::kDisableHangMonitor
  // here.
  const bool should_disable_hang_monitor_;

  // This value denotes the number of input events yet to be acknowledged
  // by the renderer.
  int in_flight_event_count_ = 0;

  bool is_blocked_ = false;

  base::OneShotTimer input_event_ack_timeout_;

  // This value indicates how long to wait before we consider a renderer hung.
  base::TimeDelta hung_renderer_delay_;

  // Must be declared before `input_router_`. The latter is constructed by
  // borrowing a reference to this object, so it must be deleted first.
  std::unique_ptr<FlingSchedulerBase> fling_scheduler_;
  std::unique_ptr<InputRouter> input_router_;

  // TODO(wjmaclean) Remove the code for supporting resending gesture events
  // when WebView transitions to OOPIF and BrowserPlugin is removed.
  // http://crbug.com/533069
  std::array<bool,
             base::checked_cast<size_t>(blink::WebGestureDevice::kMaxValue) + 1>
      is_in_gesture_scroll_ = {{false}};
  bool is_in_touchpad_gesture_fling_ = false;
  bool gsb_filtered_for_paint_holding_ = false;
  std::unique_ptr<RenderInputRouterLatencyTracker> latency_tracker_;

  std::unique_ptr<viz::PeakGpuMemoryTracker> scroll_peak_gpu_mem_tracker_;

  raw_ptr<RenderInputRouterClient> render_input_router_client_;

  raw_ptr<RenderInputRouterDelegate> delegate_;

  mojo::Remote<viz::mojom::InputTargetClient> input_target_client_;
  mojo::Remote<blink::mojom::RenderInputRouterClient> client_remote_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mojo::Remote<blink::mojom::WidgetInputHandler> widget_input_handler_;
  mojo::AssociatedRemote<blink::mojom::FrameWidgetInputHandler>
      frame_widget_input_handler_;

  bool force_enable_zoom_ = false;

  base::WeakPtr<RenderWidgetHostViewInput> view_input_;

  base::WeakPtrFactory<RenderInputRouter> weak_factory_{this};
};

}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_INPUT_ROUTER_H_
