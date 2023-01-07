// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_H_

#include <jni.h>
#include <memory>

#include "components/infobars/android/confirm_infobar.h"

namespace autofill {
class AutofillVirtualCardEnrollmentInfoBarDelegateMobile;
}

// Android implementation of the infobar for virtual card enrollment
// information.
class AutofillVirtualCardEnrollmentInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit AutofillVirtualCardEnrollmentInfoBar(
      std::unique_ptr<
          autofill::AutofillVirtualCardEnrollmentInfoBarDelegateMobile>
          delegate);
  AutofillVirtualCardEnrollmentInfoBar(
      const AutofillVirtualCardEnrollmentInfoBar&) = delete;
  AutofillVirtualCardEnrollmentInfoBar& operator=(
      const AutofillVirtualCardEnrollmentInfoBar&) = delete;
  ~AutofillVirtualCardEnrollmentInfoBar() override;

  // Called when a link in the infobar text was clicked.
  void OnInfobarLinkClicked(JNIEnv* env,
                            jobject obj,
                            jstring url,
                            jint link_type);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  // Pointer to the virtual card enrollment delegate.
  raw_ptr<autofill::AutofillVirtualCardEnrollmentInfoBarDelegateMobile>
      virtual_card_enrollment_delegate_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_INFOBAR_H_
