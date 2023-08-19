// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_H_
#define COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_H_

#include "base/functional/callback.h"
#include "components/location/android/location_settings_dialog_context.h"
#include "components/location/android/location_settings_dialog_outcome.h"

namespace ui {
class WindowAndroid;
}

// This class determines whether Chrome can access the device's location,
// i.e. whether location is enabled system-wide on the device.
class LocationSettings {
 public:
  virtual ~LocationSettings() = default;

  // Returns true if Chrome has any permission to access location.
  virtual bool HasAndroidLocationPermission() = 0;

  // Returns true if Chrome has permission to access precise location.
  virtual bool HasAndroidFineLocationPermission() = 0;

  // Returns true if Chrome can prompt to get location permission.
  virtual bool CanPromptForAndroidLocationPermission(
      ui::WindowAndroid* window) = 0;

  // Returns true if the system location is enabled.
  virtual bool IsSystemLocationSettingEnabled() = 0;

  // Returns true iff a prompt can be triggered to ask the user to turn on the
  // system location setting on their device.
  // In particular, returns false if the system location setting is already
  // enabled or if some of the features required to trigger a system location
  // setting prompt are not available.
  virtual bool CanPromptToEnableSystemLocationSetting() = 0;

  typedef base::OnceCallback<void(LocationSettingsDialogOutcome)>
      LocationSettingsDialogOutcomeCallback;

  // Triggers a prompt to ask the user to turn on the system location setting on
  // their device, and returns the outcome of the prompt via a callback.
  //
  // The prompt will be triggered in the activity of the web contents.
  //
  // The callback is guaranteed to be called unless the user never replies to
  // the prompt dialog, which in practice happens very infrequently since the
  // dialog is modal.
  //
  // The callback may be invoked a long time after this method has returned.
  // If you need to access in the callback an object that is not owned by the
  // callback, you should ensure that the object has not been destroyed before
  // accessing it to prevent crashes, e.g. by using weak pointer semantics.
  virtual void PromptToEnableSystemLocationSetting(
      const LocationSettingsDialogContext prompt_context,
      ui::WindowAndroid* window,
      LocationSettingsDialogOutcomeCallback callback) = 0;
};

#endif  // COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_H_
