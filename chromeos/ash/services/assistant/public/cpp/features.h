// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash::assistant::features {

// Enable Assistant Feedback UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAudioEraser;

// Enables Assistant app support.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAppSupport;

// Enables Assistant routines.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantRoutines;

// Enables server-driven wait scheduling. This allows the server to inject
// pauses into the interaction response to give the user time to digest one leg
// of a routine before proceeding to the next, for example, or to provide
// comedic timing for jokes.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantWaitScheduling;

// Enables DSP for hotword detection.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableDspHotword;

// Enables stereo audio input.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableStereoAudioInput;

// Enables power management features i.e. Wake locks and wake up alarms.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnablePowerManager;

// Uses the LibAssistant beta backend instead of the release channel.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableLibAssistantBetaBackend;

// Enables the sandbox of LibAssistant service.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableLibAssistantSandbox;

// Enables the LibAssistantV2 APIs and related features.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableLibAssistantV2;

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableLibAssistantDlc;

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAppSupportEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAudioEraserEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsAssistantDebuggingEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsDspHotwordEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsPowerManagerEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsLibAssistantBetaBackendEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsRoutinesEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsStereoAudioInputEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsVoiceMatchDisabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsWaitSchedulingEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsLibAssistantSandboxEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsLibAssistantV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsLibAssistantDlcEnabled();

}  // namespace ash::assistant::features

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::assistant {
namespace features = ::ash::assistant::features;
}

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
