// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/feature_list.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos::features {

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables cloud game features. A separate flag "LauncherGameSearch" controls
// launcher-only cloud gaming features, since they can also be enabled on
// non-cloud-gaming devices.
const base::Feature kCloudGamingDevice{"CloudGamingDevice",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables dark/light mode feature.
const base::Feature kDarkLightMode{"DarkLightMode",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the ChromeOS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
const base::Feature kDisableIdleSocketsCloseOnMemoryPressure{
    "disable_idle_sockets_close_on_memory_pressure",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const base::Feature kDisableOfficeEditingComponentApp{
    "DisableOfficeEditingComponentApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Disables translation services of the Quick Answers V2.
const base::Feature kDisableQuickAnswersV2Translation{
    "DisableQuickAnswersV2Translation", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers V2 settings sub-toggles.
const base::Feature kQuickAnswersV2SettingsSubToggle{
    "QuickAnswersV2SettingsSubToggle", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Quick Answers for more locales.
const base::Feature kQuickAnswersForMoreLocales{
    "QuickAnswersForMoreLocales", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsCloudGamingDeviceEnabled() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsCloudGamingDevice();
#else
  return base::FeatureList::IsEnabled(kCloudGamingDevice);
#endif
}

bool IsDarkLightModeEnabled() {
  return base::FeatureList::IsEnabled(kDarkLightMode);
}

bool IsQuickAnswersV2TranslationDisabled() {
  return base::FeatureList::IsEnabled(kDisableQuickAnswersV2Translation);
}

bool IsQuickAnswersV2SettingsSubToggleEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersV2SettingsSubToggle);
}

bool IsQuickAnswersForMoreLocalesEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersForMoreLocales);
}

}  // namespace chromeos::features
