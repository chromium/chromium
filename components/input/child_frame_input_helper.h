// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_CHILD_FRAME_INPUT_HELPER_H_
#define COMPONENTS_INPUT_CHILD_FRAME_INPUT_HELPER_H_

#include "components/input/render_widget_host_view_input.h"

namespace input {

// Helper class to share code between RenderWidgetHostViewChildFrame (in
// Browser) and RenderInputRouterSupportChildFrame (in Viz).
class COMPONENT_EXPORT(INPUT) ChildFrameInputHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual RenderWidgetHostViewInput* GetParentViewInput() = 0;
    virtual RenderWidgetHostViewInput* GetRootViewInput() = 0;
  };

  explicit ChildFrameInputHelper(RenderWidgetHostViewInput* view,
                                 Delegate* delegate);

  ChildFrameInputHelper(const ChildFrameInputHelper&) = delete;
  ChildFrameInputHelper& operator=(const ChildFrameInputHelper&) = delete;

  virtual ~ChildFrameInputHelper();

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  void NotifyHitTestRegionUpdated(const viz::AggregatedHitTestRegion& region);

  bool ScreenRectIsUnstableFor(const blink::WebInputEvent& event);
  bool ScreenRectIsUnstableForIOv2For(const blink::WebInputEvent& event);

  gfx::PointF TransformPointToRootCoordSpaceF(const gfx::PointF& point);
  // Given a point in the current view's coordinate space, return the same
  // point transformed into the coordinate space of the top-level view's
  // coordinate space.
  gfx::PointF TransformPointToRootCoordSpace(const gfx::PointF& point);

  gfx::PointF TransformRootPointToViewCoordSpace(const gfx::PointF& point);

  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point);

  // Transform a point into the coordinate space of the root
  // RenderWidgetHostView, for the current view's coordinate space.
  // Returns false if |target_view| and |view_| do not have the same root
  // RenderWidgetHostView. RenderWidgetHostViewInput is the abstract class that
  // defines the interface for handling user input and is one to one with
  // RenderWidgetHostViewBase in the browser.
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      const viz::FrameSinkId& local_frame_sink_id,
      gfx::PointF* transformed_point);

  void TransformPointToRootSurface(gfx::PointF* point);

  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event);

  void StopFlingingIfNecessary(const blink::WebGestureEvent& event,
                               blink::mojom::InputEventResultState ack_result);

  void GestureEventAckHelper(const blink::WebGestureEvent& event,
                             blink::mojom::InputEventResultSource ack_source,
                             blink::mojom::InputEventResultState ack_result);

  void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result);

  // Pass acked touchpad pinch or double tap gesture events to the root view
  // for processing.
  void ProcessTouchpadZoomEventAckInRoot(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result);

  // A gesture scroll sequence that is not consumed by a child must be bubbled
  // to ancestors who may consume it.
  // Returns false if the scroll event could not be bubbled. The caller must
  // not attempt to bubble the rest of the scroll sequence in this case.
  // Otherwise, returns true.
  // Made virtual for test override.
  virtual bool BubbleScrollEvent(const blink::WebGestureEvent& event);

  void DidAckGestureEvent(const blink::WebGestureEvent& event,
                          blink::mojom::InputEventResultState ack_result);

  bool IsScrollSequenceBubbling() { return is_scroll_sequence_bubbling_; }

 private:
  gfx::RectF last_stable_screen_rect_;
  gfx::RectF last_stable_screen_rect_for_iov2_;
  base::TimeTicks screen_rect_stable_since_;
  base::TimeTicks screen_rect_stable_since_for_iov2_;

  // True if there is currently a scroll sequence being bubbled to our parent.
  bool is_scroll_sequence_bubbling_ = false;

  // |view_| is supposed to outlive |this|.
  raw_ptr<RenderWidgetHostViewInput> view_;

  // |delegate_| can be NULL.
  raw_ptr<Delegate> delegate_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_CHILD_FRAME_INPUT_HELPER_H_
