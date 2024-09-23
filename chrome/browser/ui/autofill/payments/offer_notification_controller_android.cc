// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

OfferNotificationControllerAndroid::OfferNotificationControllerAndroid(
    content::WebContents* web_contents)
    : content::WebContentsUserData<OfferNotificationControllerAndroid>(
          *web_contents) {}

OfferNotificationControllerAndroid::~OfferNotificationControllerAndroid() {
  Dismiss();
}

void OfferNotificationControllerAndroid::ShowIfNecessary(
    const AutofillOfferData* offer,
    const CreditCard* card) {
  DCHECK(offer);
  if (!card) {
    return;
  }

  if (message_) {
    // Dismiss the currently-shown message so that the new one can be
    // displayed.
    Dismiss();
  }
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::OFFER_NOTIFICATION,
      base::BindOnce(&OfferNotificationControllerAndroid::HandleMessageAction,
                     base::Unretained(this), offer->GetOfferDetailsUrl()),
      base::BindOnce(&OfferNotificationControllerAndroid::HandleMessageDismiss,
                     base::Unretained(this)));
  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_MESSAGE_TITLE));
  message_->SetDescription(
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_OFFERS_REMINDER_DESCRIPTION_TEXT,
                                 card->CardNameAndLastFourDigits()));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_GOOGLE_PAY));
  message_->DisableIconTint();
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_OFFERS_MESSAGE_PRIMARY_BUTTON_TEXT));
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), &GetWebContents(),
      messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
}

void OfferNotificationControllerAndroid::Dismiss() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void OfferNotificationControllerAndroid::HandleMessageAction(const GURL& url) {
  GetWebContents().OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void OfferNotificationControllerAndroid::HandleMessageDismiss(
    messages::DismissReason dismiss_reason) {
  message_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationControllerAndroid);

}  // namespace autofill
