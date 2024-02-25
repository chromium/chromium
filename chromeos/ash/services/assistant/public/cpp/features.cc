// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/features.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "sandbox/policy/switches.h"

namespace ash::assistant::features {

BASE_FEATURE(kAssistantAudioEraser,
             "AssistantAudioEraser",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantAppSupport,
             "AssistantAppSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantDebugging,
             "AssistantDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAssistantWaitScheduling,
             "AssistantWaitScheduling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDspHotword,
             "EnableDspHotword",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableStereoAudioInput,
             "AssistantEnableStereoAudioInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePowerManager,
             "ChromeOSAssistantEnablePowerManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLibAssistantBetaBackend,
             "LibAssistantBetaBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Disable voice match for test purpose.
BASE_FEATURE(kDisableVoiceMatch,
             "DisableVoiceMatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLibAssistantDLC,
             "LibAssistantDLC",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableAssistantOnboarding,
             "AssistantOnboarding",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppSupportEnabled() {
  return base::FeatureList::IsEnabled(
      assistant::features::kAssistantAppSupport);
}

bool IsAudioEraserEnabled() {
  return base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsAssistantDebuggingEnabled() {
  return base::FeatureList::IsEnabled(kAssistantDebugging);
}

bool IsDspHotwordEnabled() {
  return base::FeatureList::IsEnabled(kEnableDspHotword);
}

bool IsPowerManagerEnabled() {
  return base::FeatureList::IsEnabled(kEnablePowerManager);
}

bool IsLibAssistantBetaBackendEnabled() {
  return base::FeatureList::IsEnabled(kEnableLibAssistantBetaBackend);
}

bool IsStereoAudioInputEnabled() {
  return base::FeatureList::IsEnabled(kEnableStereoAudioInput) ||
         // Audio eraser requires 2 channel input.
         base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsVoiceMatchDisabled() {
  return base::FeatureList::IsEnabled(kDisableVoiceMatch);
}

bool IsWaitSchedulingEnabled() {
  return base::FeatureList::IsEnabled(kAssistantWaitScheduling);
}

bool IsLibAssistantSandboxEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      sandbox::policy::switches::kNoSandbox);
}

bool IsLibAssistantDLCEnabled() {
  return base::FeatureList::IsEnabled(kEnableLibAssistantDLC);
}

bool IsOnboardingEnabled() {
  return base::FeatureList::IsEnabled(kEnableAssistantOnboarding);
}

}  // namespace ash::assistant::features
