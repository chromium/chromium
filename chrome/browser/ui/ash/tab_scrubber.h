// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TAB_SCRUBBER_H_
#define CHROME_BROWSER_UI_ASH_TAB_SCRUBBER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "ui/events/event_handler.h"

class Browser;
class BrowserView;
class ImmersiveRevealedLock;
class Tab;
class TabStrip;

namespace gfx {
class Point;
}

// Class to enable quick tab switching via horizontal 4 finger swipes.
class TabScrubber : public ui::EventHandler,
                    public BrowserListObserver,
                    public TabStripObserver {
 public:
  enum Direction { LEFT, RIGHT };

  // Returns a the single instance of a TabScrubber.
  static TabScrubber* GetInstance();

  // Returns the starting position (in tabstrip coordinates) of a swipe starting
  // in the tab at |index| and traveling in |direction|.
  static gfx::Point GetStartPoint(TabStrip* tab_strip,
                                  int index,
                                  TabScrubber::Direction direction);

  int highlighted_tab() const { return highlighted_tab_; }
  bool IsActivationPending();

 private:
  friend class TabScrubberTest;

  TabScrubber();
  ~TabScrubber() override;

  // ui::EventHandler overrides:
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // BrowserListObserver overrides:
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripObserver overrides.
  void OnTabAdded(int index) override;
  void OnTabMoved(int from_index, int to_index) override;
  void OnTabRemoved(int index) override;

  Browser* GetActiveBrowser();

  void BeginScrub(BrowserView* browser_view, float x_offset);
  void FinishScrub(bool activate);

  void ScheduleFinishScrubIfNeeded();

  // Updates the direction and the starting point of the swipe.
  void ScrubDirectionChanged(Direction direction);

  // Updates the X co-ordinate of the swipe taking into account RTL layouts if
  // any.
  void UpdateSwipeX(float x_offset);

  void UpdateHighlightedTab(Tab* new_tab, int new_index);

  // Are we currently scrubbing?.
  bool scrubbing_ = false;
  // The last browser we used for scrubbing, NULL if |scrubbing_| is false and
  // there is no pending work.
  Browser* browser_ = nullptr;
  // The TabStrip of the active browser we're scrubbing.
  TabStrip* tab_strip_ = nullptr;
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

  DISALLOW_COPY_AND_ASSIGN(TabScrubber);
};

#endif  // CHROME_BROWSER_UI_ASH_TAB_SCRUBBER_H_
