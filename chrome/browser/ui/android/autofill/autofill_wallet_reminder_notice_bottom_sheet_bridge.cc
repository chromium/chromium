// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_wallet_reminder_notice_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "components/autofill/android/payments/legal_message_line_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that declare env/types.
#include "chrome/browser/autofill/android/jni_headers/AutofillWalletReminderNoticeBottomSheetBridge_jni.h"

namespace autofill {

AutofillWalletReminderNoticeBottomSheetBridge::
    AutofillWalletReminderNoticeBottomSheetBridge(
        ui::WindowAndroid* window_android) {
  if (window_android && window_android->GetJavaObject()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_object_ =
        Java_AutofillWalletReminderNoticeBottomSheetBridge_Constructor(
            env, window_android->GetJavaObject());
  }
}

AutofillWalletReminderNoticeBottomSheetBridge::
    ~AutofillWalletReminderNoticeBottomSheetBridge() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutofillWalletReminderNoticeBottomSheetBridge_destroy(env,
                                                               java_object_);
  }
}

void AutofillWalletReminderNoticeBottomSheetBridge::RequestShowContent(
    LegalMessageLines legal_message_lines) {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutofillWalletReminderNoticeBottomSheetBridge_requestShowContent(
        env, java_object_,
        LegalMessageLineAndroid::ConvertToJavaLinkedList(legal_message_lines));
  }
}

}  // namespace autofill
