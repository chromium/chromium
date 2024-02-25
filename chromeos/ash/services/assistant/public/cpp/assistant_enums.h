// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_

namespace ash::assistant {
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
  DISALLOWED_BY_POLICY = 1,
  // Disallowed because user's locale is not compatible.
  DISALLOWED_BY_LOCALE = 2,
  // Disallowed because current user is not primary user.
  DISALLOWED_BY_NONPRIMARY_USER = 3,
  // DISALLOWED_BY_SUPERVISED_USER = 4, // Deprecated.
  // Disallowed because incognito mode.
  DISALLOWED_BY_INCOGNITO = 5,
  // Disallowed because the device is in demo mode.
  DISALLOWED_BY_DEMO_MODE = 6,
  // Disallowed because the device is in public session.
  DISALLOWED_BY_PUBLIC_SESSION = 7,
  // Disallowed because the user's account type is currently not supported.
  DISALLOWED_BY_ACCOUNT_TYPE = 8,
  // Disallowed because the device is in Kiosk mode.
  DISALLOWED_BY_KIOSK_MODE = 9,
  // Disallowed because no libassistant binary available.
  DISALLOWED_BY_NO_BINARY = 10,

  MAX_VALUE = DISALLOWED_BY_NO_BINARY,
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

// Enumeration of Assistant entry points. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if the possible entry source grows.
enum class AssistantEntryPoint {
  kUnspecified = 0,
  kMinValue = kUnspecified,
  kDeepLink = 1,
  kHotkey = 2,
  kHotword = 3,
  // Deprecated, please do not reuse
  // kLauncherSearchBox = 4,
  kLongPressLauncher = 5,
  kSetup = 6,
  kStylus = 7,
  kLauncherSearchResult = 8,
  kLauncherSearchBoxIcon = 9,
  // Deprecated, please do not reuse
  // kProactiveSuggestions = 10,
  // Deprecated, please do not reuse
  // kLauncherChip = 11,
  // Deprecated, please do not reuse
  // kBloom = 12,
  kLauncherSearchIphChip = 13,
  kMaxValue = kLauncherSearchIphChip
};

// Enumeration of Assistant exit points. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if the possible exit source grows.
enum class AssistantExitPoint {
  // Includes keyboard interruptions (e.g. launching Chrome OS feedback
  // using keyboard shortcuts, pressing search button).
  kUnspecified = 0,
  kMinValue = kUnspecified,
  // kCloseButtonDeprecated = 1,
  kHotkey = 2,
  kNewBrowserTabFromServer = 3,
  kNewBrowserTabFromUser = 4,
  // kOutsidePressDeprecated = 5,
  kSetup = 6,
  kStylus = 7,
  kBackInLauncher = 8,
  kLauncherClose = 9,
  kLauncherOpen = 10,
  kScreenshot = 11,
  kOverviewMode = 12,
  kLauncherSearchIphChip = 13,
  kMaxValue = kLauncherSearchIphChip,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// If any value is added, please update enums.xml
// Assistant.LibassistantDlcInstallResult.
// Enumeration of the result to install libassistant DLC.
enum class LibassistantDlcInstallResult {
  kSuccess = 0,
  kErrorInternal = 1,
  kErrorBusy = 2,
  kErrorNeedReboot = 3,
  kErrorInvalidDlc = 4,
  kErrorAllocation = 5,
  kErrorNoImageFound = 6,

  kMaxValue = kErrorNoImageFound,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// If any value is added, please update enums.xml
// Assistant.LibassistantDlcLoadStatus.
// Enumeration of the status to load libassistant DLC.
enum class LibassistantDlcLoadStatus {
  kNotLoaded = 0,
  kLoaded = 1,

  kMaxValue = kLoaded,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// If any value is added, please update enums.xml
// Assistant.LauncherSearchIphQueryType.{SearchBox, AssistantPage}
// Enumeration of the query types of the chip inside LauncherSearchIph.
enum class LauncherSearchIphQueryType {
  kWeather = 0,
  kUnitConversion1 = 1,
  kUnitConversion2 = 2,
  kTranslation = 3,
  kDefinition = 4,
  kCalculation = 5,

  kMaxValue = kCalculation,
};

}  // namespace ash::assistant

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
using ::ash::assistant::AssistantAllowedState;
using ::ash::assistant::AssistantInteractionResolution;
using ::ash::assistant::AssistantStatus;
using ::ash::assistant::LibassistantDlcInstallResult;
using ::ash::assistant::LibassistantDlcLoadStatus;
}  // namespace chromeos::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_ENUMS_H_
