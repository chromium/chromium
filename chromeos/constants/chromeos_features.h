// Copyright 2021 The Chromium Authors
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

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file. If a feature is
// being rolled out via Finch, add a comment in the .cc file.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kBluetoothPhoneFilter);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) BASE_DECLARE_FEATURE(kCloudGamingDevice);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) BASE_DECLARE_FEATURE(kDarkLightMode);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) BASE_DECLARE_FEATURE(kDemoModeSWA);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kDisableIdleSocketsCloseOnMemoryPressure);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kDisableOfficeEditingComponentApp);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kDisableQuickAnswersV2Translation);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) BASE_DECLARE_FEATURE(kJellyroll);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kPasswordManagerSystemAuthentication);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kQuickAnswersRichCard);
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
BASE_DECLARE_FEATURE(kQuickAnswersV2SettingsSubToggle);

// Keep alphabetized.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsDarkLightModeEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsCloudGamingDeviceEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsDemoModeSWAEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsJellyrollEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsPasswordManagerSystemAuthenticationEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) bool IsQuickAnswersV2TranslationDisabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersV2SettingsSubToggleEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersRichCardEnabled();
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
bool IsQuickAnswersAlwaysTriggerForSingleWord();

}  // namespace features
}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
