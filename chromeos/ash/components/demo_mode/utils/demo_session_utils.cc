// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::demo_mode {

namespace {
bool g_should_fall_back_mgs = false;
}

bool IsDeviceInDemoMode() {
  if (!InstallAttributes::IsInitialized()) {
    // TODO(b/281905036): Add a log to indicate that the install
    // attributes haven't been initialized yet.
    return false;
  }

  return InstallAttributes::Get()->IsDeviceInDemoMode();
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Demo session prefs:
  registry->RegisterStringPref(prefs::kDemoModeDefaultLocale, std::string());
  registry->RegisterStringPref(prefs::kDemoModeCountry, kSupportedCountries[0]);
  registry->RegisterStringPref(prefs::kDemoModeRetailerId, std::string());
  registry->RegisterStringPref(prefs::kDemoModeStoreId, std::string());
  registry->RegisterStringPref(prefs::kDemoModeAppVersion, std::string());
  registry->RegisterStringPref(prefs::kDemoModeResourcesVersion, std::string());

  // TODO(crbugs.com/366092466): Use DemoSession::DemoModeConfig::kNone once
  // finishes refactoring. Demo mode setup controller prefs:
  registry->RegisterIntegerPref(prefs::kDemoModeConfig, 0);

  // Demo login controller prefs:
  registry->RegisterStringPref(prefs::kDemoAccountGaiaId, std::string());
  registry->RegisterStringPref(prefs::kDemoModeSessionIdentifier,
                               std::string());
}

bool ShouldFallBackToMGS() {
  // Always fall back to MGS if demo mode sign in not enable.
  return !features::IsDemoModeSignInEnabled() || g_should_fall_back_mgs;
}

void SetShouldFallBackMGS(bool should_fall_back_mgs) {
  g_should_fall_back_mgs = should_fall_back_mgs;
}

}  // namespace ash::demo_mode
