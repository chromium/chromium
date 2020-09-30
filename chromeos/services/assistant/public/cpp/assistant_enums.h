// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_

namespace chromeos {
namespace assistant {
// The initial state is NOT_READY, after Assistant service started it becomes
// READY. When Assistant UI shows up the state becomes VISIBLE.
enum AssistantStatus {
  // The Assistant service is not ready yet.
  NOT_READY = 0,
  // The Assistant service is ready.
  READY,
};

enum AssistantAllowedState {
  // Assistant feature is allowed.
  ALLOWED = 0,
  // Disallowed because search and assistant is disabled by policy.
  DISALLOWED_BY_POLICY,
  // Disallowed because user's locale is not compatible.
  DISALLOWED_BY_LOCALE,
  // Disallowed because current user is not primary user.
  DISALLOWED_BY_NONPRIMARY_USER,
  // Disallowed because current user is supervised user.
  DISALLOWED_BY_SUPERVISED_USER,
  // Disallowed because incognito mode.
  DISALLOWED_BY_INCOGNITO,
  // Disallowed because the device is in demo mode.
  DISALLOWED_BY_DEMO_MODE,
  // Disallowed because the device is in public session.
  DISALLOWED_BY_PUBLIC_SESSION,
  // Disallowed because the user's account type is currently not supported.
  DISALLOWED_BY_ACCOUNT_TYPE,
  // Disallowed because the device is in Kiosk mode.
  DISALLOWED_BY_KIOSK_MODE,

  MAX_VALUE = DISALLOWED_BY_KIOSK_MODE,
};

// Enumeration of possible Assistant query sources. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never
// be reused. Append new values to the end.
enum class AssistantQuerySource {
  kUnspecified = 0,
  kMinValue = kUnspecified,
  kDeepLink = 1,
  kDialogPlateTextField = 2,
  kStylus = 3,
  kSuggestionChip = 4,
  kVoiceInput = 5,
  kProactiveSuggestions = 6,
  kLibAssistantInitiated = 7,
  // kWarmerWelcomeDeprecated = 8,
  kConversationStarter = 9,
  kWhatsOnMyScreen = 10,
  kQuickAnswers = 11,
  kLauncherChip = 12,
  kBetterOnboarding = 13,
  kBloom = 14,
  kMaxValue = kBloom,
};

// Enumeration of possible Assistant interaction types.
enum class AssistantInteractionType {
  kText,
  kVoice,
};

// Enumeration of possible completions for an Assistant interaction.
// The values are recorded in UMA, do not reuse existing values when updating.
enum class AssistantInteractionResolution {
  // Assistant interaction completed normally.
  kNormal = 0,
  // Assistant interaction completed due to barge in or cancellation.
  kInterruption = 1,
  // Assistant interaction completed due to error.
  kError = 2,
  // Assistant interaction completed due to mic timeout.
  kMicTimeout = 3,
  // Assistant interaction completed due to multi-device hotword loss.
  kMultiDeviceHotwordLoss = 4,
  kMaxValue = kMultiDeviceHotwordLoss,
};

// Enumeration of possible Assistant suggestion types.
enum class AssistantSuggestionType {
  kUnspecified,
  kConversationStarter,
  kBetterOnboarding,
};

// Enumeration of better onboarding options. This is reported to histogram,
// please do not change the values.
enum class AssistantBetterOnboardingType {
  kUnspecified = 0,
  kMath = 1,
  kKnowledgeEdu = 2,
  kConversion = 3,
  kKnowledge = 4,
  kProductivity = 5,
  kPersonality = 6,
  kLanguage = 7,
  kTechnical = 8,
  kMaxValue = kTechnical,
};

// Enumeration of Assistant entry points. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if the possible entry source grows.
enum class AssistantEntryPoint {
  kUnspecified = 0,
  kMinValue = kUnspecified,
  kDeepLink = 1,
  kHotkey = 2,
  kHotword = 3,
  // kLauncherSearchBoxDeprecated = 4,
  kLongPressLauncher = 5,
  kSetup = 6,
  kStylus = 7,
  kLauncherSearchResult = 8,
  kLauncherSearchBoxIcon = 9,
  kProactiveSuggestions = 10,
  kLauncherChip = 11,
  kBloom = 12,
  kMaxValue = kBloom,
};

// Enumeration of Assistant exit points. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if the possible exit source grows.
enum class AssistantExitPoint {
  // Includes keyboard interruptions (e.g. launching Chrome OS feedback
  // using keyboard shortcuts, pressing search button).
  kUnspecified = 0,
  kMinValue = kUnspecified,
  kCloseButton = 1,
  kHotkey = 2,
  kNewBrowserTabFromServer = 3,
  kNewBrowserTabFromUser = 4,
  kOutsidePress = 5,
  kSetup = 6,
  kStylus = 7,
  kBackInLauncher = 8,
  kLauncherClose = 9,
  kLauncherOpen = 10,
  kScreenshot = 11,
  kOverviewMode = 12,
  kMaxValue = kOverviewMode,
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
