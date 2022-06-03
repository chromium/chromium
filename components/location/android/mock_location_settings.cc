// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/location/android/mock_location_settings.h"

#include "base/lazy_instance.h"

namespace {

static bool has_android_location_permission_ = false;
static bool is_system_location_setting_enabled_ = false;
static bool can_prompt_for_android_location_permission_ = false;
static bool location_settings_dialog_enabled_ = false;
static LocationSettingsDialogOutcome location_settings_dialog_outcome_ =
    NO_PROMPT;
static bool has_shown_location_settings_dialog_ = false;
static bool resolve_location_settings_dialog_async_ = false;
static base::LazyInstance<
    LocationSettings::LocationSettingsDialogOutcomeCallback>::Leaky
    location_settings_dialog_callback_ = LAZY_INSTANCE_INITIALIZER;

}  // namespace

MockLocationSettings::MockLocationSettings() : LocationSettings() {}

MockLocationSettings::~MockLocationSettings() = default;

void MockLocationSettings::SetLocationStatus(
    bool has_android_location_permission,
    bool is_system_location_setting_enabled) {
  has_android_location_permission_ = has_android_location_permission;
  is_system_location_setting_enabled_ = is_system_location_setting_enabled;
}

void MockLocationSettings::SetCanPromptForAndroidPermission(bool can_prompt) {
  can_prompt_for_android_location_permission_ = can_prompt;
}

void MockLocationSettings::SetLocationSettingsDialogStatus(
    bool enabled,
    LocationSettingsDialogOutcome outcome) {
  location_settings_dialog_enabled_ = enabled;
  location_settings_dialog_outcome_ = outcome;
}

bool MockLocationSettings::HasShownLocationSettingsDialog() {
  return has_shown_location_settings_dialog_;
}

void MockLocationSettings::ClearHasShownLocationSettingsDialog() {
  has_shown_location_settings_dialog_ = false;
}

void MockLocationSettings::SetAsyncLocationSettingsDialog() {
  resolve_location_settings_dialog_async_ = true;
}

void MockLocationSettings::ResolveAsyncLocationSettingsDialog() {
  DCHECK(!location_settings_dialog_callback_.Get().is_null());
  std::move(location_settings_dialog_callback_.Get())
      .Run(location_settings_dialog_outcome_);
}

bool MockLocationSettings::HasAndroidLocationPermission() {
  return has_android_location_permission_;
}

bool MockLocationSettings::CanPromptForAndroidLocationPermission(
    ui::WindowAndroid* window) {
  return can_prompt_for_android_location_permission_;
}

bool MockLocationSettings::IsSystemLocationSettingEnabled() {
  return is_system_location_setting_enabled_;
}

bool MockLocationSettings::CanPromptToEnableSystemLocationSetting() {
  return location_settings_dialog_enabled_;
}

void MockLocationSettings::PromptToEnableSystemLocationSetting(
    const LocationSettingsDialogContext prompt_context,
    ui::WindowAndroid* window,
    LocationSettingsDialogOutcomeCallback callback) {
  has_shown_location_settings_dialog_ = true;

  if (resolve_location_settings_dialog_async_) {
    location_settings_dialog_callback_.Get() = std::move(callback);
  } else {
    std::move(callback).Run(location_settings_dialog_outcome_);
  }
}
