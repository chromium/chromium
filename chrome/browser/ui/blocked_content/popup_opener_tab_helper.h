// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

namespace ui {
class ScopedVisibilityTracker;
}

class PopupTracker;

// This class tracks WebContents for the purpose of logging metrics related to
// popup openers.
class PopupOpenerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupOpenerTabHelper> {
 public:
  // |tick_clock| overrides the internal time for testing. This doesn't take the
  // ownership of the clock. |tick_clock| must outlive the PopupOpenerTabHelper
  // instance.
  static void CreateForWebContents(content::WebContents* contents,
                                   const base::TickClock* tick_clock);
  ~PopupOpenerTabHelper() override;

  void OnOpenedPopup(PopupTracker* popup_tracker);
  void OnDidTabUnder();

  bool has_opened_popup_since_last_user_gesture() const {
    return has_opened_popup_since_last_user_gesture_;
  }

  bool did_tab_under() const {
    return visible_time_before_tab_under_.has_value();
  }

 private:
  friend class content::WebContentsUserData<PopupOpenerTabHelper>;

  PopupOpenerTabHelper(content::WebContents* web_contents,
                       const base::TickClock* tick_clock);

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

  // Visible time for this tab until a tab-under is detected. At which point it
  // gets the visible time from the |visibility_tracker_|. Will be unset until a
  // tab-under is detected.
  base::Optional<base::TimeDelta> visible_time_before_tab_under_;

  // The clock which is used by the visibility trackers.
  const base::TickClock* tick_clock_;

  // Keeps track of the total foreground time for this tab.
  std::unique_ptr<ui::ScopedVisibilityTracker> visibility_tracker_;

  // Measures the time this WebContents opened a popup.
  base::TimeTicks last_popup_open_time_;

  bool has_opened_popup_since_last_user_gesture_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PopupOpenerTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
