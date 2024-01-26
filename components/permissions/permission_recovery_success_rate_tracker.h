// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_RECOVERY_SUCCESS_RATE_TRACKER_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_RECOVERY_SUCCESS_RATE_TRACKER_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace permissions {

// This class track permissions that were reallowed via PageInfo. In addition,
// it records if the "Reload this page" infobar was shown or if a page was
// reloaded. The reload may be initiated via the infobar or any other UI
// elements. The recording happens when a main frame is going to be destroyed.
// That may happen while a tab is being closed or while cross-origin navigation.
//
// The recording happens instantly if permission is used by an origin as the
// goal is to verify permission usage after it was reallowed.
class PermissionRecoverySuccessRateTracker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          PermissionRecoverySuccessRateTracker> {
 public:
  explicit PermissionRecoverySuccessRateTracker(
      content::WebContents* web_contents);

  PermissionRecoverySuccessRateTracker(
      const PermissionRecoverySuccessRateTracker&) = delete;
  PermissionRecoverySuccessRateTracker& operator=(
      const PermissionRecoverySuccessRateTracker&) = delete;

  ~PermissionRecoverySuccessRateTracker() override;

  // Adds `permission` into the `reallowed_permissions_` map. `permission` will
  // be recorded before a main frame is destroyed or after `permission` is used
  // by an origin.
  void PermissionStatusChanged(ContentSettingsType permission,
                               ContentSetting setting,
                               bool show_infobar);

  // `permission` has been used by an origin, record usage and remove from
  // `reallowed_permissions_`.
  void TrackUsage(ContentSettingsType permission);

 private:
  friend class content::WebContentsUserData<
      PermissionRecoverySuccessRateTracker>;

  void ClearTrackingMap();

  void Track(ContentSettingsType permission, bool is_used, bool show_infobar);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  std::optional<url::Origin> origin_;

  bool page_reload_ = false;

  std::map<ContentSettingsType, bool> reallowed_permissions_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_RECOVERY_SUCCESS_RATE_TRACKER_H_
