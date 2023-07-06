// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_IMPL_H_
#define COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_IMPL_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/location/android/location_settings.h"

class LocationSettingsImpl : public LocationSettings {
 public:
  LocationSettingsImpl();

  LocationSettingsImpl(const LocationSettingsImpl&) = delete;
  LocationSettingsImpl& operator=(const LocationSettingsImpl&) = delete;

  ~LocationSettingsImpl() override;

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

#endif  // COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_IMPL_H_
