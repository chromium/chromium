// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UMA_CONSTANTS_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UMA_CONSTANTS_H_

#include "base/containers/fixed_flat_set.h"

namespace permissions {

// This enum backs a UMA histogram, so it must be treated as append-only.
enum class PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,
  REVOKED = 4,
  GRANTED_ONCE = 5,

  // Always keep this at the end.
  NUM,
};

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
//      tools/metrics/histograms/enums.xml.
//   2) The PermissionRequestTypes suffix list in
//      tools/metrics/histograms/metadata/permissions/histograms.xml.
//   3) GetPermissionRequestString function in
//      components/permissions/permission_uma_util.cc
//
// The usual rules of updating UMA values applies to this enum:
// - don't remove values
// - only ever add values at the end
// LINT.IfChange(RequestTypeForUma)
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
  // PERMISSION_FLASH = 12,
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
  // PERMISSION_FILE_HANDLING = 28,
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
  // PERMISSION_LOCAL_NETWORK_ACCESS = 42,
  PERMISSION_LOCAL_NETWORK = 43,
  PERMISSION_LOOPBACK_NETWORK = 44,
  PERMISSION_SENSORS = 45,
  PERMISSION_GEOLOCATION_APPROXIMATE_OR_PRECISE = 46,
  PERMISSION_GEOLOCATION_APPROXIMATE = 47,
  PERMISSION_GEOLOCATION_UPGRADE = 48,
  // NUM must be the last value in the enum.
  NUM,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PermissionRequestType,
// //components/permissions/permission_uma_util.cc:GetPermissionRequestString,
// //tools/metrics/histograms/metadata/permissions/histograms.xml:PermissionRequestTypes)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Any new values should be inserted
// immediately prior to kMaxValue.
// LINT.IfChange(PermissionSourceUI)
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
  // Android O+ system channel settings activity, and only when that activity
  // is launched directly from the Chrome site settings, which is not a common
  // user journey (see usages of `REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS`).
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
  // This is recorded instead of ANDROID_SETTINGS above when the Android system
  // settings UI interaction happens while Chrome is not running, and thus
  // Chrome only observes the permission change on next start-up.
  UNIDENTIFIED = 8,

  // Permission changes due to automatic revocation of disruptive notifications.
  DISRUPTIVE_NOTIFICATION_REVOCATION = 9,

  // Always keep this at the end.
  kMaxValue = DISRUPTIVE_NOTIFICATION_REVOCATION,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionSourceUI)

// Any new values should be inserted immediately prior to NUM.
// LINT.IfChange(PermissionEmbargoStatus)
enum class PermissionEmbargoStatus {
  NOT_EMBARGOED = 0,
  // Removed: PERMISSIONS_BLACKLISTING = 1,
  REPEATED_DISMISSALS = 2,
  REPEATED_IGNORES = 3,
  RECENT_DISPLAY = 4,

  // Keep this at the end.
  NUM,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionEmbargoStatus)

// Used for UMA to record the strict level of permission policy which is
// configured to allow sub-frame origin. Any new values should be inserted
// immediately prior to NUM. All values here should have corresponding entries
// PermissionsPolicyConfiguration area of enums.xml.
// LINT.IfChange(PermissionHeaderPolicyForUMA)
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
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionsPolicyConfiguration)

// The kind of permission prompt UX used to surface a permission request.
// Enum used in UKMs and UMAs, do not re-order or change values. Deprecated
// items should only be commented out. New items should be added at the end.
// LINT.IfChange(PermissionPromptDisposition)
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
  // DEPRECATED: This disposition is no longer existent.
  // LOCATION_BAR_LEFT_CHIP = 6,

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

  // A prompt shown as a result of the user clicking the permission element.
  ELEMENT_ANCHORED_BUBBLE = 13,

  // Only used on macOS, a native OS provided permission prompt.
  MAC_OS_PROMPT = 14,

  // Only used on Android, a message bubble near top of the screen and below the
  // location bar. This is a flavor of MESSAGE_UI that is used for loud prompts.
  MESSAGE_UI_LOUD = 15,

  // Only used on Android. The prompt is suppressed, and the user is notified
  // via an icon on the left-hand side of the location bar.
  LOCATION_BAR_LEFT_QUIET_ICON = 16,

  kMaxValue = LOCATION_BAR_LEFT_QUIET_ICON,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/histograms.xml:PromptDisposition,
// :PromptDispositionSets)

// The reason why the permission prompt disposition was used. Enum used in UKMs,
// do not re-order or change values. Deprecated items should only be commented
// out.
// LINT.IfChange(PermissionPromptDispositionReason)
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

  // Disposition was chosen because the request lacked a user gesture.
  LACK_OF_GESTURE = 5,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PermissionPromptDispositionReason)

enum class AdaptiveTriggers {
  // None of the adaptive triggers were met. Currently this means two or less
  // consecutive denies in a row.
  NONE = 0,

  // User denied permission prompt 3 or more times.
  THREE_CONSECUTIVE_DENIES = 0x01,
};

// LINT.IfChange(DismissedReason)
enum class DismissedReason {
  // The prompt was dismissed through the [x] button.
  kDismissedXButton = 0,

  // The prompt was dismissed through the user clicking on the scrim (area
  // around the prompt).
  kDismissedScrim = 1,

  kMaxValue = kDismissedScrim,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:DismissedReason)

// LINT.IfChange(OsScreen)
enum class OsScreen {
  // Informs the user that Chrome needs permission from the OS level.
  kOsPrompt = 0,

  // Informs the user that they need to go to OS system settings.
  kOsSystemSettings = 1,

  kMaxValue = kOsSystemSettings,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/histograms.xml:OsScreen)

// LINT.IfChange(OsScreenAction)
enum class OsScreenAction {
  // User clicks on "Go to System settings"
  kSystemSettings = 0,

  // The prompt was dismissed through the [x] button.
  kDismissedXButton = 1,

  // The prompt was dismissed through the user clicking on the scrim (area
  // around the prompt).
  kDismissedScrim = 2,

  // Os prompt denied.
  kOsPromptDenied = 3,

  // Os prompt allowed.
  kOsPromptAllowed = 4,

  kMaxValue = kOsPromptAllowed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/histograms.xml:OsScreenAction)

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "OneTimePermissionEvent" in tools/metrics/histograms/enums.xml.
// LINT.IfChange(OneTimePermissionEvent)
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
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:OneTimePermissionEvent)

// LINT.IfChange(ElementAnchoredBubbleVariant)
// Prompt views shown after the user clicks on the embedded permission prompt.
// The values represent the priority of each variant, higher number means
// higher priority.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ElementAnchoredBubbleVariant {
  // Default when conditions are not met to show any of the permission views.
  kUninitialized = 0,
  // Informs the user that the permission was allowed by their administrator.
  kAdministratorGranted = 1,
  // Permission prompt that informs the user they already granted permission.
  // Offers additional options to modify the permission decision.
  kPreviouslyGranted = 2,
  // Informs the user that they need to go to OS system settings to grant
  // access to Chrome.
  kOsSystemSettings = 3,
  // Informs the user that Chrome needs permission from the OS level, in order
  // for the site to be able to access a permission.
  kOsPrompt = 4,
  // Permission prompt that asks the user for site-level permission.
  kAsk = 5,
  // Permission prompt that additionally informs the user that they have
  // previously denied permission to the site. May offer different options
  // (buttons) to the site-level prompt |kAsk|.
  kPreviouslyDenied = 6,
  // Informs the user that the permission was denied by their administrator.
  kAdministratorDenied = 7,

  kMaxValue = kAdministratorDenied,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ElementAnchoredBubbleVariant)

// LINT.IfChange(PermissionAutoRevocationHistory)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PermissionAutoRevocationHistory {
  // Permission has not been automatically revoked.
  NONE = 0,

  // Permission has been automatically revoked.
  PREVIOUSLY_AUTO_REVOKED = 0x01,

  // Always keep at the end.
  kMaxValue = PREVIOUSLY_AUTO_REVOKED,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PermissionAutoRevocationHistory)

// This enum backs up the
// 'Permissions.PageInfo.ChangedWithin1m.{PermissionType}' histograms enum. It
// is used for collecting page info permission change metrics following in the
// first minute after a PermissionAction has been taken. Note that
// PermissionActions  DISMISSED and IGNORED are not taken into account, as they
// don't have an effect on the content settings.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionChangeAction)
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
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionChangeAction)

// LINT.IfChange(ElementAnchoredBubbleAction)
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
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ElementAnchoredBubbleAction)

// The reason the permission action `PermissionAction::IGNORED` was triggered.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionIgnoredReason)
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
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionRequestIgnoredReason)

// This enum backs up the
// 'Permissions.PageInfo.Changed.{PermissionType}.Reallowed.Outcome' histograms
// enum. It is used for collecting permission usage rates after permission
// status was reallowed via PageInfo. It is applicable only if permission is
// allowed as all other states are no-op for an origin.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PermissionChangeInfo)
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
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionChangeInfo)

// LINT.IfChange(DismissalType)

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

  // It's possible that the modal dialog manager is null when showing a dialog,
  // for example if the tab has been navigated/closed or the layout might not be
  // inflated in some embedders (e.g WebEngine).
  kAutodismissNoDialogManager = 5,

  // The user dismissed by clicking on the close button.
  kCloseButtonClicked = 6,

  // Always keep this at the end.
  kMaxValue = kCloseButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/permissions/enums.xml:PermissionPromptDismissMethod)

// LINT.IfChange(UkmPermissionPromptOptions)
// This enum backs the UKM Permission.PromptOptions, so it must be treated as
// append-only.
enum class UkmPermissionPromptOptions {
  APPROXIMATE_LOCATION = 1,
  PRECISE_LOCATION = 2,
};
// LINT.ThenChange(//tools/metrics/ukm/ukm.xml:UkmPermissionPromptOptions)

// LINT.IfChange(PromptDispositionSets)
inline constexpr auto kQuietPromptDispositions =
    base::MakeFixedFlatSet<PermissionPromptDisposition>({
        PermissionPromptDisposition::LOCATION_BAR_RIGHT_STATIC_ICON,
        PermissionPromptDisposition::LOCATION_BAR_RIGHT_ANIMATED_ICON,
        PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_CHIP,
        PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP,
        PermissionPromptDisposition::MINI_INFOBAR,
        PermissionPromptDisposition::MESSAGE_UI,
        PermissionPromptDisposition::LOCATION_BAR_LEFT_QUIET_ICON,
    });

inline constexpr auto kLoudPromptDispositions =
    base::MakeFixedFlatSet<PermissionPromptDisposition>({
        PermissionPromptDisposition::ANCHORED_BUBBLE,
        PermissionPromptDisposition::ELEMENT_ANCHORED_BUBBLE,
        PermissionPromptDisposition::CUSTOM_MODAL_DIALOG,
        PermissionPromptDisposition::MODAL_DIALOG,
        PermissionPromptDisposition::MAC_OS_PROMPT,
        PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE,
        PermissionPromptDisposition::MESSAGE_UI_LOUD,
    });
// LINT.ThenChange(:PermissionPromptDisposition)

// We want to assign every new prompt disposition either to quiet or loud
// prompts set. Left out:
//  - NOT_APPLICABLE (0 anyways)
//  - LOCATION_BAR_LEFT_CHIP (deprecated)
//  - NONE_VISIBLE
static_assert(kLoudPromptDispositions.size() +
                  kQuietPromptDispositions.size() ==
              static_cast<size_t>(PermissionPromptDisposition::kMaxValue) - 2);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UMA_CONSTANTS_H_
