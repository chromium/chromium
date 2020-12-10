// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_H_

#include <jni.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/chrome_confirm_infobar.h"

namespace autofill {
class AutofillCreditCardFillingInfoBarDelegateMobile;
}

// Android implementation of the infobar for credit card assisted filling, which
// proposes to autofill user data into the detected credit card form in the
// page. Upon accepting the infobar, the form is filled automatically. If
// the infobar is dismissed, nothing happens.
class AutofillCreditCardFillingInfoBar : public ChromeConfirmInfoBar {
 public:
  explicit AutofillCreditCardFillingInfoBar(
      std::unique_ptr<autofill::AutofillCreditCardFillingInfoBarDelegateMobile>
          delegate);
  ~AutofillCreditCardFillingInfoBar() override;

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardFillingInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_CREDIT_CARD_FILLING_INFOBAR_H_
