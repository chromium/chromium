// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/nfc/nfc_system_level_setting_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/jni_headers/NfcSystemLevelSetting_jni.h"

namespace permissions {

NfcSystemLevelSettingImpl::NfcSystemLevelSettingImpl() = default;

NfcSystemLevelSettingImpl::~NfcSystemLevelSettingImpl() = default;

bool NfcSystemLevelSettingImpl::IsNfcAccessPossible() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_NfcSystemLevelSetting_isNfcAccessPossible(env);
}

bool NfcSystemLevelSettingImpl::IsNfcSystemLevelSettingEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_NfcSystemLevelSetting_isNfcSystemLevelSettingEnabled(env);
}

void NfcSystemLevelSettingImpl::PromptToEnableNfcSystemLevelSetting(
    content::WebContents* web_contents,
    base::OnceClosure prompt_completed_callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Convert the C++ callback to a JNI callback using ToJniCallback.
  Java_NfcSystemLevelSetting_promptToEnableNfcSystemLevelSetting(
      env, web_contents->GetJavaWebContents(),
      base::android::ToJniCallback(env, std::move(prompt_completed_callback)));
}

}  // namespace permissions

DEFINE_JNI(NfcSystemLevelSetting)
