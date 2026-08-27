// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_FOCUS_SWIPE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_FOCUS_SWIPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/event_handler.h"

class VerticalTabStripRegionView;

// Handles two-finger swipe gestures across the vertical tab strip to cycle
// between the normal (unfocused) tab strip and focused tab groups.
// Installed as a pre-target handler on VerticalTabStripRegionView.
class VerticalTabStripFocusSwipeController : public ui::EventHandler {
 public:
  static constexpr float kAxisLockThreshold = 15.0f;
  static constexpr float kSwipeThreshold = 75.0f;
  static constexpr base::TimeDelta kGestureResetTimeout =
      base::Milliseconds(300);

  explicit VerticalTabStripFocusSwipeController(
      VerticalTabStripRegionView* region_view);
  VerticalTabStripFocusSwipeController(
      const VerticalTabStripFocusSwipeController&) = delete;
  VerticalTabStripFocusSwipeController& operator=(
      const VerticalTabStripFocusSwipeController&) = delete;
  ~VerticalTabStripFocusSwipeController() override;

  // ui::EventHandler:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void OnMouseEvent(ui::MouseEvent* event) override;
#elif BUILDFLAG(IS_MAC)
  void OnScrollEvent(ui::ScrollEvent* event) override;
#endif

  bool has_triggered_in_current_gesture_for_testing() const {
    return has_triggered_in_current_gesture_;
  }

 private:
  enum class GestureState {
    kNone,
    kHorizontalSwipe,
    kVerticalPassthrough,
  };

  void ProcessSwipeOffset(float x_offset, float y_offset, ui::Event* event);
  void ResetGesture();
  void RotateTabGroupFocus(bool forward);

  const raw_ptr<VerticalTabStripRegionView> region_view_;

  GestureState gesture_state_ = GestureState::kNone;
  float cumulative_x_ = 0.0f;
  float cumulative_y_ = 0.0f;
  bool has_triggered_in_current_gesture_ = false;
  base::TimeTicks last_event_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_FOCUS_SWIPE_CONTROLLER_H_
