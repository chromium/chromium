// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_FACILITATED_PAYMENT_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_FACILITATED_PAYMENT_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/android/window_android.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Bridge class providing an entry point to trigger the facilitated payment
// bottom sheet on Android.
class FacilitatedPaymentBottomSheetBridge {
 public:
  FacilitatedPaymentBottomSheetBridge();

  FacilitatedPaymentBottomSheetBridge(
      const FacilitatedPaymentBottomSheetBridge&) = delete;
  FacilitatedPaymentBottomSheetBridge& operator=(
      const FacilitatedPaymentBottomSheetBridge&) = delete;

  ~FacilitatedPaymentBottomSheetBridge();

  bool RequestShowContent(content::WebContents* web_contents);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_FACILITATED_PAYMENT_BOTTOM_SHEET_BRIDGE_H_
