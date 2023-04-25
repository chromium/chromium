// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_opener_tab_helper.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/time/tick_clock.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/scoped_visibility_tracker.h"

namespace blocked_content {

PopupOpenerTabHelper::~PopupOpenerTabHelper() = default;

void PopupOpenerTabHelper::OnOpenedPopup(PopupTracker* popup_tracker) {
  has_opened_popup_since_last_user_gesture_ = true;
  MaybeLogPagePopupContentSettings();
}

PopupOpenerTabHelper::PopupOpenerTabHelper(content::WebContents* web_contents,
                                           const base::TickClock* tick_clock,
                                           HostContentSettingsMap* settings_map)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PopupOpenerTabHelper>(*web_contents),
      tick_clock_(tick_clock),
      settings_map_(settings_map) {
  visibility_tracker_ = std::make_unique<ui::ScopedVisibilityTracker>(
      tick_clock_,
      web_contents->GetVisibility() != content::Visibility::HIDDEN);
}

void PopupOpenerTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  // TODO(csharrison): Consider handling OCCLUDED tabs the same way as HIDDEN
  // tabs.
  if (visibility == content::Visibility::HIDDEN)
    visibility_tracker_->OnHidden();
  else
    visibility_tracker_->OnShown();
}

void PopupOpenerTabHelper::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  has_opened_popup_since_last_user_gesture_ = false;
}

void PopupOpenerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Treat browser-initiated navigations as user interactions.
  // Note that |HasUserGesture| does not capture browser-initiated navigations.
  // The negation of |IsRendererInitiated| tells us whether the navigation is
  // browser-generated.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      (navigation_handle->HasUserGesture() ||
       !navigation_handle->IsRendererInitiated())) {
    has_opened_popup_since_last_user_gesture_ = false;
  }
}

void PopupOpenerTabHelper::MaybeLogPagePopupContentSettings() {
  // If the user has opened a popup, record the page popup settings ukm.
  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.is_valid())
    return;

  const ukm::SourceId source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  // Do not record duplicate Popup.Page events for popups opened in succession
  // from the same opener.
  if (source_id != last_opener_source_id_) {
    bool user_allows_popups =
        settings_map_->GetContentSetting(
            url, url, ContentSettingsType::POPUPS) == CONTENT_SETTING_ALLOW;
    ukm::builders::Popup_Page(source_id)
        .SetAllowed(user_allows_popups)
        .Record(ukm::UkmRecorder::Get());
    last_opener_source_id_ = source_id;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PopupOpenerTabHelper);

}  // namespace blocked_content
