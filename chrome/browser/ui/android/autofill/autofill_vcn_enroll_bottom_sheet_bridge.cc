// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/AutofillVCNEnrollBottomSheetBridge_jni.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AutofillVCNEnrollBottomSheetBridge::AutofillVCNEnrollBottomSheetBridge()
    : java_bridge_(Java_AutofillVCNEnrollBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread())) {}

AutofillVCNEnrollBottomSheetBridge::~AutofillVCNEnrollBottomSheetBridge() =
    default;

bool AutofillVCNEnrollBottomSheetBridge::RequestShowContent(
    content::WebContents* web_contents,
    std::unique_ptr<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>
        delegate) {
  if (!web_contents) {
    return false;
  }

  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      web_contents->GetJavaWebContents();
  if (!java_web_contents) {
    return false;
  }

  return Java_AutofillVCNEnrollBottomSheetBridge_requestShowContent(
      base::android::AttachCurrentThread(), java_bridge_,
      reinterpret_cast<jlong>(this), java_web_contents);
}

}  // namespace autofill
