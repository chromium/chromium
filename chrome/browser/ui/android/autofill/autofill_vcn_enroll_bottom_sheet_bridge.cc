// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/payments/legal_message_line_android.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillVcnEnrollBottomSheetBridge_jni.h"

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

  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_AutofillVcnEnrollBottomSheetBridge_requestShowContent(
      env, java_bridge_, reinterpret_cast<jlong>(this), java_web_contents,
      delegate_->GetMessageText(), delegate_->GetDescriptionText(),
      delegate_->GetLearnMoreLinkText(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION,
          delegate_->GetCardLabel()),
      gfx::ConvertToJavaBitmap(*delegate_->GetIssuerIcon()->bitmap()),
      delegate_->GetCardLabel(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX),
      LegalMessageLineAndroid::ConvertToJavaLinkedList(
          delegate_->GetGoogleLegalMessage()),
      LegalMessageLineAndroid::ConvertToJavaLinkedList(
          delegate_->GetIssuerLegalMessage()),
      delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
      delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_LOADING_THROBBER_ACCESSIBLE_NAME));
}

void AutofillVCNEnrollBottomSheetBridge::Hide() {
  Java_AutofillVcnEnrollBottomSheetBridge_hide(
      base::android::AttachCurrentThread(), java_bridge_);
}

void AutofillVCNEnrollBottomSheetBridge::OnAccept(JNIEnv* env) {
  delegate_->Accept();
}

void AutofillVCNEnrollBottomSheetBridge::OnCancel(JNIEnv* env) {
  delegate_->Cancel();
}

void AutofillVCNEnrollBottomSheetBridge::OnDismiss(JNIEnv* env) {
  delegate_->InfoBarDismissed();
}

void AutofillVCNEnrollBottomSheetBridge::RecordLinkClickMetric(JNIEnv* env,
                                                               int link_type) {
  LogVirtualCardEnrollmentLinkClickedMetric(
      static_cast<VirtualCardEnrollmentLinkType>(link_type),
      delegate_->GetVirtualCardEnrollmentBubbleSource());
}

}  // namespace autofill
