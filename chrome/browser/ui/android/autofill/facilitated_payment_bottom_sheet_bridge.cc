// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/facilitated_payment_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/FacilitatedPaymentBottomSheetBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

FacilitatedPaymentBottomSheetBridge::FacilitatedPaymentBottomSheetBridge()
    : java_bridge_(Java_FacilitatedPaymentBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread())) {}

FacilitatedPaymentBottomSheetBridge::~FacilitatedPaymentBottomSheetBridge() = default;

bool FacilitatedPaymentBottomSheetBridge::RequestShowContent(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents->GetJavaWebContents();
  if (!java_web_contents) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_FacilitatedPaymentBottomSheetBridge_requestShowContent(env, java_bridge_,
      java_web_contents);
}

}  // namespace autofill
