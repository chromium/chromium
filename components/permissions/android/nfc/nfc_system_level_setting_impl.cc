// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/nfc/nfc_system_level_setting_impl.h"

#include "base/android/jni_android.h"
#include "components/permissions/android/jni_headers/NfcSystemLevelSetting_jni.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

NfcSystemLevelSettingImpl::NfcSystemLevelSettingImpl() {}

NfcSystemLevelSettingImpl::~NfcSystemLevelSettingImpl() {}

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
  // Transfers the ownership of the callback to the Java callback. The Java
  // callback is guaranteed to be called unless the user never replies to the
  // dialog, and the callback pointer will be destroyed in
  // NfcSystemLevelPrompt.onDismiss.
  auto* callback_ptr =
      new base::OnceClosure(std::move(prompt_completed_callback));
  Java_NfcSystemLevelSetting_promptToEnableNfcSystemLevelSetting(
      env, web_contents->GetJavaWebContents(),
      reinterpret_cast<jlong>(callback_ptr));
}

}  // namespace permissions

static void JNI_NfcSystemLevelSetting_OnNfcSystemLevelPromptCompleted(
    JNIEnv* env,
    jlong callback_ptr) {
  auto* callback = reinterpret_cast<base::OnceClosure*>(callback_ptr);
  std::move(*callback).Run();
  // Destroy the callback whose ownership was transferred in
  // PromptToEnableNfcSystemLevelSetting.
  delete callback;
}
