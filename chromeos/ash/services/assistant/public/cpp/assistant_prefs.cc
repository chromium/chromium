// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"

#include "base/notreached.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::assistant::prefs {

// NOTE: These values are persisted in preferences and cannot be changed.
const char kAssistantOnboardingModeDefault[] = "Default";
const char kAssistantOnboardingModeEducation[] = "Education";

// A preference that indicates the activity control consent status from user.
// This preference should only be changed in browser.
const char kAssistantConsentStatus[] =
    "settings.voice_interaction.activity_control.consent_status";
// A preference that indicates the user has allowed the Assistant services
// to access the "context" (text and graphic content that is currently on
// screen). This preference can be overridden by the
// VoiceInteractionContextEnabled administrator policy.
const char kAssistantContextEnabled[] =
    "settings.voice_interaction.context.enabled";
// A preference that indicates the Assistant has been disabled by domain policy.
// If true, the Assistant will always been disabled and user cannot enable it.
// This preference should only be changed in browser.
const char kAssistantDisabledByPolicy[] =
    "settings.assistant.disabled_by_policy";
// A preference that indicates the user has enabled the Assistant services.
const char kAssistantEnabled[] = "settings.voice_interaction.enabled";
// A preference that indicates the user has chosen to always keep hotword
// listening on even without DSP support.
// This preference should only be changed in browser.
const char kAssistantHotwordAlwaysOn[] =
    "settings.voice_interaction.hotword.always_on";
// A preference that indicates the user has allowed the Assistant services
// to use hotword listening. This preference can be overridden by the
// VoiceInteractionHotwordEnabled administrator policy.
const char kAssistantHotwordEnabled[] =
    "settings.voice_interaction.hotword.enabled";
// A preference that indicates whether microphone should be open when the
// Assistant launches.
// This preference should only be changed in browser.
const char kAssistantLaunchWithMicOpen[] =
    "settings.voice_interaction.launch_with_mic_open";
// A preference that indicates the user has allowed the Assistant services
// to send notification.
// This preference should only be changed in browser.
const char kAssistantNotificationEnabled[] =
    "settings.voice_interaction.notification.enabled";
// A preference that indicates the mode of the Assistant onboarding experience.
// This preference should only be changed via policy.
const char kAssistantOnboardingMode[] = "settings.assistant.onboarding_mode";
// A preference that indicates whether Voice Match is enabled during OOBE.
// This preference should only be changed via policy.
const char kAssistantVoiceMatchEnabledDuringOobe[] =
    "settings.voice_interaction.oobe_voice_match.enabled";
// A preference that stores the number of failures since the last successful run
// of Assistant service.
extern const char kAssistantNumFailuresSinceLastServiceRun[] =
    "ash.assistant.num_failures_since_last_service_run";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kAssistantConsentStatus,
                                ConsentStatus::kUnknown);
  registry->RegisterBooleanPref(kAssistantContextEnabled, false);
  registry->RegisterBooleanPref(kAssistantDisabledByPolicy, false);
  registry->RegisterBooleanPref(kAssistantEnabled, false);
  registry->RegisterBooleanPref(kAssistantHotwordAlwaysOn, false);
  registry->RegisterBooleanPref(kAssistantHotwordEnabled, false);
  registry->RegisterBooleanPref(kAssistantLaunchWithMicOpen, false);
  registry->RegisterBooleanPref(kAssistantNotificationEnabled, true);
  registry->RegisterBooleanPref(kAssistantVoiceMatchEnabledDuringOobe, true);
  registry->RegisterStringPref(kAssistantOnboardingMode,
                               kAssistantOnboardingModeDefault);
  registry->RegisterIntegerPref(prefs::kAssistantNumFailuresSinceLastServiceRun,
                                0);
}

AssistantOnboardingMode ToOnboardingMode(const std::string& onboarding_mode) {
  if (onboarding_mode == kAssistantOnboardingModeEducation)
    return AssistantOnboardingMode::kEducation;
  if (onboarding_mode != kAssistantOnboardingModeDefault)
    NOTREACHED_IN_MIGRATION();
  return AssistantOnboardingMode::kDefault;
}

std::string ToOnboardingModeString(AssistantOnboardingMode onboarding_mode) {
  switch (onboarding_mode) {
    case AssistantOnboardingMode::kDefault:
      return kAssistantOnboardingModeDefault;
    case AssistantOnboardingMode::kEducation:
      return kAssistantOnboardingModeEducation;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace ash::assistant::prefs
