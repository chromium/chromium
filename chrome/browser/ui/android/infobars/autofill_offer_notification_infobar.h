// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_OFFER_NOTIFICATION_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_OFFER_NOTIFICATION_INFOBAR_H_

#include <jni.h>

#include <memory>

#include "components/infobars/android/confirm_infobar.h"
namespace autofill {
class AutofillOfferNotificationInfoBarDelegateMobile;
}

// Android implementation of the infobar for showing offer notifications on a
// web page when an offer exists for that merchant.
class AutofillOfferNotificationInfoBar : public infobars::ConfirmInfoBar {
 public:
  explicit AutofillOfferNotificationInfoBar(
      std::unique_ptr<autofill::AutofillOfferNotificationInfoBarDelegateMobile>
          delegate);

  ~AutofillOfferNotificationInfoBar() override;

  AutofillOfferNotificationInfoBar(const AutofillOfferNotificationInfoBar&) =
      delete;
  AutofillOfferNotificationInfoBar& operator=(
      const AutofillOfferNotificationInfoBar&) = delete;

  // Called when a link in the legal message text was clicked.
  void OnOfferDeepLinkClicked(JNIEnv* env,
                              jobject obj,
                              const base::android::JavaParamRef<jobject>& url);

 private:
  // infobars::ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  // Returns the infobar delegate.
  autofill::AutofillOfferNotificationInfoBarDelegateMobile*
  GetOfferNotificationDelegate();
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_AUTOFILL_OFFER_NOTIFICATION_INFOBAR_H_
