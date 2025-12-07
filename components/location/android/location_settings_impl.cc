// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/location/android/location_settings_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "base/functional/bind.h"
#include "components/location/android/location_settings_dialog_outcome.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/location/android/jni_headers/LocationSettings_jni.h"

using base::android::AttachCurrentThread;

using LocationSettingsDialogOutcomeCallback =
    LocationSettings::LocationSettingsDialogOutcomeCallback;

LocationSettingsImpl::LocationSettingsImpl() = default;

LocationSettingsImpl::~LocationSettingsImpl() = default;

bool LocationSettingsImpl::HasAndroidLocationPermission() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_hasAndroidLocationPermission(env);
}

bool LocationSettingsImpl::HasAndroidFineLocationPermission() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_hasAndroidFineLocationPermission(env);
}

bool LocationSettingsImpl::CanPromptForAndroidLocationPermission(
    ui::WindowAndroid* window) {
  if (window == nullptr)
    return false;
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_canPromptForAndroidLocationPermission(
      env, window->GetJavaObject());
}

bool LocationSettingsImpl::IsSystemLocationSettingEnabled() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_isSystemLocationSettingEnabled(env);
}

bool LocationSettingsImpl::CanPromptToEnableSystemLocationSetting() {
  JNIEnv* env = AttachCurrentThread();
  return Java_LocationSettings_canPromptToEnableSystemLocationSetting(env);
}

void LocationSettingsImpl::PromptToEnableSystemLocationSetting(
    const LocationSettingsDialogContext prompt_context,
    ui::WindowAndroid* window,
    LocationSettingsDialogOutcomeCallback callback) {
  if (window == nullptr) {
    std::move(callback).Run(LocationSettingsDialogOutcome::NO_PROMPT);
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  // Convert the C++ callback to a JNI callback using ToJniCallback.
  Java_LocationSettings_promptToEnableSystemLocationSetting(
      env, prompt_context, window->GetJavaObject(),
      base::android::ToJniCallback(
          env,
          base::BindOnce(
              [](LocationSettingsDialogOutcomeCallback callback, int result) {
                std::move(callback).Run(
                    static_cast<LocationSettingsDialogOutcome>(result));
              },
              std::move(callback))));
}

DEFINE_JNI(LocationSettings)
