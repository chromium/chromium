// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/AutofillVcnEnrollBottomSheetBridge_jni.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AutofillVCNEnrollBottomSheetBridge::AutofillVCNEnrollBottomSheetBridge()
    : java_bridge_(Java_AutofillVcnEnrollBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread())) {}

AutofillVCNEnrollBottomSheetBridge::~AutofillVCNEnrollBottomSheetBridge() {
  Java_AutofillVcnEnrollBottomSheetBridge_hide(
      base::android::AttachCurrentThread(), java_bridge_);
}

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

  delegate_ = std::move(delegate);

  return Java_AutofillVcnEnrollBottomSheetBridge_requestShowContent(
      base::android::AttachCurrentThread(), java_bridge_,
      reinterpret_cast<jlong>(this), java_web_contents);
}

void AutofillVCNEnrollBottomSheetBridge::OnDismiss(JNIEnv* env) {
  delegate_->InfoBarDismissed();
}

}  // namespace autofill
