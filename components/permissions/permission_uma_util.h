// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_result.h"
#include "components/permissions/permission_util.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class GURL;

namespace permissions {
enum class PermissionRequestGestureType;
class PermissionRequest;

// Any new values should be inserted immediately prior to NUM.
enum class PermissionSourceUI {
  // Permission prompt.
  PROMPT = 0,

  // Origin info bubble.
  // https://www.chromium.org/Home/chromium-security/enamel/goals-for-the-origin-info-bubble
  OIB = 1,

  // chrome://settings/content/siteDetails?site=[SITE]
  // chrome://settings/content/[PERMISSION TYPE]
  SITE_SETTINGS = 2,

  // Page action bubble.
  PAGE_ACTION = 3,

  // Permission settings from Android.
  // Currently this value is only used when revoking notification permission in
  // Android O+ system channel settings.
  ANDROID_SETTINGS = 4,

  // Permission settings as part of the event's UI.
  // Currently this value is only used when revoking notification permission
  // through the notification UI.
  INLINE_SETTINGS = 5,

  // Permission settings changes as part of the abusive origins revocation.
  AUTO_REVOCATION = 6,

  // Always keep this at the end.
  NUM,
};

// Any new values should be inserted immediately prior to NUM.
enum class PermissionEmbargoStatus {
  NOT_EMBARGOED = 0,
  // Removed: PERMISSIONS_BLACKLISTING = 1,
  REPEATED_DISMISSALS = 2,
  REPEATED_IGNORES = 3,

  // Keep this at the end.
  NUM,
};

// The kind of permission prompt UX used to surface a permission request.
// Enum used in UKMs and UMAs, do not re-order or change values. Deprecated
// items should only be commented out. New items should be added at the end,
// and the "PermissionPromptDisposition" histogram suffix needs to be updated to
// match (tools/metrics/histograms/histograms_xml/histogram_suffixes_list.xml).
enum class PermissionPromptDisposition {
  // Not all permission actions will have an associated permission prompt (e.g.
  // changing permission via the settings page).
  NOT_APPLICABLE = 0,

  // Only used on desktop, a bubble under the site settings padlock.
  ANCHORED_BUBBLE = 1,

  // Only used on desktop, a static indicator on the right-hand side of the
  // location bar.
  LOCATION_BAR_RIGHT_STATIC_ICON = 2,

  // Only used on desktop, an animated indicator on the right-hand side of the
  // location bar.
  LOCATION_BAR_RIGHT_ANIMATED_ICON = 3,

  // Only used on Android, a modal dialog.
  MODAL_DIALOG = 4,

  // Only used on Android, an initially-collapsed infobar at the bottom of the
  // page.
  MINI_INFOBAR = 5,

  // Only used on desktop, a chip on the left-hand side of the location bar that
  // shows a bubble when clicked.
  LOCATION_BAR_LEFT_CHIP = 6,

  // There was no UI being shown. This is usually because the user closed an
  // inactive tab that had a pending permission request.
  NONE_VISIBLE = 7,
};

enum class AdaptiveTriggers {
  // None of the adaptive triggers were met. Currently this means two or less
  // consecutive denies in a row.
  NONE = 0,

  // User denied permission prompt 3 or more times.
  THREE_CONSECUTIVE_DENIES = 0x01,
};

enum class PermissionAutoRevocationHistory {
  // Permission has not been automatically revoked.
  NONE = 0,

  // Permission has been automatically revoked.
  PREVIOUSLY_AUTO_REVOKED = 0x01,
};

// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  static const char kPermissionsPromptShown[];
  static const char kPermissionsPromptShownGesture[];
  static const char kPermissionsPromptShownNoGesture[];
  static const char kPermissionsPromptAccepted[];
  static const char kPermissionsPromptAcceptedGesture[];
  static const char kPermissionsPromptAcceptedNoGesture[];
  static const char kPermissionsPromptDenied[];
  static const char kPermissionsPromptDeniedGesture[];
  static const char kPermissionsPromptDeniedNoGesture[];

  static void PermissionRequested(ContentSettingsType permission,
                                  const GURL& requesting_origin);
  static void PermissionRevoked(ContentSettingsType permission,
                                PermissionSourceUI source_ui,
                                const GURL& revoked_origin,
                                content::BrowserContext* browser_context);

  static void RecordEmbargoPromptSuppression(
      PermissionEmbargoStatus embargo_status);

  static void RecordEmbargoPromptSuppressionFromSource(
      PermissionStatusSource source);

  static void RecordEmbargoStatus(PermissionEmbargoStatus embargo_status);

  // UMA specifically for when permission prompts are shown. This should be
  // roughly equivalent to the metrics above, however it is
  // useful to have separate UMA to a few reasons:
  // - to account for, and get data on coalesced permission bubbles
  // - there are other types of permissions prompts (e.g. download limiting)
  //   which don't go through PermissionContext
  // - the above metrics don't always add up (e.g. sum of
  //   granted+denied+dismissed+ignored is not equal to requested), so it is
  //   unclear from those metrics alone how many prompts are seen by users.
  static void PermissionPromptShown(
      const std::vector<PermissionRequest*>& requests);

  static void PermissionPromptResolved(
      const std::vector<PermissionRequest*>& requests,
      content::WebContents* web_contents,
      PermissionAction permission_action,
      PermissionPromptDisposition ui_disposition);

  static void RecordWithBatteryBucket(const std::string& histogram);

  static void RecordInfobarDetailsExpanded(bool expanded);

  static void RecordCrowdDenyIsLoadedAtAbuseCheckTime(bool loaded);

  static void RecordCrowdDenyVersionAtAbuseCheckTime(
      const base::Optional<base::Version>& version);

  // Record UMAs related to the Android "Missing permissions" infobar.
  static void RecordMissingPermissionInfobarShouldShow(
      bool should_show,
      const std::vector<ContentSettingsType>& content_settings_types);
  static void RecordMissingPermissionInfobarAction(
      PermissionAction action,
      const std::vector<ContentSettingsType>& content_settings_types);

  static void RecordTimeElapsedBetweenGrantAndUse(ContentSettingsType type,
                                                  base::TimeDelta delta);

  static void RecordTimeElapsedBetweenGrantAndRevoke(ContentSettingsType type,
                                                     base::TimeDelta delta);

  // A scoped class that will check the current resolved content setting on
  // construction and report a revocation metric accordingly if the revocation
  // condition is met (from ALLOW to something else).
  class ScopedRevocationReporter {
   public:
    ScopedRevocationReporter(content::BrowserContext* browser_context,
                             const GURL& primary_url,
                             const GURL& secondary_url,
                             ContentSettingsType content_type,
                             PermissionSourceUI source_ui);

    ScopedRevocationReporter(content::BrowserContext* browser_context,
                             const ContentSettingsPattern& primary_pattern,
                             const ContentSettingsPattern& secondary_pattern,
                             ContentSettingsType content_type,
                             PermissionSourceUI source_ui);

    ~ScopedRevocationReporter();

   private:
    content::BrowserContext* browser_context_;
    const GURL primary_url_;
    const GURL secondary_url_;
    ContentSettingsType content_type_;
    PermissionSourceUI source_ui_;
    bool is_initially_allowed_;
    base::Time last_modified_date_;
  };

 private:
  friend class PermissionUmaUtilTest;

  // web_contents may be null when for recording non-prompt actions.
  static void RecordPermissionAction(ContentSettingsType permission,
                                     PermissionAction action,
                                     PermissionSourceUI source_ui,
                                     PermissionRequestGestureType gesture_type,
                                     PermissionPromptDisposition ui_disposition,
                                     const GURL& requesting_origin,
                                     const content::WebContents* web_contents,
                                     content::BrowserContext* browser_context);

  // Records |count| total prior actions for a prompt of type |permission|
  // for a single origin using |prefix| for the metric.
  static void RecordPermissionPromptPriorCount(ContentSettingsType permission,
                                               const std::string& prefix,
                                               int count);

  static void RecordPromptDecided(
      const std::vector<PermissionRequest*>& requests,
      bool accepted);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUmaUtil);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_
