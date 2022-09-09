// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_SAVE_CARD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_SAVE_CARD_INFOBAR_H_

#include <jni.h>

#include <memory>

#include "components/infobars/android/confirm_infobar.h"

namespace autofill {
class AutofillSaveCardInfoBarDelegateMobile;
}

// Android implementation of the infobar for saving credit card information.
class AutofillSaveCardInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit AutofillSaveCardInfoBar(
      std::unique_ptr<autofill::AutofillSaveCardInfoBarDelegateMobile>
          delegate);

  AutofillSaveCardInfoBar(const AutofillSaveCardInfoBar&) = delete;
  AutofillSaveCardInfoBar& operator=(const AutofillSaveCardInfoBar&) = delete;

  ~AutofillSaveCardInfoBar() override;

  // Called when a link in the legal message text was clicked.
  void OnLegalMessageLinkClicked(JNIEnv* env, jobject obj, jstring url);

 private:
  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  // Returns the infobar delegate.
  autofill::AutofillSaveCardInfoBarDelegateMobile* GetSaveCardDelegate();

  // Returns Google Pay branding icon id. Keeping this method here instead
  // of autofill_save_card_infobar_delegate_mobile.cc as Android icon .xmls
  // are stored in /chrome and /components cannot depend on /chrome.
  int GetGooglePayBrandingIconId();
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_SAVE_CARD_INFOBAR_H_
