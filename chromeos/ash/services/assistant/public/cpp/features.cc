// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

const base::Feature kAssistantAudioEraser{"AssistantAudioEraser",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantAppSupport{"AssistantAppSupport",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantDebugging{"AssistantDebugging",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantRoutines{"AssistantRoutines",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantWaitScheduling{"AssistantWaitScheduling",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableDspHotword{"EnableDspHotword",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStereoAudioInput{"AssistantEnableStereoAudioInput",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnablePowerManager{"ChromeOSAssistantEnablePowerManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableLibAssistantBetaBackend{
    "LibAssistantBetaBackend", base::FEATURE_DISABLED_BY_DEFAULT};

// Disable voice match for test purpose.
const base::Feature kDisableVoiceMatch{"DisableVoiceMatch",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableLibAssistantSandbox{
    "LibAssistantSandbox", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableLibAssistantV2{"LibAssistantV2",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

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

bool IsRoutinesEnabled() {
  return base::FeatureList::IsEnabled(kAssistantRoutines);
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
  return base::FeatureList::IsEnabled(kEnableLibAssistantSandbox);
}

bool IsLibAssistantV2Enabled() {
  return base::FeatureList::IsEnabled(kEnableLibAssistantV2);
}

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
