// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_WALLET_REMINDER_NOTICE_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_WALLET_REMINDER_NOTICE_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace ui {
class WindowAndroid;
}

namespace autofill {

// Bridge class to trigger the Wallet Reminder Notice bottom sheet on Android.
class AutofillWalletReminderNoticeBottomSheetBridge {
 public:
  explicit AutofillWalletReminderNoticeBottomSheetBridge(
      ui::WindowAndroid* window_android);

  AutofillWalletReminderNoticeBottomSheetBridge(
      const AutofillWalletReminderNoticeBottomSheetBridge&) = delete;
  AutofillWalletReminderNoticeBottomSheetBridge& operator=(
      const AutofillWalletReminderNoticeBottomSheetBridge&) = delete;

  virtual ~AutofillWalletReminderNoticeBottomSheetBridge();

  // Requests to show the bottom sheet notice.
  virtual void RequestShowContent();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_WALLET_REMINDER_NOTICE_BOTTOM_SHEET_BRIDGE_H_
