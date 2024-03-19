// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
#define COMPONENTS_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
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

namespace blocked_content {
class PopupTracker;

// This class tracks WebContents for the purpose of logging metrics related to
// popup openers.
class PopupOpenerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupOpenerTabHelper> {
 public:
  PopupOpenerTabHelper(const PopupOpenerTabHelper&) = delete;
  PopupOpenerTabHelper& operator=(const PopupOpenerTabHelper&) = delete;

  ~PopupOpenerTabHelper() override;

  void OnOpenedPopup(PopupTracker* popup_tracker);

  bool has_opened_popup_since_last_user_gesture() const {
    return has_opened_popup_since_last_user_gesture_;
  }

 private:
  friend class content::WebContentsUserData<PopupOpenerTabHelper>;

  // |tick_clock| overrides the internal time for testing. This doesn't take
  // ownership of |tick_clock| or |settings_map|, and they both must outlive the
  // PopupOpenerTabHelper instance.
  PopupOpenerTabHelper(content::WebContents* web_contents,
                       const base::TickClock* tick_clock,
                       HostContentSettingsMap* settings_map);

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  // Logs user popup content settings if the last committed URL is valid and
  // we have not recorded the settings for the opener id of the helper's
  // web contents at the time the function is called.
  void MaybeLogPagePopupContentSettings();

  // The clock which is used by the visibility trackers.
  raw_ptr<const base::TickClock> tick_clock_;

  // Keeps track of the total foreground time for this tab.
  std::unique_ptr<ui::ScopedVisibilityTracker> visibility_tracker_;

  bool has_opened_popup_since_last_user_gesture_ = false;

  // The last source id used for logging Popup_Page.
  ukm::SourceId last_opener_source_id_ = ukm::kInvalidSourceId;

  // The settings map for the web contents this object is associated with.
  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
