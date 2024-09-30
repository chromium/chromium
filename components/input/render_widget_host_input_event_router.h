// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
#define COMPONENTS_INPUT_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/input/event_with_latency_info.h"
#include "components/input/render_widget_host_view_input_observer.h"
#include "components/input/render_widget_targeter.h"
#include "components/input/touch_emulator_client.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/common/hit_test/hit_test_region_observer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/mojom/delegated_ink_point_renderer.mojom.h"

namespace blink {
class WebGestureEvent;
class WebInputEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebPointerProperties;
class WebTouchEvent;
}

namespace gfx {
class Point;
class PointF;
}

namespace content {
class RenderWidgetHostInputEventRouterTest;
FORWARD_DECLARE_TEST(
      BrowserSideFlingBrowserTest,
      DISABLED_InertialGSUBubblingStopsWhenParentCannotScroll);
FORWARD_DECLARE_TEST(
      WebContentsImplBrowserTest,
      MouseUpInOOPIframeShouldCancelMainFrameAutoscrollSelection);
FORWARD_DECLARE_TEST(SitePerProcessHitTestBrowserTest,
                           CacheCoordinateTransformUponMouseDown);
FORWARD_DECLARE_TEST(SitePerProcessHitTestBrowserTest,
                           HitTestStaleDataDeletedView);
FORWARD_DECLARE_TEST(SitePerProcessHitTestBrowserTest,
                           InputEventRouterGestureTargetMapTest);
FORWARD_DECLARE_TEST(SitePerProcessHitTestBrowserTest,
                           InputEventRouterGesturePreventDefaultTargetMapTest);
FORWARD_DECLARE_TEST(SitePerProcessHitTestBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
FORWARD_DECLARE_TEST(SitePerProcessHitTestBrowserTest,
                           TouchpadPinchOverOOPIF);
FORWARD_DECLARE_TEST(SitePerProcessMouseWheelHitTestBrowserTest,
                           InputEventRouterWheelTargetTest);
FORWARD_DECLARE_TEST(SitePerProcessMacBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
FORWARD_DECLARE_TEST(SitePerProcessDelegatedInkBrowserTest,
                           MetadataAndPointGoThroughOOPIF);
}  // namespace content

namespace ui {
class LatencyInfo;
}

namespace viz {
class HitTestDataProvider;
}

namespace input {

class RenderWidgetHostViewInput;
class RenderWidgetTargeter;
class TouchEmulator;
class TouchEventAckQueue;

// Helper method also used from hit_test_debug_key_event_observer.cc
viz::HitTestQuery* GetHitTestQuery(viz::HitTestDataProvider* provider,
                                   const viz::FrameSinkId& frame_sink_id);

// Class owned by WebContentsImpl uniquely for the purpose of directing input
// events to the correct RenderWidgetHost on pages with multiple
// RenderWidgetHosts in the browser process. With InputVizard
// (https://docs.google.com/document/d/1mcydbkgFCO_TT9NuFE962L8PLJWT2XOfXUAPO88VuKE),
// InputManager uses reference counting to track this class's usage and is
// responsible for handling its lifecycle on VizCompositor thread. It maintains
// a mapping of RenderWidgetHostViews to Surface IDs that they own. When an
// input event requires routing based on window coordinates, this class requests
// a Surface hit test from the provided |root_view| and forwards the event to
// the owning RWHV of the returned Surface ID.
class COMPONENT_EXPORT(INPUT) RenderWidgetHostInputEventRouter final
    : public RenderWidgetHostViewInputObserver,
      public RenderWidgetTargeter::Delegate,
      public TouchEmulatorClient,
      public viz::HitTestRegionObserver,
      public base::RefCounted<RenderWidgetHostInputEventRouter> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual TouchEmulator* GetTouchEmulator(bool create_if_necessary) = 0;
  };

  explicit RenderWidgetHostInputEventRouter(viz::HitTestDataProvider* provider,
                                            Delegate* delegate);

  RenderWidgetHostInputEventRouter(const RenderWidgetHostInputEventRouter&) =
      delete;
  RenderWidgetHostInputEventRouter& operator=(
      const RenderWidgetHostInputEventRouter&) = delete;

  void OnRenderWidgetHostViewInputDestroyed(
      RenderWidgetHostViewInput* view) override;

  void RouteMouseEvent(RenderWidgetHostViewInput* root_view,
                       const blink::WebMouseEvent* event,
                       const ui::LatencyInfo& latency);
  void RouteMouseWheelEvent(RenderWidgetHostViewInput* root_view,
                            blink::WebMouseWheelEvent* event,
                            const ui::LatencyInfo& latency);
  void RouteGestureEvent(RenderWidgetHostViewInput* root_view,
                         const blink::WebGestureEvent* event,
                         const ui::LatencyInfo& latency);
  void OnHandledTouchStartOrFirstTouchMove(uint32_t unique_touch_event_id);
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& event,
                              blink::mojom::InputEventResultState ack_result,
                              RenderWidgetHostViewInput* view);
  void RouteTouchEvent(RenderWidgetHostViewInput* root_view,
                       blink::WebTouchEvent* event,
                       const ui::LatencyInfo& latency);

  // |event| is in root coordinates.
  // Returns false if the router is unable to bubble the scroll event. The
  // caller must not attempt to bubble the rest of the scroll sequence in this
  // case. Otherwise, returns true.
  [[nodiscard]] bool BubbleScrollEvent(
      RenderWidgetHostViewInput* target_view,
      RenderWidgetHostViewInput* resending_view,
      const blink::WebGestureEvent& event);
  void WillDetachChildView(
      const RenderWidgetHostViewInput* detaching_view);

  void AddFrameSinkIdOwner(const viz::FrameSinkId& id,
                           RenderWidgetHostViewInput* owner);
  void RemoveFrameSinkIdOwner(const viz::FrameSinkId& id);

  // Returns the existing TouchEmulator if |create_if_necessary| is false, else
  // creates a touch emulator.
  TouchEmulator* GetTouchEmulator(bool create_if_necessary);

  float last_device_scale_factor() { return last_device_scale_factor_; }

  // Returns the RenderWidgetHostViewInput inside the |root_view| at |point|
  // where |point| is with respect to |root_view|'s coordinates. If a RWHI is
  // found, the value of |transformed_point| is the coordinate of the point with
  // respect to the RWHI's coordinates. If |root_view| is nullptr, this method
  // will return nullptr and will not modify |transformed_point|.
  RenderWidgetHostViewInput* GetRenderWidgetHostViewInputAtPoint(
      RenderWidgetHostViewInput* root_view,
      const gfx::PointF& point,
      gfx::PointF* transformed_point);

  // Finds the RenderWidgetHostImpl inside the |root_view| at |point| where
  // |point| is with respect to |root_view|'s coordinates. If a RWHI is found,
  // it is passed along with the coordinate of the point with
  // respect to the RWHI's coordinates to the callback function. If
  // |root_view| is nullptr or RWHI is not found, the callback is called with
  // nullptr and no location.
  void GetRenderWidgetHostAtPointAsynchronously(
      RenderWidgetHostViewInput* root_view,
      const gfx::PointF& point,
      RenderWidgetTargeter::RenderWidgetHostAtPointCallback callback);

  // RenderWidgetTargeter::Delegate:
  RenderWidgetHostViewInput* FindViewFromFrameSinkId(
      const viz::FrameSinkId& frame_sink_id) const override;
  bool ShouldContinueHitTesting(
      RenderWidgetHostViewInput* target_view) const override;

  // Allows a target to claim or release capture of mouse events.
  void SetMouseCaptureTarget(RenderWidgetHostViewInput* target,
                             bool captures_dragging);

  // Toggle if mouse up event should be dispatched to root RenderWidgetHostView
  // in addition to the target RenderWidgethostView.
  void RootViewReceivesMouseUpIfNecessary(bool root_view_receives_mouse_up);

  std::vector<RenderWidgetHostViewInput*>
  GetRenderWidgetHostViewInputsForTests() const;
  RenderWidgetTargeter* GetRenderWidgetTargeterForTests();

  // Tells the fling controller of the last_fling_start_target_ to stop
  // flinging.
  void StopFling();

  // Returns true if |view| is currently registered in the router's owners map.
  // Returns false if |view| is null.
  bool IsViewInMap(const RenderWidgetHostViewInput* view) const;
  bool ViewMapIsEmpty() const;

  // TouchEmulatorClient:
  void ForwardEmulatedGestureEvent(
      const blink::WebGestureEvent& event) override;
  void ForwardEmulatedTouchEvent(
      const blink::WebTouchEvent& event,
      RenderWidgetHostViewInput* target) override;
  void SetCursor(const ui::Cursor& cursor) override;
  void ShowContextMenuAtPoint(
      const gfx::Point& point,
      const ui::MenuSourceType source_type,
      RenderWidgetHostViewInput* target) override;

  // HitTestRegionObserver
  void OnAggregatedHitTestRegionListUpdated(
      const viz::FrameSinkId& frame_sink_id,
      const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) override;

  bool HasEventsPendingDispatch() const;

  size_t TouchEventAckQueueLengthForTesting() const;
  size_t RegisteredViewCountForTesting() const;

  void set_route_to_root_for_devtools(bool route) {
    route_to_root_for_devtools_ = route;
  }

  void SetAutoScrollInProgress(bool is_autoscroll_in_progress);

  RenderWidgetHostViewInput* GetLastMouseMoveTargetForTest();
  RenderWidgetHostViewInput* GetLastMouseMoveRootViewForTest();

 protected:
  friend class base::RefCounted<RenderWidgetHostInputEventRouter>;
  ~RenderWidgetHostInputEventRouter() final;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      content::BrowserSideFlingBrowserTest,
      DISABLED_InertialGSUBubblingStopsWhenParentCannotScroll);
  FRIEND_TEST_ALL_PREFIXES(
      content::WebContentsImplBrowserTest,
      MouseUpInOOPIframeShouldCancelMainFrameAutoscrollSelection);

  using FrameSinkIdOwnerMap =
      std::unordered_map<viz::FrameSinkId,
                         base::WeakPtr<RenderWidgetHostViewInput>,
                         viz::FrameSinkIdHash>;
  using TargetMap =
      std::map<uint32_t, base::WeakPtr<RenderWidgetHostViewInput>>;

  void ClearAllObserverRegistrations();
  RenderWidgetTargetResult FindViewAtLocation(
      RenderWidgetHostViewInput* root_view,
      const gfx::PointF& point,
      viz::EventSource source,
      gfx::PointF* transformed_point) const;

  void RouteTouchscreenGestureEvent(RenderWidgetHostViewInput* root_view,
                                    const blink::WebGestureEvent* event,
                                    const ui::LatencyInfo& latency);

  RenderWidgetTargetResult FindTouchpadGestureEventTarget(
      RenderWidgetHostViewInput* root_view,
      const blink::WebGestureEvent& event) const;
  void RouteTouchpadGestureEvent(RenderWidgetHostViewInput* root_view,
                                 const blink::WebGestureEvent* event,
                                 const ui::LatencyInfo& latency);
  void DispatchTouchpadGestureEvent(
      RenderWidgetHostViewInput* root_view,
      RenderWidgetHostViewInput* target,
      const blink::WebGestureEvent& touchpad_gesture_event,
      const ui::LatencyInfo& latency,
      const std::optional<gfx::PointF>& target_location);

  // MouseMove/Enter/Leave events might need to be processed by multiple frames
  // in different processes for MouseEnter and MouseLeave event handlers to
  // properly fire. This method determines which RenderWidgetHostViews other
  // than the actual target require notification, and sends the appropriate
  // events to them. |event| should be in |root_view|'s coordinate space.
  // |include_target_view| indicates whether a MouseEnter should also be sent
  // to |target|, which is typically not needed if this is invoked while a
  // MouseMove already being sent there.
  void SendMouseEnterOrLeaveEvents(
      const blink::WebMouseEvent& event,
      RenderWidgetHostViewInput* target,
      RenderWidgetHostViewInput* root_view,
      blink::WebInputEvent::Modifiers extra_modifiers =
          blink::WebInputEvent::Modifiers::kNoModifiers,
      bool include_target_view = false);

  void CancelScrollBubbling();

  // Cancels scroll bubbling if it is unsafe to send a gesture event sequence
  // to |target| considering the views involved in an ongoing scroll.
  void CancelScrollBubblingIfConflicting(
      const RenderWidgetHostViewInput* target);

  // Wraps a touchscreen GesturePinchBegin in a GestureScrollBegin.
  void SendGestureScrollBegin(RenderWidgetHostViewInput* view,
                              const blink::WebGestureEvent& event);
  // Used to end a scroll sequence during scroll bubbling or as part of a
  // wrapped pinch gesture.
  void SendGestureScrollEnd(RenderWidgetHostViewInput* view,
                            const blink::WebGestureEvent& event);
  // Used when scroll bubbling is canceled to indicate to |view| that it should
  // consider the scroll sequence to have ended.
  void SendGestureScrollEnd(RenderWidgetHostViewInput* view,
                            blink::WebGestureDevice source_device);

  // Helper functions to implement RenderWidgetTargeter::Delegate functions.
  RenderWidgetTargetResult FindMouseEventTarget(
      RenderWidgetHostViewInput* root_view,
      const blink::WebMouseEvent& event) const;
  RenderWidgetTargetResult FindMouseWheelEventTarget(
      RenderWidgetHostViewInput* root_view,
      const blink::WebMouseWheelEvent& event) const;
  // Returns target for first TouchStart in a sequence, or a null target
  // otherwise.
  RenderWidgetTargetResult FindTouchEventTarget(
      RenderWidgetHostViewInput* root_view,
      const blink::WebTouchEvent& event);
  RenderWidgetTargetResult FindTouchscreenGestureEventTarget(
      RenderWidgetHostViewInput* root_view,
      const blink::WebGestureEvent& gesture_event);

  // |mouse_event| is in the coord-space of |root_view|.
  void DispatchMouseEvent(RenderWidgetHostViewInput* root_view,
                          RenderWidgetHostViewInput* target,
                          const blink::WebMouseEvent& mouse_event,
                          const ui::LatencyInfo& latency,
                          const std::optional<gfx::PointF>& target_location);
  // |mouse_wheel_event| is in the coord-space of |root_view|.
  void DispatchMouseWheelEvent(
      RenderWidgetHostViewInput* root_view,
      RenderWidgetHostViewInput* target,
      const blink::WebMouseWheelEvent& mouse_wheel_event,
      const ui::LatencyInfo& latency,
      const std::optional<gfx::PointF>& target_location);
  // Assumes |touch_event| has coordinates in the root view's coordinate space.
  void DispatchTouchEvent(RenderWidgetHostViewInput* root_view,
                          RenderWidgetHostViewInput* target,
                          const blink::WebTouchEvent& touch_event,
                          const ui::LatencyInfo& latency,
                          const std::optional<gfx::PointF>& target_location,
                          bool is_emulated);
  // Assumes |gesture_event| has coordinates in root view's coordinate space.
  void DispatchTouchscreenGestureEvent(
      RenderWidgetHostViewInput* root_view,
      RenderWidgetHostViewInput* target,
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency,
      const std::optional<gfx::PointF>& target_location);

  // TODO(crbug.com/41380487): Remove once this issue no longer occurs.
  void ReportBubblingScrollToSameView(
      const blink::WebGestureEvent& event,
      const RenderWidgetHostViewInput* view);

  // RenderWidgetTargeter::Delegate:
  RenderWidgetTargetResult FindTargetSynchronouslyAtPoint(
      RenderWidgetHostViewInput* root_view,
      const gfx::PointF& location) override;

  RenderWidgetTargetResult FindTargetSynchronously(
      RenderWidgetHostViewInput* root_view,
      const blink::WebInputEvent& event) override;
  void DispatchEventToTarget(
      RenderWidgetHostViewInput* root_view,
      RenderWidgetHostViewInput* target,
      blink::WebInputEvent* event,
      const ui::LatencyInfo& latency,
      const std::optional<gfx::PointF>& target_location) override;
  // Notify whether the events in the queue are being flushed due to touch ack
  // timeout, or the flushing has completed.
  void SetEventsBeingFlushed(bool events_being_flushed) override;

  bool forced_last_fling_start_target_to_stop_flinging_for_test() const {
    return forced_last_fling_start_target_to_stop_flinging_for_test_;
  }

  void SetTouchscreenGestureTarget(RenderWidgetHostViewInput* target,
                                   bool moved_recently,
                                   bool moved_recently_for_iov2);
  void ClearTouchscreenGestureTarget();

  void ForwardDelegatedInkPoint(
      RenderWidgetHostViewInput* target_view,
      RenderWidgetHostViewInput* root_view,
      const blink::WebInputEvent& input_event,
      const blink::WebPointerProperties& pointer_properties,
      bool hovering);

  FrameSinkIdOwnerMap owner_map_;
  TargetMap touchscreen_gesture_target_map_;
  raw_ptr<RenderWidgetHostViewInput> touch_target_ = nullptr;
  base::WeakPtr<RenderWidgetHostViewInput> touchscreen_gesture_target_;
  bool touchscreen_gesture_target_moved_recently_ = false;
  bool touchscreen_gesture_target_moved_recently_for_iov2_ = false;
  raw_ptr<RenderWidgetHostViewInput> touchpad_gesture_target_ = nullptr;
  raw_ptr<RenderWidgetHostViewInput> bubbling_gesture_scroll_target_ = nullptr;
  raw_ptr<RenderWidgetHostViewInput> bubbling_gesture_scroll_origin_ = nullptr;
  // Used to target wheel events for the duration of a scroll.
  raw_ptr<RenderWidgetHostViewInput> wheel_target_ = nullptr;
  // Maintains the same target between mouse down and mouse up.
  raw_ptr<RenderWidgetHostViewInput> mouse_capture_target_ = nullptr;
  // There is no mouse capture set if a mouse down event dispatches to main
  // frame. The subsequent mouse events might not be delivered to the main frame
  // if mouse is moved over to an OOP iframe. There is caches mouse state in
  // main frame such as autoscroll selection. In this case we should dispatch an
  // additional mouse up event to the main frame to clear any cached mouse
  // state. This flag indicates the state is active and mouse up event will be
  // dispatched to main frame in addition to the target frame.
  bool root_view_receive_additional_mouse_up_ = false;

  // Tracked for the purpose of generating MouseEnter and MouseLeave events.
  raw_ptr<RenderWidgetHostViewInput> last_mouse_move_target_;
  raw_ptr<RenderWidgetHostViewInput> last_mouse_move_root_view_;

  // Tracked for the purpose of targeting subsequent fling cancel events.
  raw_ptr<RenderWidgetHostViewInput> last_fling_start_target_ = nullptr;

  // True when the router calls |last_fling_start_target_->StopFling()|.
  bool forced_last_fling_start_target_to_stop_flinging_for_test_ = false;

  // Tracked for the purpose of providing a root_view when dispatching emulated
  // touch/gesture events.
  raw_ptr<RenderWidgetHostViewInput> last_emulated_event_root_view_;

  // Used to send a GSE with proper source device to terminate scroll bubbling
  // whenever needed.
  blink::WebGestureDevice bubbling_gesture_scroll_source_device_;

  float last_device_scale_factor_;

  int active_touches_;

  // Route all input events into the root view while devtools is showing a full
  // page overlay.
  bool route_to_root_for_devtools_ = false;

  // Touchscreen gesture pinch events must be routed to the main frame. This
  // keeps track of ongoing scroll and pinch gestures so we know when to divert
  // gesture events to the main frame and whether additional scroll begin/end
  // events are needed to wrap the pinch.
  class TouchscreenPinchState {
   public:
    TouchscreenPinchState();

    bool IsInPinch() const;
    bool NeedsWrappingScrollSequence() const;

    void DidStartBubblingToRoot();
    void DidStopBubblingToRoot();
    void DidStartPinchInRoot();
    void DidStartPinchInChild();
    void DidStopPinch();

   private:
    enum class PinchState {
      NONE,
      // We have touchscreen scroll bubbling to the root before a gesture pinch
      // starts.
      EXISTING_BUBBLING_TO_ROOT,

      // We are in a pinch gesture and the root is already the touchscreen
      // gesture target.
      PINCH_WITH_ROOT_GESTURE_TARGET,

      // We are in a pinch gesture that is happening while we're also bubbling
      // scroll to the root.
      PINCH_WHILE_BUBBLING_TO_ROOT,

      // We are in a pinch gesture that is happening while the child is the
      // gesture target and it has not bubbled scroll to the root.
      PINCH_DURING_CHILD_GESTURE
    };
    PinchState state_;
  };
  TouchscreenPinchState touchscreen_pinch_state_;

  // This is expected to outlive RenderWidgetHostInputEventRouter object.
  const raw_ptr<viz::HitTestDataProvider> hit_test_provider_ = nullptr;

  std::unique_ptr<RenderWidgetTargeter> event_targeter_;
  bool events_being_flushed_ = false;

  raw_ptr<Delegate> delegate_;
  std::unique_ptr<TouchEventAckQueue> touch_event_ack_queue_;

  // The coordinates that are determined by the renderer process on MouseDown
  // are cached then used by the browser process on the following MouseUp. This
  // is a temporary fix of https://crbug.com/934434 to eliminate the mismatch
  // between the two coordinate transforms.
  mutable gfx::PointF mouse_down_pre_transformed_coordinate_;
  mutable gfx::PointF mouse_down_post_transformed_coordinate_;
  raw_ptr<RenderWidgetHostViewInput> last_mouse_down_target_ = nullptr;

  // Used to know if we have already told viz to reset prediction because the
  // final point of the delegated ink trail has been sent. True when prediction
  // has already been reset for the most recent trail, false otherwise. This
  // flag helps make sure that we don't send more IPCs than necessary to viz to
  // reset prediction. Sending extra IPCs wouldn't impact correctness, but can
  // impact performance due to the IPC overhead.
  bool ended_delegated_ink_trail_ = false;

  base::WeakPtrFactory<RenderWidgetHostInputEventRouter> weak_ptr_factory_{
      this};
  friend class content::RenderWidgetHostInputEventRouterTest;
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessHitTestBrowserTest,
                           CacheCoordinateTransformUponMouseDown);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessHitTestBrowserTest,
                           HitTestStaleDataDeletedView);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessHitTestBrowserTest,
                           InputEventRouterGestureTargetMapTest);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessHitTestBrowserTest,
                           InputEventRouterGesturePreventDefaultTargetMapTest);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessHitTestBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessHitTestBrowserTest,
                           TouchpadPinchOverOOPIF);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessMouseWheelHitTestBrowserTest,
                           InputEventRouterWheelTargetTest);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessMacBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
  FRIEND_TEST_ALL_PREFIXES(content::SitePerProcessDelegatedInkBrowserTest,
                           MetadataAndPointGoThroughOOPIF);
};

}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
