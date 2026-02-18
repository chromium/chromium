// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELL_DELEGATE_TAB_SCRUBBER_H_
#define CHROME_BROWSER_UI_ASH_SHELL_DELEGATE_TAB_SCRUBBER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "ui/events/event_handler.h"

class BrowserView;
class ImmersiveRevealedLock;
class Tab;
class TabStrip;

namespace gfx {
class Point;
}

namespace ash {

// Class to enable quick tab switching via horizontal X finger swipes (see
// kFingerCount definition).
class TabScrubber : public ui::EventHandler, public TabStripObserver {
 public:
  static constexpr int kFingerCount = 3;

  enum Direction { LEFT, RIGHT };

  TabScrubber(const TabScrubber&) = delete;
  TabScrubber& operator=(const TabScrubber&) = delete;

  explicit TabScrubber(BrowserView* browser_view);
  ~TabScrubber() override;

  // Returns the starting position (in tabstrip coordinates) of a swipe starting
  // in the tab at |index| and traveling in |direction|.
  static gfx::Point GetStartPoint(TabStrip* tab_strip,
                                  int index,
                                  TabScrubber::Direction direction);

  int highlighted_tab() const { return highlighted_tab_; }
  bool IsActivationPending();

  static void SetEnabled(bool enabled);
  static bool GetEnabledForTesting();

  // Synthesize an ScrollEvent given a x offset (in DIPs).
  // `is_fling_scroll_event` is set to true when the scroll event should be
  // fling scroll event.
  void SynthesizedScrollEvent(float x_offset, bool is_fling_scroll_event);

  // Returns true if it does finish the ongoing scrubbing.
  bool FinishScrub(bool activate);

 private:
  friend class TabScrubberTest;

  // ui::EventHandler overrides:
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // TabStripObserver overrides.
  void OnTabAdded(int index) override;
  void OnTabMoved(int from_index, int to_index) override;
  void OnTabRemoved(int index) override;

  void BeginScrub(float x_offset);

  void ScheduleFinishScrubIfNeeded();

  // Updates the direction and the starting point of the swipe.
  void ScrubDirectionChanged(Direction direction);

  // Updates the X co-ordinate of the swipe taking into account RTL layouts if
  // any.
  void UpdateSwipeX(float x_offset);

  void UpdateHighlightedTab(Tab* new_tab, int new_index);

  const raw_ptr<BrowserView> browser_view_;
  // Are we currently scrubbing?.
  bool scrubbing_ = false;
  // The TabStrip of the active browser we're scrubbing.
  raw_ptr<TabStrip> tab_strip_ = nullptr;
  // The current accumulated x and y positions of a swipe, in the coordinates
  // of the TabStrip of |browser_|.
  float swipe_x_ = -1;
  int swipe_y_ = -1;
  // The direction the current swipe is headed.
  Direction swipe_direction_ = LEFT;
  // The index of the tab that is currently highlighted.
  int highlighted_tab_ = -1;
  // Timer to control a delayed activation of the |highlighted_tab_|.
  base::RetainingOneShotTimer activate_timer_;
  // True if the default activation delay should be used with |activate_timer_|.
  // A value of false means the |activate_timer_| gets a really long delay.
  bool use_default_activation_delay_ = true;
  // Forces the tabs to be revealed if we are in immersive fullscreen.
  std::unique_ptr<ImmersiveRevealedLock> immersive_reveal_lock_;
  // The time at which scrubbing started. Needed for UMA reporting of scrubbing
  // duration.
  base::TimeTicks scrubbing_start_time_;
  // If |enabled_|, tab scrubber takes events and determines whether tabs should
  // scrub. If not |enabled_|, tab scrubber ignores events. Should be disabled
  // when clashing interactions can occur, like window cycle list scrolling
  // gesture.
  static bool enabled_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHELL_DELEGATE_TAB_SCRUBBER_H_
