// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace assistant {
namespace prefs {

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
}

}  // namespace prefs
}  // namespace assistant
}  // namespace chromeos
