// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_INPUT_HELPER_H_
#define COMPONENTS_INPUT_ANDROID_INPUT_HELPER_H_

#include "components/input/render_widget_host_view_input.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"

namespace input {

// Helper class to share code between RenderWidgetHostViewAndroid (in Browser)
// and RenderInputRouterSupportAndroid (in Viz).
class COMPONENT_EXPORT(INPUT) AndroidInputHelper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SendGestureEvent(const blink::WebGestureEvent& event) = 0;
    virtual ui::FilteredGestureProvider& GetGestureProvider() = 0;
  };

  explicit AndroidInputHelper(RenderWidgetHostViewInput* view,
                              Delegate* delegate);

  AndroidInputHelper(const AndroidInputHelper&) = delete;
  AndroidInputHelper& operator=(const AndroidInputHelper&) = delete;

  ~AndroidInputHelper();

  void RouteOrForwardTouchEvent(blink::WebTouchEvent& web_event);
  void RouteOrForwardGestureEvent(const blink::WebGestureEvent& event);

  bool ShouldRouteEvents() const;

  void ResetGestureDetection();

  void OnGestureEvent(const ui::GestureEventData& gesture);
  bool RequiresDoubleTapGestureEvents() const;

  void ProcessAckedTouchEvent(const input::TouchEventWithLatencyInfo& touch,
                              blink::mojom::InputEventResultState ack_result);
  bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      input::RenderWidgetHostViewInput* target_view,
      gfx::PointF* transformed_point);

  void RecordToolTypeForActionDown(const ui::MotionEventAndroid& event);
  void ComputeEventLatencyOSTouchHistograms(
      const ui::MotionEvent& event,
      const base::TimeTicks& processing_time);

 private:
  // |view_| is supposed to outlive |this|.
  raw_ref<RenderWidgetHostViewInput> view_;
  // |delegate_| is supposed to outlive |this|.
  raw_ref<Delegate> delegate_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_INPUT_HELPER_H_
