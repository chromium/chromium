// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that declare env/types.
#include "chrome/browser/autofill/android/jni_headers/AutofillPaymentsChurnedUsersBottomSheetBridge_jni.h"

namespace autofill {

AutofillPaymentsChurnedUsersBottomSheetBridge::
    AutofillPaymentsChurnedUsersBottomSheetBridge(
        ui::WindowAndroid* window_android) {
  if (window_android && window_android->GetJavaObject()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_object_ =
        Java_AutofillPaymentsChurnedUsersBottomSheetBridge_Constructor(
            env, window_android->GetJavaObject());
  }
}

AutofillPaymentsChurnedUsersBottomSheetBridge::
    ~AutofillPaymentsChurnedUsersBottomSheetBridge() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutofillPaymentsChurnedUsersBottomSheetBridge_destroy(env,
                                                               java_object_);
  }
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::RequestShowContent() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutofillPaymentsChurnedUsersBottomSheetBridge_requestShowContent(
        env, java_object_);
  }
}

}  // namespace autofill
