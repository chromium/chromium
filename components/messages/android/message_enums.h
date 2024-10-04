// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_

namespace messages {

// List of constants describing the reasons why the message was dismissed.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// TODO(crbug.com/40755174): Revisit enum values. TAB_SWITCHED is not currently
// used. Likely the same for TAB_DESTROYED and ACTIVITY_DESTROYED. We also need
// a dedicated value for message dismissed from feature code.
enum class DismissReason {
  // Dismiss reasons that are fully controlled by clients (i.e. are not used
  // inside the Messages implementation are marked "Controlled by client" on
  // comments.

  UNKNOWN = 0,
  // A message was dismissed by clicking on the primary action button.
  PRIMARY_ACTION = 1,
  // Controlled by client: A message was dismissed by clicking on the secondary
  // action button.
  SECONDARY_ACTION = 2,
  // A message was automatically dismissed based on timer.
  TIMER = 3,
  // A message was dismissed by user gesture.
  GESTURE = 4,
  // Controlled by client: A message was dismissed in response to switching
  // tabs.
  TAB_SWITCHED = 5,
  // Controlled by client: A message was dismissed as a result of closing a tab.
  TAB_DESTROYED = 6,
  // Controlled by client: A message is dismissed because the activity is
  // destroyed.
  ACTIVITY_DESTROYED = 7,
  // A message was dismissed due to the destruction of the corresponding scopes.
  SCOPE_DESTROYED = 8,
  // A message was dismissed explicitly in feature code.
  DISMISSED_BY_FEATURE = 9,

  // Insert new values before this line.
  COUNT,

  kMaxValue = COUNT,
};

// "Urgent" means the user should take actions ASAP, such as responding to
// permissions or safety warnings.
enum class MessagePriority { kUrgent, kNormal };

// The constants of message scope type.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class MessageScopeType {
  WINDOW = 0,
  WEB_CONTENTS = 1,
  NAVIGATION = 2,
  ORIGIN = 3
};

// Enumerates unique identifiers for various messages. Used for recording
// messages related histograms.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// When adding a new message identifier, make corresponding changes in the
// following locations:
// - tools/metrics/histograms/metadata/android/enums.xml:
//       <enum name="MessageIdentifier">
// - tools/metrics/histograms/metadata/android/histograms.xml:
//       <variants name="MessageIdentifiers">
// - MessagesMetrics.java: #messageIdentifierToHistogramSuffix()
//
// A Java counterpart is generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
//
// LINT.IfChange(MessageIdentifier)
enum class MessageIdentifier {
  INVALID_MESSAGE = 0,
  SAVE_PASSWORD = 1,
  UPDATE_PASSWORD = 2,
  GENERATED_PASSWORD_SAVED = 3,
  POPUP_BLOCKED = 4,
  SAFETY_TIP = 5,
  SAVE_ADDRESS_PROFILE = 6,
  MERCHANT_TRUST = 7,
  // Removed: ADD_TO_HOMESCREEN_IPH = 8,
  SEND_TAB_TO_SELF = 9,
  READER_MODE = 10,
  CHROME_SURVEY = 11,
  SAVE_CARD = 12,
  NOTIFICATION_BLOCKED = 13,
  PERMISSION_UPDATE = 14,
  ADS_BLOCKED = 15,
  DOWNLOAD_PROGRESS = 16,
  SYNC_ERROR = 17,
  SHARED_HIGHLIGHTING = 18,
  NEAR_OOM_REDUCTION = 19,
  INSTALLABLE_AMBIENT_BADGE = 20,
  AUTO_DARK_WEB_CONTENTS = 21,
  TEST_MESSAGE = 22,
  TAILORED_SECURITY_ENABLED = 23,
  VR_SERVICES_UPGRADE = 24,
  TAILORED_SECURITY_DISABLED = 25,
  AR_CORE_UPGRADE = 26,
  // Removed: INSTANT_APPS = 27,
  ABOUT_THIS_SITE = 28,
  TRANSLATE = 29,
  OFFER_NOTIFICATION = 30,
  EXTERNAL_NAVIGATION = 31,
  FRAMEBUST_BLOCKED = 32,
  DESKTOP_SITE_GLOBAL_DEFAULT_OPT_OUT = 33,
  DESKTOP_SITE_GLOBAL_OPT_IN = 34,
  PASSWORD_MANAGER_ERROR = 35,
  DOWNLOAD_INCOGNITO_WARNING = 36,
  // Removed: RESTORE_CUSTOM_TAB = 37,
  // Removed: UNDO_CUSTOM_TAB_RESTORATION = 38,
  CVC_SAVE = 39,
  // Removed: TRACKING_PROTECTION_NOTICE = 40,
  DESKTOP_SITE_WINDOW_SETTING = 41,
  PROMPT_HATS_LOCATION_CUSTOM_INVITATION = 42,
  PROMPT_HATS_LOCATION_GENERIC_INVITATION = 43,
  PROMPT_HATS_CAMERA_CUSTOM_INVITATION = 44,
  PROMPT_HATS_CAMERA_GENERIC_INVITATION = 45,
  PROMPT_HATS_MICROPHONE_CUSTOM_INVITATION = 46,
  PROMPT_HATS_MICROPHONE_GENERIC_INVITATION = 47,
  PERMISSION_BLOCKED = 48,
  SAVE_CARD_FAILURE = 49,
  VIRTUAL_CARD_ENROLL_FAILURE = 50,
  PROMPT_HATS_QUICK_DELETE = 51,
  PROMPT_HATS_SAFETY_HUB = 52,
  DEFAULT_BROWSER_PROMO = 53,
  TAB_REMOVED_THROUGH_COLLABORATION = 54,
  TAB_NAVIGATED_THROUGH_COLLABORATION = 55,
  COLLABORATION_USER_JOINED = 56,
  COLLABORATION_REMOVED = 57,
  // Insert new values before this line.
  COUNT
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/histograms.xml:MessageIdentifier)

// The behavior the message should follow when the primary button is clicked,
// after running the primary action callback.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class PrimaryActionClickBehavior {
  DO_NOT_DISMISS = 0,
  DISMISS_IMMEDIATELY = 1
};

// The max size of the message secondary menu.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class SecondaryMenuMaxSize {
  SMALL = 0,   // default: 180dp -> @dimen/message_secondary_menu_max_size_small
  MEDIUM = 1,  // 250dp -> @dimen/message_secondary_menu_max_size_medium
  LARGE = 2,   // 300dp -> @dimen/message_secondary_menu_max_size_large
};

// The primary widget that should be shown in the message.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.messages
enum class PrimaryWidgetAppearance {
  // Default value. Show the primary action button if non-empty text has been
  // set for the primary action button, otherwise no primary widget is shown.
  BUTTON_IF_TEXT_IS_SET = 0,
  // Show a spinning progress indicator that isn't clickable.
  PROGRESS_SPINNER = 1,
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGE_ENUMS_H_
