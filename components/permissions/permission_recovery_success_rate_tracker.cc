// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_recovery_success_rate_tracker.h"

#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

PermissionRecoverySuccessRateTracker::PermissionRecoverySuccessRateTracker(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PermissionRecoverySuccessRateTracker>(
          *web_contents) {
  DCHECK(web_contents);
}

PermissionRecoverySuccessRateTracker::~PermissionRecoverySuccessRateTracker() {
  DCHECK(reallowed_permissions_.empty());
}

void PermissionRecoverySuccessRateTracker::PermissionStatusChanged(
    ContentSettingsType permission,
    ContentSetting setting,
    bool show_infobar) {
  // If permission is not allowed, it is not actionable for origins.
  if (setting != ContentSetting::CONTENT_SETTING_ALLOW) {
    return;
  }

  reallowed_permissions_[permission] = show_infobar;
}

void PermissionRecoverySuccessRateTracker::ClearTrackingMap() {
  for (const auto& [permission, show_infobar] : reallowed_permissions_) {
    Track(permission, /*is_used=*/false, show_infobar);
  }

  reallowed_permissions_.clear();
}

void PermissionRecoverySuccessRateTracker::TrackUsage(
    ContentSettingsType permission) {
  if (reallowed_permissions_.find(permission) != reallowed_permissions_.end()) {
    Track(permission, /*is_used=*/true, reallowed_permissions_[permission]);
    reallowed_permissions_.erase(permission);
  }
}

void PermissionRecoverySuccessRateTracker::Track(ContentSettingsType permission,
                                                 bool is_used,
                                                 bool show_infobar) {
  PermissionUmaUtil::RecordPermissionRecoverySuccessRate(
      permission, is_used, show_infobar, page_reload_);
}

void PermissionRecoverySuccessRateTracker::WebContentsDestroyed() {
  ClearTrackingMap();
}

void PermissionRecoverySuccessRateTracker::PrimaryPageChanged(
    content::Page& page) {
  if (origin_ != page.GetMainDocument().GetLastCommittedOrigin()) {
    origin_ = page.GetMainDocument().GetLastCommittedOrigin();
    // Clear tracking map only for cross-origin navigation.
    ClearTrackingMap();
    page_reload_ = false;
  } else {
    page_reload_ = true;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionRecoverySuccessRateTracker);

}  // namespace permissions
