// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_H_

#include <jni.h>

#include <memory>

#include "components/infobars/android/confirm_infobar.h"

namespace autofill {
class AutofillCreditCardFillingInfoBarDelegateMobile;
}

// Android implementation of the infobar for credit card assisted filling, which
// proposes to autofill user data into the detected credit card form in the
// page. Upon accepting the infobar, the form is filled automatically. If
// the infobar is dismissed, nothing happens.
class AutofillCreditCardFillingInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit AutofillCreditCardFillingInfoBar(
      std::unique_ptr<autofill::AutofillCreditCardFillingInfoBarDelegateMobile>
          delegate);

  AutofillCreditCardFillingInfoBar(const AutofillCreditCardFillingInfoBar&) =
      delete;
  AutofillCreditCardFillingInfoBar& operator=(
      const AutofillCreditCardFillingInfoBar&) = delete;

  ~AutofillCreditCardFillingInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_H_
