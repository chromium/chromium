// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_SESSION_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "tab_style_views.h"

class TabDragWithScrollManager;

// Interface that starts and stops a scrolling session. Current implementations
// are timer based implementations
class TabStripScrollSession {
 public:
  explicit TabStripScrollSession(TabDragWithScrollManager& drag_controller);
  TabStripScrollSession(const TabStripScrollSession&) = delete;
  TabStripScrollSession& operator=(const TabStripScrollSession&) = delete;
  virtual ~TabStripScrollSession();
  enum class TabScrollDirection {
    kNoScroll,
    kScrollTowardsLeadingTabs,
    kScrollTowardsTrailingTabs
  };
  // Calculates which direction should the scrolling occur and
  // starts the `Start()` method
  virtual void MaybeStart() = 0;
  // Stop the scroll_session
  virtual void Stop() = 0;
  // Check if the scroll session is still active
  virtual bool IsRunning() = 0;
  // Determines which direction should the scrolling happen.
  virtual TabStripScrollSession::TabScrollDirection GetTabScrollDirection() = 0;
  // The offset from the start or end of scroll view when the scrolling should
  // begin.
  int GetScrollableOffset() const;

 protected:
  // Start the scroll_session towards the direction passed
  virtual void Start(TabScrollDirection direction) = 0;
  // Direction in which the scroll is currently happening
  TabScrollDirection scroll_direction_ = TabScrollDirection::kNoScroll;
  // the controller for running operations like MoveAttached and getting
  // the attached_context.
  const raw_ref<TabDragWithScrollManager> tab_drag_with_scroll_manager_;
  // The tabdrag context on which scroll is happening
  raw_ptr<views::View> scroll_context_ = nullptr;
};

class TabStripScrollSessionWithTimer : public TabStripScrollSession {
 public:
  // enum for type of timer. constant timer scrolls at a constant velocity but
  // variable timer scrolls faster towards the ends of the visible view of
  // tab_strip
  enum class ScrollSessionTimerType { kVariableTimer, kConstantTimer };

  TabStripScrollSessionWithTimer(TabDragWithScrollManager& drag_controller,
                                 ScrollSessionTimerType timer_type);

  explicit TabStripScrollSessionWithTimer(const TabStripScrollSession&) =
      delete;
  TabStripScrollSessionWithTimer& operator=(const TabStripScrollSession&) =
      delete;
  ~TabStripScrollSessionWithTimer() override;

  void MaybeStart() override;
  void Stop() override;
  bool IsRunning() override;
  TabStripScrollSession::TabScrollDirection GetTabScrollDirection() override;

  // public method for unittest to use a mockTimer
  void SetTimerForTesting(base::RepeatingTimer* testing_timer) {
    scroll_timer_.reset(testing_timer);
  }
  // Getter to expose kScrollableOffsetFromScrollView to test class
  int GetScrollableOffsetFromScrollViewForTesting() {
    return GetScrollableOffset();
  }
  // Returns the base scroll offset which is the case with constant timer
  double CalculateBaseScrollOffset();

 private:
  void Start(TabScrollDirection direction) override;
  // Returns the offset to be scrolled per callback. For variable timer based
  // implementation the maximum speed is 3*base_speed
  int CalculateSpeed();
  // Callback invoked by the timer
  void TabScrollCallback();
  // Returns the ratio of how far the bounds of the tab_strip are the
  // dragged tabs at
  double GetRatioInScrollableRegion();
  // timer for scrolling and dragging
  std::unique_ptr<base::RepeatingTimer> scroll_timer_;
  // type of timer which can have a constant velocity or a variable velocity
  // based on how close the tabs are to the end of the visible content view.
  const ScrollSessionTimerType timer_type_ =
      ScrollSessionTimerType::kConstantTimer;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_SESSION_H_
