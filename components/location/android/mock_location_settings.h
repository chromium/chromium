// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCATION_ANDROID_MOCK_LOCATION_SETTINGS_H_
#define COMPONENTS_LOCATION_ANDROID_MOCK_LOCATION_SETTINGS_H_

#include "components/location/android/location_settings.h"
#include "components/location/android/location_settings_dialog_context.h"
#include "components/location/android/location_settings_dialog_outcome.h"

// Mock implementation of LocationSettings for unit tests.
class MockLocationSettings : public LocationSettings {
 public:
  MockLocationSettings();

  MockLocationSettings(const MockLocationSettings&) = delete;
  MockLocationSettings& operator=(const MockLocationSettings&) = delete;

  ~MockLocationSettings() override;

  static void SetLocationStatus(bool has_android_coarse_location_permission,
                                bool has_android_fine_location_permission,
                                bool is_system_location_setting_enabled);
  static void SetCanPromptForAndroidPermission(bool can_prompt);
  static void SetLocationSettingsDialogStatus(
      bool enabled,
      LocationSettingsDialogOutcome outcome);
  static bool HasShownLocationSettingsDialog();
  static void ClearHasShownLocationSettingsDialog();

  static void SetAsyncLocationSettingsDialog();
  static void ResolveAsyncLocationSettingsDialog();

  // LocationSettings implementation:
  bool HasAndroidLocationPermission() override;
  bool HasAndroidFineLocationPermission() override;
  bool CanPromptForAndroidLocationPermission(
      ui::WindowAndroid* window) override;
  bool IsSystemLocationSettingEnabled() override;
  bool CanPromptToEnableSystemLocationSetting() override;
  void PromptToEnableSystemLocationSetting(
      const LocationSettingsDialogContext prompt_context,
      ui::WindowAndroid* window,
      LocationSettingsDialogOutcomeCallback callback) override;
};

#endif  // COMPONENTS_LOCATION_ANDROID_MOCK_LOCATION_SETTINGS_H_
