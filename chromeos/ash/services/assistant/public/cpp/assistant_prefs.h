// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_PREFS_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_PREFS_H_

#include <string>

#include "base/component_export.h"

class PrefRegistrySimple;

namespace ash::assistant::prefs {

// The status of the user's consent. The enum values cannot be changed because
// they are persisted on disk.
enum ConsentStatus {
  // The status is unknown.
  kUnknown = 0,

  // The user accepted activity control access.
  kActivityControlAccepted = 1,

  // The user is not authorized to give consent.
  kUnauthorized = 2,

  // The user's consent information is not found. This is typically the case
  // when consent from the user has never been requested.
  kNotFound = 3,
};

// The mode of the Assistant onboarding experience.
// This enum is used in histogram, please do not change the values.
enum class AssistantOnboardingMode {
  kDefault = 0,    // Maps to kAssistantOnboardingModeDefault.
  kEducation = 1,  // Maps to kAssistantOnboardingModeEducation.
  kMaxValue = kEducation
};

// Constants.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantOnboardingModeDefault[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantOnboardingModeEducation[];

// Preferences.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantConsentStatus[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantContextEnabled[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantDisabledByPolicy[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantEnabled[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantHotwordAlwaysOn[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantHotwordEnabled[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantLaunchWithMicOpen[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantNotificationEnabled[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantOnboardingMode[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantVoiceMatchEnabledDuringOobe[];
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const char kAssistantNumFailuresSinceLastServiceRun[];

// Registers Assistant specific profile preferences for browser prefs.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Converts between onboarding mode enum and string representations.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
AssistantOnboardingMode ToOnboardingMode(const std::string& onboarding_mode);
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
std::string ToOnboardingModeString(AssistantOnboardingMode onboarding_mode);

}  // namespace ash::assistant::prefs

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
namespace prefs = ::ash::assistant::prefs;
}

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_PREFS_H_
