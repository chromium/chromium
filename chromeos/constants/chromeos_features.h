// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

// This file is only for the feature flags that are shared between ash-chrome
// and lacros-chrome that are not common. For ash features, please add them
// in //ash/constants/ash_features.h.
namespace chromeos {

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kLacrosTtsSupport;

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothAdvertisementMonitoring;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBluetoothPhoneFilter;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDarkLightMode;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kDisableQuickAnswersV2Translation;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersV2SettingsSubToggle;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersAlwaysTriggerForSingleWord;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kQuickAnswersForMoreLocales;

// Keep alphabetized.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsBluetoothAdvertisementMonitoringEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsDarkLightModeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersV2TranslationDisabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersV2SettingsSubToggleEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersAlwaysTriggerForSingleWord();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersForMoreLocalesEnabled();

}  // namespace features
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
