// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_TAB_TRACKER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_TAB_TRACKER_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace app_list {

// CrOSActionRecorderTabTracker observes WebContents behavior for recording tab
// navigation, reactivation and opening of urls.
class CrOSActionRecorderTabTracker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CrOSActionRecorderTabTracker> {
 public:
  ~CrOSActionRecorderTabTracker() override = default;

 private:
  friend class content::WebContentsUserData<CrOSActionRecorderTabTracker>;

  // The only constructor is private because it will only be called by the
  // WebContentsUserData.
  explicit CrOSActionRecorderTabTracker(content::WebContents* web_contents);

  // Default pages are all skipped including "about::blank" and
  // "chrome://newtab/".
  bool ShouldSkip();

  // For content::WebContentsObserver:
  // For tracking tab navigations.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  // For tracking tab reactivations.
  void OnVisibilityChanged(content::Visibility visibility) override;
  // For tracking a new url is opened from current tab.
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CROS_ACTION_HISTORY_CROS_ACTION_RECORDER_TAB_TRACKER_H_
