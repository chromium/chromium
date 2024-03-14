// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_CONTROLLER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class AutofillOfferData;

// Per-tab controller to control the offer notification message displayed on
// mobile.
class OfferNotificationControllerAndroid
    : public content::WebContentsUserData<OfferNotificationControllerAndroid> {
 public:
  explicit OfferNotificationControllerAndroid(
      content::WebContents* web_contents);
  ~OfferNotificationControllerAndroid() override;

  OfferNotificationControllerAndroid(
      const OfferNotificationControllerAndroid&) = delete;
  OfferNotificationControllerAndroid& operator=(
      const OfferNotificationControllerAndroid&) = delete;

  // Show the message unless it was already shown in the same tab with the same
  // origin.
  void ShowIfNecessary(const AutofillOfferData* offer, const CreditCard* card);

  // Dismiss the message if it is visible.
  void Dismiss();

 private:
  friend class content::WebContentsUserData<OfferNotificationControllerAndroid>;

  // Callbacks for user selection on offer notification message.
  void HandleMessageAction(const GURL& url);
  void HandleMessageDismiss(messages::DismissReason dismiss_reason);

  // Delegate of a toast style popup showing in the top of the screen.
  std::unique_ptr<messages::MessageWrapper> message_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OFFER_NOTIFICATION_CONTROLLER_ANDROID_H_
