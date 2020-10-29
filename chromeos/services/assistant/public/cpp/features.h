// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace chromeos {
namespace assistant {
namespace features {

// Enable Assistant Feedback UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAudioEraser;

// Enables Assistant app support.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAppSupport;

// Enables better onboarding for Assistant.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantBetterOnboarding;

// Enables Assistant launcher chip integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantLauncherChipIntegration;

// When enabled, Assistant will use response processing V2. This is a set of
// synced client and server changes which will turn on default parallel client
// op processing and eager (streaming) UI element rendering.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantResponseProcessingV2;

// Enables Assistant routines.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantRoutines;

// When enabled, we support the second version of timers UX which includes new
// UI treatments for timers in Assistant and System UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantTimersV2;

// Enables server-driven wait scheduling. This allows the server to inject
// pauses into the interaction response to give the user time to digest one leg
// of a routine before proceeding to the next, for example, or to provide
// comedic timing for jokes.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantWaitScheduling;

// Enables Assistant in Ambient Mode.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableAmbientAssistant;

// Enables Better Assistant.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableBetterAssistant;

// Enables Bloom integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableBloom;

// Enables DSP for hotword detection.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableDspHotword;

// Enables MediaSession Integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableMediaSessionIntegration;

// Enables stereo audio input.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableStereoAudioInput;

// Enables power management features i.e. Wake locks and wake up alarms.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnablePowerManager;

// Enables on-device-assistant to handle the most common queries on device.
// See go/marble
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableOnDeviceAssistant;

// Uses the LibAssistant beta backend instead of the release channel.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableLibAssistantBetaBackend;

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAmbientAssistantEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAppSupportEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAudioEraserEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsBetterAssistantEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsBetterOnboardingEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsBloomEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsConversationStartersV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsAssistantDebuggingEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsDspHotwordEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsLauncherChipIntegrationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsMediaSessionIntegrationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsPowerManagerEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsOnDeviceAssistantEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsLibAssistantBetaBackendEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsResponseProcessingV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsRoutinesEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsStereoAudioInputEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsTimersV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsVoiceMatchDisabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsWaitSchedulingEnabled();

}  // namespace features
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
