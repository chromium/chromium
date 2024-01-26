// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_POPUP_TRACKER_H_
#define COMPONENTS_BLOCKED_CONTENT_POPUP_TRACKER_H_

#include <optional>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/scoped_visibility_tracker.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

namespace blocked_content {

// This class tracks new popups, and is used to log metrics on the visibility
// time of the first document in the popup.
// TODO(csharrison): Consider adding more metrics like total visibility for the
// lifetime of the WebContents.
class PopupTracker : public content::WebContentsObserver,
                     public content::WebContentsUserData<PopupTracker>,
                     public subresource_filter::SubresourceFilterObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PopupSafeBrowsingStatus {
    kNoValue = 0,
    kSafe = 1,
    kUnsafe = 2,
    kMaxValue = kUnsafe,
  };

  static PopupTracker* CreateForWebContents(content::WebContents* contents,
                                            content::WebContents* opener,
                                            WindowOpenDisposition disposition);

  PopupTracker(const PopupTracker&) = delete;
  PopupTracker& operator=(const PopupTracker&) = delete;

  ~PopupTracker() override;

  void set_is_trusted(bool is_trusted) { is_trusted_ = is_trusted; }

  bool has_first_load_visible_time_for_testing() const {
    return first_load_visible_time_.has_value();
  }

 private:
  friend class content::WebContentsUserData<PopupTracker>;

  PopupTracker(content::WebContents* contents,
               content::WebContents* opener,
               WindowOpenDisposition disposition);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  // subresource_filter::SubresourceFilterObserver:
  void OnSafeBrowsingChecksComplete(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::SubresourceFilterSafeBrowsingClient::
          CheckResult& result) override;
  void OnSubresourceFilterGoingAway() override;

  base::ScopedObservation<subresource_filter::SubresourceFilterObserverManager,
                          subresource_filter::SubresourceFilterObserver>
      scoped_observation_{this};

  // Will be unset until the first navigation commits. Will be set to the total
  // time the contents was visible at commit time.
  std::optional<base::TimeDelta> first_load_visible_time_start_;
  // Will be unset until the second navigation commits. Is the total time the
  // contents is visible while the first document is loading (after commit).
  std::optional<base::TimeDelta> first_load_visible_time_;

  ui::ScopedVisibilityTracker visibility_tracker_;

  // The number of user interactions occurring in this popup tab.
  int num_interactions_ = 0;
  // The number of user interacitons in a popup tab broken down into
  // user activation and gesture scroll begin events.
  int num_activation_events_ = 0;
  int num_gesture_scroll_begin_events_ = 0;

  // Number of redirects taken by the pop-up during navigation.
  int num_redirects_ = 0;
  bool first_navigation_committed_ = false;

  // The id of the web contents that created the popup at the time of creation.
  // SourceIds are permanent so it's okay to use at any point so long as it's
  // not invalid.
  const ukm::SourceId opener_source_id_;

  bool is_trusted_ = false;

  // Whether the pop-up navigated to a site on the safe browsing list. Set when
  // the safe browsing checks complete.
  PopupSafeBrowsingStatus safe_browsing_status_ =
      PopupSafeBrowsingStatus::kNoValue;

  // The window open disposition used when creating the popup.
  const WindowOpenDisposition window_open_disposition_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_POPUP_TRACKER_H_
