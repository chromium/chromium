// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"
#include "components/permissions/request_type.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace blink {
enum class PermissionType;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
class RenderFrameHost;
}  // namespace content

class GURL;

namespace permissions {
enum class PermissionRequestGestureType;
enum class PermissionAction;
class PermissionRequest;

enum class ActivityIndicatorState {
  kInUse = 0,
  kBlockedOnSiteLevel = 1,
  kBlockedOnSystemLevel = 2,

  // Always keep at the end.
  kMaxValue = kBlockedOnSystemLevel,
};

// Used for UMA to record the types of permission prompts shown.
// When updating, you also need to update:
//   1) The PermissionRequestType enum in
//      tools/metrics/histograms/metadata/permissions/enums.xml.
//   2) The PermissionRequestTypes suffix list in
//      tools/metrics/histograms/metadata/histogram_suffixes_list.xml.
//   3) GetPermissionRequestString function in
//      components/permissions/permission_uma_util.cc
//
// The usual rules of updating UMA values applies to this enum:
// - don't remove values
// - only ever add values at the end
enum class RequestTypeForUma {
  UNKNOWN = 0,
  MULTIPLE_AUDIO_AND_VIDEO_CAPTURE = 1,
  // UNUSED_PERMISSION = 2,
  QUOTA = 3,
  DOWNLOAD = 4,
  // MEDIA_STREAM = 5,
  REGISTER_PROTOCOL_HANDLER = 6,
  PERMISSION_GEOLOCATION = 7,
  PERMISSION_MIDI_SYSEX = 8,
  PERMISSION_NOTIFICATIONS = 9,
  PERMISSION_PROTECTED_MEDIA_IDENTIFIER = 10,
  // PERMISSION_PUSH_MESSAGING = 11,
  PERMISSION_FLASH = 12,
  PERMISSION_MEDIASTREAM_MIC = 13,
  PERMISSION_MEDIASTREAM_CAMERA = 14,
  // PERMISSION_ACCESSIBILITY_EVENTS = 15,  // Removed in M131.
  // PERMISSION_CLIPBOARD_READ = 16, // Replaced by
  // PERMISSION_CLIPBOARD_READ_WRITE in M81.
  // PERMISSION_SECURITY_KEY_ATTESTATION = 17,
  PERMISSION_PAYMENT_HANDLER = 18,
  PERMISSION_NFC = 19,
  PERMISSION_CLIPBOARD_READ_WRITE = 20,
  PERMISSION_VR = 21,
  PERMISSION_AR = 22,
  PERMISSION_STORAGE_ACCESS = 23,
  PERMISSION_CAMERA_PAN_TILT_ZOOM = 24,
  PERMISSION_WINDOW_MANAGEMENT = 25,
  PERMISSION_LOCAL_FONTS = 26,
  PERMISSION_IDLE_DETECTION = 27,
  PERMISSION_FILE_HANDLING = 28,
  // PERMISSION_U2F_API_REQUEST = 29,
  PERMISSION_TOP_LEVEL_STORAGE_ACCESS = 30,
  // PERMISSION_MIDI = 31,
  PERMISSION_FILE_SYSTEM_ACCESS = 32,
  CAPTURED_SURFACE_CONTROL = 33,
  PERMISSION_SMART_CARD = 34,
  PERMISSION_WEB_PRINTING = 35,
  PERMISSION_IDENTITY_PROVIDER = 36,
  PERMISSION_KEYBOARD_LOCK = 37,
  PERMISSION_POINTER_LOCK = 38,
  MULTIPLE_KEYBOARD_AND_POINTER_LOCK = 39,
  PERMISSION_HAND_TRACKING = 40,
  PERMISSION_WEB_APP_INSTALLATION = 41,
  // NUM must be the last value in the enum.
  NUM,
};

// Any new values should be inserted immediately prior to kMaxValue.
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

  // Permission changes due to automatic revocations of permissions from unused
  // sites, as part of Safety Hub.
  SAFETY_HUB_AUTO_REVOCATION = 7,

  // The permission status changed, but we're unsure from what source.
  // This is likely ANDROID_SETTINGS above though.
  UNIDENTIFIED = 8,

  // Always keep this at the end.
  kMaxValue = UNIDENTIFIED,
};

// Any new values should be inserted immediately prior to NUM.
enum class PermissionEmbargoStatus {
  NOT_EMBARGOED = 0,
  // Removed: PERMISSIONS_BLACKLISTING = 1,
  REPEATED_DISMISSALS = 2,
  REPEATED_IGNORES = 3,
  RECENT_DISPLAY = 4,

  // Keep this at the end.
  NUM,
};

// Used for UMA to record the strict level of permission policy which is
// configured to allow sub-frame origin. Any new values should be inserted
// immediately prior to NUM. All values here should have corresponding entries
// PermissionsPolicyConfiguration area of enums.xml.
enum class PermissionHeaderPolicyForUMA {
  // No (or an invalid) Permissions-Policy header was present, results in an
  // empty features list. It indicates none security-awareness of permissions
  // policy configuration.
  HEADER_NOT_PRESENT_OR_INVALID = 0,

  // Permissions-Policy header was present, but it did not define an allowlist
  // for the feature. It indicates less security-awareness of permissions policy
  // configuration.
  FEATURE_NOT_PRESENT = 1,

  // The sub-frame origin is included in allow-list of permission
  // policy. This indicates a good policy configuration.
  FEATURE_ALLOWLIST_EXPLICITLY_MATCHES_ORIGIN = 2,

  // Granted by setting value of permission policy to '*'. This also
  // indicates a bad policy configuration.
  FEATURE_ALLOWLIST_IS_WILDCARD = 3,

  // The Permissions-Policy header was present and defined an empty
  // allowlist for the feature. The feature will be disabled everywhere.
  FEATURE_ALLOWLIST_IS_NONE = 4,

  // The sub-frame origin is not explicitly declared in allow-list of top level
  // permission policy. It generally indicates less security-awareness of
  // policy configuration.
  FEATURE_ALLOWLIST_DOES_NOT_MATCH_ORIGIN = 5,

  // Always keep this at the end.
  NUM,
};

// The kind of permission prompt UX used to surface a permission request.
// Enum used in UKMs and UMAs, do not re-order or change values. Deprecated
// items should only be commented out. New items should be added at the end,
// and the "PermissionPromptDisposition" histogram suffix needs to be updated to
// match (tools/metrics/histograms/metadata/histogram_suffixes_list.xml).
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

  // Other custom modal dialogs.
  CUSTOM_MODAL_DIALOG = 8,

  // Only used on desktop, a less prominent version of chip on the left-hand
  // side of the location bar that shows a bubble when clicked.
  LOCATION_BAR_LEFT_QUIET_CHIP = 9,

  // Only used on Android, a message bubble near top of the screen and below the
  // location bar. Message UI is an alternative UI to infobar UI.
  MESSAGE_UI = 10,

  // Only used on desktop, a chip on the left-hand side of the location bar that
  // automatically shows a bubble.
  LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP = 11,

  // Only used on desktop, a chip on the left-hand side of the location bar that
  // automatically shows a bubble.
  LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE = 12,

  // Only used on desktop, a bubble shown near the embedded permission element,
  // after the user clicks on the element.
  ELEMENT_ANCHORED_BUBBLE = 13,

  // Only used on macOS, a native OS provided permission prompt.
  MAC_OS_PROMPT = 14,
};

// The reason why the permission prompt disposition was used. Enum used in UKMs,
// do not re-order or change values. Deprecated items should only be commented
// out.
enum class PermissionPromptDispositionReason {
  // Disposition was selected in prefs.
  USER_PREFERENCE_IN_SETTINGS = 0,

  // Disposition was chosen because Safe Browsing classifies the origin
  // as being spammy or abusive with permission requests.
  SAFE_BROWSING_VERDICT = 1,

  // Disposition was chosen based on grant likelihood predicted by the
  // Web Permission Prediction Service.
  PREDICTION_SERVICE = 2,

  // Disposition was used as a fallback, if no selector made a decision.
  DEFAULT_FALLBACK = 3,

  // Disposition was chosen based on grant likelihood predicted by the On-Device
  // Permission Prediction Model.
  ON_DEVICE_PREDICTION_MODEL = 4,
};

enum class AdaptiveTriggers {
  // None of the adaptive triggers were met. Currently this means two or less
  // consecutive denies in a row.
  NONE = 0,

  // User denied permission prompt 3 or more times.
  THREE_CONSECUTIVE_DENIES = 0x01,
};

enum class DismissedReason {
  // The prompt was dismissed through the [x] button.
  DISMISSED_X_BUTTON = 0,

  // The prompt was dismissed through the user clicking on the scrim (area
  // around the prompt).
  DISMISSED_SCRIM = 1,

  kMaxValue = DISMISSED_SCRIM,
};

enum class OsScreen {
  // Informs the user that Chrome needs permission from the OS level.
  OS_PROMPT = 0,

  // Informs the user that they need to go to OS system settings.
  OS_SYSTEM_SETTINGS = 1,

  kMaxValue = OS_SYSTEM_SETTINGS,
};

enum class OsScreenAction {
  // User clicks on "Go to System settings"
  SYSTEM_SETTINGS = 0,

  // The prompt was dismissed through the [x] button.
  DISMISSED_X_BUTTON = 1,

  // The prompt was dismissed through the user clicking on the scrim (area
  // around the prompt).
  DISMISSED_SCRIM = 2,

  // Os prompt denied.
  OS_PROMPT_DENIED = 3,

  // Os prompt allowed.
  OS_PROMPT_ALLOWED = 4,

  kMaxValue = OS_PROMPT_ALLOWED,
};

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "OneTimePermissionEvent" in tools/metrics/histograms/enums.xml.
enum class OneTimePermissionEvent {
  // Recorded for each one time grant
  GRANTED_ONE_TIME = 0,

  // Recorded when the user manually revokes a one time grant
  REVOKED_MANUALLY = 1,

  // Recorded when a one time grant expires because all tabs are either closed
  // or discarded.
  ALL_TABS_CLOSED_OR_DISCARDED = 2,

  // Recorded when a one time grant expires because the permission was unused in
  // the background.
  EXPIRED_IN_BACKGROUND = 3,

  // Revoked because of the maximum one time permission lifetime
  // `kOneTimePermissionMaximumLifetime`
  EXPIRED_AFTER_MAXIMUM_LIFETIME = 4,

  // Recorded when a one time grant expires because the device was suspended.
  EXPIRED_ON_SUSPEND = 5,

  kMaxValue = EXPIRED_ON_SUSPEND,
};

// Prompt views shown after the user clicks on the embedded permission prompt.
// The values represent the priority of each variant, higher number means
// higher priority.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ElementAnchoredBubbleVariant {
  // Default when conditions are not met to show any of the permission views.
  UNINITIALIZED = 0,
  // Informs the user that the permission was allowed by their administrator.
  ADMINISTRATOR_GRANTED = 1,
  // Permission prompt that informs the user they already granted permission.
  // Offers additional options to modify the permission decision.
  PREVIOUSLY_GRANTED = 2,
  // Informs the user that they need to go to OS system settings to grant
  // access to Chrome.
  OS_SYSTEM_SETTINGS = 3,
  // Informs the user that Chrome needs permission from the OS level, in order
  // for the site to be able to access a permission.
  OS_PROMPT = 4,
  // Permission prompt that asks the user for site-level permission.
  ASK = 5,
  // Permission prompt that additionally informs the user that they have
  // previously denied permission to the site. May offer different options
  // (buttons) to the site-level prompt |kAsk|.
  PREVIOUSLY_DENIED = 6,
  // Informs the user that the permission was denied by their administrator.
  ADMINISTRATOR_DENIED = 7,

  kMaxValue = ADMINISTRATOR_DENIED,
};

enum class PermissionAutoRevocationHistory {
  // Permission has not been automatically revoked.
  NONE = 0,

  // Permission has been automatically revoked.
  PREVIOUSLY_AUTO_REVOKED = 0x01,
};

// This enum backs up the `AutoDSEPermissionRevertTransition` histogram enum.
// Never reuse values and mirror any updates to it.
// Describes the transition that has occured for the setting of a DSE origin
// when DSE autogrant becomes disabled.
enum class AutoDSEPermissionRevertTransition {
  // The user has not previously made any decision so it results in an `ASK` end
  // state.
  NO_DECISION_ASK = 0,
  // The user has decided to `ALLOW` the origin before it was the DSE origin and
  // has not reverted this decision.
  PRESERVE_ALLOW = 1,
  // The user has previously `BLOCKED` the origin but has allowed it after it
  // became the DSE origin. Resolve the conflict by setting it to `ASK` so the
  // user will make a decision again.
  CONFLICT_ASK = 2,
  // The user has blocked the DSE origin and has not made a previous decision
  // before the origin became the DSE origin.
  PRESERVE_BLOCK_ASK = 3,
  // The user has blocked the DSE origin and has `ALLOWED` it before it became
  // the DSE origin, preserve the latest decision.
  PRESERVE_BLOCK_ALLOW = 4,
  // The user has blocked the DSE origin and has `BLOCKED` it before it became
  // the DSE origin as well.
  PRESERVE_BLOCK_BLOCK = 5,
  // There has been an invalid transition.
  INVALID_END_STATE = 6,

  // Always keep at the end.
  kMaxValue = INVALID_END_STATE,
};

// This enum backs up the 'PermissionPredictionSource` histogram enum. It
// indicates whether the permission prediction was done by the local on device
// model or by the server side model.
enum class PermissionPredictionSource {
  ON_DEVICE = 0,
  SERVER_SIDE = 1,

  // Always keep at the end.
  kMaxValue = SERVER_SIDE,
};

// This enum backs up the 'PageInfoDialogAccessType' histogram enum.
// It is used for collecting page info access type metrics in the context of
// the confirmation chip.
enum class PageInfoDialogAccessType {
  // The user opened page info by clicking on the lock in a situation that is
  // considered independent of the display of a confirmation chip.
  LOCK_CLICK = 0,
  // The user opened page info by clicking on the lock while a confirmation chip
  // was being displayed.
  LOCK_CLICK_DURING_CONFIRMATION_CHIP = 1,
  // The user opened page info by clicking on the confirmation chip while it was
  // being displayed.
  CONFIRMATION_CHIP_CLICK = 2,

  // The user opened page info by clicking on the lock within
  // 'kConfirmationConsiderationDurationForUma' after confirmation chip has
  // collapsed. This click may be considered influenced by the displaying of the
  // confirmation chip.
  LOCK_CLICK_SHORTLY_AFTER_CONFIRMATION_CHIP = 3,

  // Always keep at the end.
  kMaxValue = LOCK_CLICK_SHORTLY_AFTER_CONFIRMATION_CHIP,
};

constexpr auto kConfirmationConsiderationDurationForUma = base::Seconds(20);

// This enum backs up the
// 'Permissions.PageInfo.ChangedWithin1m.{PermissionType}' histograms enum. It
// is used for collecting page info permission change metrics following in the
// first minute after a PermissionAction has been taken. Note that
// PermissionActions  DISMISSED and IGNORED are not taken into account, as they
// don't have an effect on the content settings.
enum class PermissionChangeAction {
  // PermissionAction was one of {GRANTED, GRANTED_ONCE} and the content
  // setting is changed to CONTENT_SETTING_BLOCK.
  REVOKED = 0,

  // PermissionAction was DENIED and the content setting is changed to
  // CONTENT_SETTING_ALLOW.
  REALLOWED = 1,

  // PermissionAction was one of {GRANTED, GRANTED_ONCE} and the content setting
  // is changed to CONTENT_SETTING_DEFAULT.
  RESET_FROM_ALLOWED = 2,

  // PermissionAction was DENIED and the content setting is changed to
  // CONTENT_SETTING_DEFAULT.
  RESET_FROM_DENIED = 3,

  // For one time grantable permissions, the user can toggle a remember checkbox
  // in the secondary page info page which toggles grants between permanent
  // grant and one time grant.
  REMEMBER_CHECKBOX_TOGGLED = 4,

  // Always keep at the end.
  kMaxValue = REMEMBER_CHECKBOX_TOGGLED,
};

// This enum backs up the 'ElementAnchoredBubbleAction' histograms enum.
enum class ElementAnchoredBubbleAction {
  // Site level permission was granted.
  kGranted = 0,

  // Site level permission was granted once.
  kGrantedOnce = 1,

  // Site level permission was denied.
  kDenied = 2,

  // Acknowledging the prompt informing the user a permission is managed by
  // admin.
  kOk = 3,

  // The prompt was dismissed by the user clicking on the [X] button.
  kDismissedXButton = 4,

  // The prompt was dismissed by the user clicking outside of the prompt area.
  kDismissedScrim = 5,

  // User clicked "Open system settings" to manage OS level permission prompts.
  kSystemSettings = 6,

  // Always keep at the end.
  kMaxValue = kSystemSettings,
};

// The reason the permission action `PermissionAction::IGNORED` was triggered.
enum class PermissionIgnoredReason {
  // Ignore was triggered due to closure of the browser window
  WINDOW_CLOSED = 0,

  // Ignore was triggered due to closure of the tab
  TAB_CLOSED = 1,

  // Ignore was triggered due to navigation
  NAVIGATION = 2,

  // Catches all other cases
  UNKNOWN = 3,

  // Always keep at the end
  NUM,
};

// This enum backs up the
// 'Permissions.PageInfo.Changed.{PermissionType}.Reallowed.Outcome' histograms
// enum. It is used for collecting permission usage rates after permission
// status was reallowed via PageInfo. It is applicable only if permission is
// allowed as all other states are no-op for an origin.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PermissionChangeInfo {
  kInfobarShownPageReloadPermissionUsed = 0,

  kInfobarShownPageReloadPermissionNotUsed = 1,

  kInfobarShownNoPageReloadPermissionUsed = 2,

  kInfobarShownNoPageReloadPermissionNotUsed = 3,

  kInfobarNotShownPageReloadPermissionUsed = 4,

  kInfobarNotShownPageReloadPermissionNotUsed = 5,

  kInfobarNotShownNoPageReloadPermissionUsed = 6,

  kInfobarNotShownNoPageReloadPermissionNotUsed = 7,

  // Always keep at the end.
  kMaxValue = kInfobarNotShownNoPageReloadPermissionNotUsed,
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.permissions
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: DismissalType
enum class DismissalType {
  // Fallback if a more specific dismissal type is not available..
  kUnspecified = 0,

  // The user dismissed by touching the back button.
  kNavigateBack = 1,  //

  // The user dismissed by touching outside the scrim
  kTouchOutside = 2,

  // It's possible for the context to be null if a prompt is
  // dequeued after the user backgrounds the browser and cleanup has already
  // happened. In that case, the prompt gets quietly dismissed.
  kAutodismissNoContext = 3,

  // The user accepted the site-level prompt but denied the
  // app-level prompt (= OS prompt), in which case the permission request gets
  // quietly dismissed.
  kAutodismissOsDenied = 4,

  // Always keep this at the end.
  kMaxValue = kAutodismissOsDenied,
};

// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  using PredictionGrantLikelihood =
      PermissionPrediction_Likelihood_DiscretizedLikelihood;

  static const char kPermissionsPromptShown[];
  static const char kPermissionsPromptShownGesture[];
  static const char kPermissionsPromptShownNoGesture[];
  static const char kPermissionsPromptAccepted[];
  static const char kPermissionsPromptAcceptedGesture[];
  static const char kPermissionsPromptAcceptedNoGesture[];
  static const char kPermissionsPromptAcceptedOnce[];
  static const char kPermissionsPromptAcceptedOnceGesture[];
  static const char kPermissionsPromptAcceptedOnceNoGesture[];
  static const char kPermissionsPromptDenied[];
  static const char kPermissionsPromptDeniedGesture[];
  static const char kPermissionsPromptDeniedNoGesture[];
  static const char kPermissionsPromptDismissed[];

  static const char kPermissionsExperimentalUsagePrefix[];
  static const char kPermissionsActionPrefix[];

  PermissionUmaUtil() = delete;
  PermissionUmaUtil(const PermissionUmaUtil&) = delete;
  PermissionUmaUtil& operator=(const PermissionUmaUtil&) = delete;

  static void PermissionRequested(ContentSettingsType permission);

  static void RecordActivityIndicator(std::set<ContentSettingsType> permissions,
                                      bool blocked,
                                      bool blocked_system_level,
                                      bool clicked);

  static void RecordDismissalType(
      const std::vector<ContentSettingsType>& content_settings_types,
      PermissionPromptDisposition ui_disposition,
      DismissalType dismissalType);

  static void RecordPermissionRequestedFromFrame(
      ContentSettingsType content_settings_type,
      content::RenderFrameHost* rfh);

  static void PermissionRequestPreignored(blink::PermissionType permission);

  // Records the revocation UMA and UKM metrics for ContentSettingsTypes that
  // have user facing permission prompts. The passed in `permission` must be
  // such that PermissionUtil::IsPermission(permission) returns true.
  static void PermissionRevoked(ContentSettingsType permission,
                                PermissionSourceUI source_ui,
                                const GURL& revoked_origin,
                                content::BrowserContext* browser_context);

  static void RecordEmbargoPromptSuppression(
      PermissionEmbargoStatus embargo_status);

  static void RecordEmbargoPromptSuppressionFromSource(
      content::PermissionStatusSource source);

  static void RecordEmbargoStatus(PermissionEmbargoStatus embargo_status);

  static void RecordPermissionRecoverySuccessRate(
      ContentSettingsType permission,
      bool is_used,
      bool show_infobar,
      bool page_reload);

  // Recorded when a permission prompt creation is in progress.
  static void RecordPermissionPromptAttempt(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      bool IsLocationBarEditingOrEmpty);

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
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests);

  static void PermissionPromptResolved(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      content::WebContents* web_contents,
      PermissionAction permission_action,
      base::TimeDelta time_to_action,
      PermissionPromptDisposition ui_disposition,
      std::optional<PermissionPromptDispositionReason> ui_reason,
      std::optional<std::vector<ElementAnchoredBubbleVariant>> variants,
      std::optional<PredictionGrantLikelihood> predicted_grant_likelihood,
      std::optional<bool> prediction_decision_held_back,
      std::optional<permissions::PermissionIgnoredReason> ignored_reason,
      bool did_show_prompt,
      bool did_click_manage,
      bool did_click_learn_more);

  static void RecordInfobarDetailsExpanded(bool expanded);

  static void RecordCrowdDenyDelayedPushNotification(base::TimeDelta delay);

  static void RecordCrowdDenyVersionAtAbuseCheckTime(
      const std::optional<base::Version>& version);

  static void RecordElementAnchoredBubbleDismiss(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      DismissedReason reason);

  static void RecordElementAnchoredBubbleOsMetrics(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      OsScreen screen,
      OsScreenAction action,
      base::TimeDelta time_to_action);

  static void RecordElementAnchoredBubbleVariantUMA(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      ElementAnchoredBubbleVariant variant);

  // Record UMAs related to the Android "Missing permissions" infobar.
  static void RecordMissingPermissionInfobarShouldShow(
      bool should_show,
      const std::vector<ContentSettingsType>& content_settings_types);
  static void RecordMissingPermissionInfobarAction(
      PermissionAction action,
      const std::vector<ContentSettingsType>& content_settings_types);

  static void RecordPermissionUsage(ContentSettingsType permission_type,
                                    content::BrowserContext* browser_context,
                                    content::WebContents* web_contents,
                                    const GURL& requesting_origin);

  static void RecordTimeElapsedBetweenGrantAndUse(
      ContentSettingsType type,
      base::TimeDelta delta,
      content_settings::SettingSource source);

  static void RecordTimeElapsedBetweenGrantAndRevoke(ContentSettingsType type,
                                                     base::TimeDelta delta);

  static void RecordAutoDSEPermissionReverted(
      ContentSettingsType permission_type,
      ContentSetting backed_up_setting,
      ContentSetting effective_setting,
      ContentSetting end_state_setting);

  static void RecordDSEEffectiveSetting(ContentSettingsType permission_type,
                                        ContentSetting setting);

  static void RecordPermissionPredictionSource(
      PermissionPredictionSource prediction_type);

  static void RecordPermissionPredictionServiceHoldback(
      RequestType request_type,
      bool is_on_device,
      bool is_heldback);

  static void RecordPageInfoDialogAccessType(
      PageInfoDialogAccessType access_type);

  static std::string GetOneTimePermissionEventHistogram(
      ContentSettingsType type);

  static void RecordOneTimePermissionEvent(ContentSettingsType type,
                                           OneTimePermissionEvent event);

  static void RecordPageInfoPermissionChangeWithin1m(
      ContentSettingsType type,
      PermissionAction previous_action,
      ContentSetting setting_after);

  static void RecordPageInfoPermissionChange(ContentSettingsType type,
                                             ContentSetting setting_before,
                                             ContentSetting setting_after,
                                             bool suppress_reload_page_bar);

  static std::string GetPermissionActionString(
      PermissionAction permission_action);

  static std::string GetPromptDispositionString(
      PermissionPromptDisposition ui_disposition);

  static std::string GetPromptDispositionReasonString(
      PermissionPromptDispositionReason ui_disposition_reason);

  static std::string GetRequestTypeString(RequestType request_type);

  static bool IsPromptDispositionQuiet(
      PermissionPromptDisposition prompt_disposition);

  static bool IsPromptDispositionLoud(
      PermissionPromptDisposition prompt_disposition);

  static void RecordIgnoreReason(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      PermissionPromptDisposition prompt_disposition,
      PermissionIgnoredReason reason);

  // Record metrics related to usage of permissions delegation.
  static void RecordPermissionsUsageSourceAndPolicyConfiguration(
      ContentSettingsType content_settings_type,
      content::RenderFrameHost* render_frame_host);

  static void RecordCrossOriginFrameActionAndPolicyConfiguration(
      ContentSettingsType content_settings_type,
      PermissionAction action,
      content::RenderFrameHost* render_frame_host);

  static void RecordTopLevelPermissionsHeaderPolicyOnNavigation(
      content::RenderFrameHost* render_frame_host);

  // Logs a metric that captures how long since revocation, due to a site being
  // considered unused, the user regrants a revoked permission.
  static void RecordPermissionRegrantForUnusedSites(
      const GURL& origin,
      ContentSettingsType request_type,
      PermissionSourceUI source_ui,
      content::BrowserContext* browser_context,
      base::Time current_time);

  static std::optional<uint32_t> GetDaysSinceUnusedSitePermissionRevocation(
      const GURL& origin,
      ContentSettingsType content_settings_type,
      base::Time current_time,
      HostContentSettingsMap* hcsm);

  // Records UKM metrics for ContentSettingsTypes that have user facing
  // permission prompts triggered by the user clicking on the Embedded
  // Permission Element. The passed in `permission` must be such that
  // PermissionUtil::IsPermission(permission) returns true.
  static void RecordElementAnchoredPermissionPromptAction(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          screen_requests,
      ElementAnchoredBubbleAction action,
      ElementAnchoredBubbleVariant variant,
      int screen_counter,
      const GURL& requesting_origin,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context);

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

    // Returns true if a ScopedRevocationReporter instance is in scope.
    static bool IsInstanceInScope();

   private:
    raw_ptr<content::BrowserContext> browser_context_;
    const GURL primary_url_;
    const GURL secondary_url_;
    ContentSettingsType content_type_;
    PermissionSourceUI source_ui_;
    bool is_initially_allowed_;
    base::Time last_modified_date_;
  };

 private:
  friend class PermissionUmaUtilTest;

  // Records UMA and UKM metrics for ContentSettingsTypes that have user facing
  // permission prompts. The passed in `permission` must be such that
  // PermissionUtil::IsPermission(permission) returns true.
  // web_contents may be null when for recording non-prompt actions.
  static void RecordPermissionAction(
      ContentSettingsType permission,
      PermissionAction action,
      PermissionSourceUI source_ui,
      PermissionRequestGestureType gesture_type,
      base::TimeDelta time_to_action,
      PermissionPromptDisposition ui_disposition,
      std::optional<PermissionPromptDispositionReason> ui_reason,
      std::optional<std::vector<ElementAnchoredBubbleVariant>> variants,
      const GURL& requesting_origin,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      content::RenderFrameHost* render_frame_host,
      std::optional<PredictionGrantLikelihood> predicted_grant_likelihood,
      std::optional<bool> prediction_decision_held_back);

  // Records |count| total prior actions for a prompt of type |permission|
  // for a single origin using |prefix| for the metric.
  static void RecordPermissionPromptPriorCount(ContentSettingsType permission,
                                               const std::string& prefix,
                                               int count);

  static void RecordPromptDecided(
      const std::vector<raw_ptr<PermissionRequest, VectorExperimental>>&
          requests,
      bool accepted,
      bool is_one_time);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UMA_UTIL_H_
