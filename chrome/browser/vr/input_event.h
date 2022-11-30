// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_INPUT_EVENT_H_
#define CHROME_BROWSER_VR_INPUT_EVENT_H_

#include "base/time/time.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

class VR_BASE_EXPORT InputEvent {
 public:
  enum Type {
    kTypeUndefined = -1,

    kHoverEnter,
    kTypeFirst = kHoverEnter,
    kHoverLeave,
    kHoverMove,
    kButtonDown,
    kButtonUp,
    kMove,
    kFlingCancel,
    kScrollBegin,
    kScrollTypeFirst = kScrollBegin,
    kScrollUpdate,
    kScrollEnd,
    kScrollTypeLast = kScrollEnd,

    kMenuButtonClicked,
    kMenuButtonTypeFirst = kMenuButtonClicked,
    kMenuButtonLongPressStart,
    kMenuButtonLongPressEnd,
    kMenuButtonTypeLast = kMenuButtonLongPressEnd,

    kNumVrInputEventTypes
  };

  explicit InputEvent(Type type);
  virtual ~InputEvent();

  Type type() const { return type_; }

  base::TimeTicks time_stamp() const { return time_stamp_; }

  void set_time_stamp(base::TimeTicks time_stamp) { time_stamp_ = time_stamp; }

  gfx::PointF position_in_widget() const { return position_in_widget_; }

  void set_position_in_widget(const gfx::PointF& position) {
    position_in_widget_ = position;
  }

  void SetPositionInWidget(float x, float y) {
    position_in_widget_ = gfx::PointF(x, y);
  }

  static bool IsScrollEventType(InputEvent::Type type) {
    return kScrollTypeFirst <= type && type <= kScrollTypeLast;
  }

  static bool IsMenuButtonEventType(InputEvent::Type type) {
    return kMenuButtonTypeFirst <= type && type <= kMenuButtonTypeLast;
  }

  struct {
    float delta_x;
    float delta_y;
  } scroll_data;

 private:
  Type type_;
  base::TimeTicks time_stamp_;
  gfx::PointF position_in_widget_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_INPUT_EVENT_H_
