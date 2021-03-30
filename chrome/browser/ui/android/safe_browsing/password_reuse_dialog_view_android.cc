// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/safe_browsing/password_reuse_dialog_view_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/SafeBrowsingPasswordReuseDialogBridge_jni.h"
#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"
#include "ui/android/window_android.h"

namespace safe_browsing {

PasswordReuseDialogViewAndroid::PasswordReuseDialogViewAndroid(
    PasswordReuseControllerAndroid* controller)
    : controller_(controller) {}

PasswordReuseDialogViewAndroid::~PasswordReuseDialogViewAndroid() {
  Java_SafeBrowsingPasswordReuseDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordReuseDialogViewAndroid::Show(ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_SafeBrowsingPasswordReuseDialogBridge_create(
      env, window_android->GetJavaObject(), reinterpret_cast<intptr_t>(this)));

  std::vector<size_t> placeholder_offsets;
  std::u16string warning_detail_text =
      controller_->GetWarningDetailText(&placeholder_offsets);

  const std::vector<std::u16string> placeholders =
      controller_->GetPlaceholdersForSavedPasswordWarningText();

  DCHECK_EQ(placeholder_offsets.size(), placeholders.size());

  int len = placeholder_offsets.size();
  int start_ranges[len], end_ranges[len];

  for (int i = 0; i < len; i++) {
    start_ranges[i] = placeholder_offsets[i];
    end_ranges[i] = placeholder_offsets[i] + placeholders[i].length();
  }

  base::android::ScopedJavaLocalRef<jintArray> j_start_ranges =
      base::android::ToJavaIntArray(env, start_ranges, len);
  base::android::ScopedJavaLocalRef<jintArray> j_end_ranges =
      base::android::ToJavaIntArray(env, end_ranges, len);

  Java_SafeBrowsingPasswordReuseDialogBridge_showDialog(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, controller_->GetTitle()),
      base::android::ConvertUTF16ToJavaString(env, warning_detail_text),
      base::android::ConvertUTF16ToJavaString(env,
                                              controller_->GetButtonText()),
      j_start_ranges, j_end_ranges);
}

void PasswordReuseDialogViewAndroid::Close(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->CloseDialog();
}

}  // namespace safe_browsing
