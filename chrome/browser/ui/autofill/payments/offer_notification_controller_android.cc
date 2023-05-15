// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"

#include <memory>

#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/infobars/autofill_offer_notification_infobar.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/grit/components_scaled_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/messages_feature.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

OfferNotificationControllerAndroid::OfferNotificationControllerAndroid(
    content::WebContents* web_contents)
    : content::WebContentsUserData<OfferNotificationControllerAndroid>(
          *web_contents) {}

OfferNotificationControllerAndroid::~OfferNotificationControllerAndroid() {
  DismissMessage();
}

void OfferNotificationControllerAndroid::ShowIfNecessary(
    const AutofillOfferData* offer,
    const CreditCard* card) {
  DCHECK(offer);
  if (!card)
    return;

  if (messages::IsOfferNotificationMessagesUiEnabled()) {
    if (message_) {
      // Dismiss the currently-shown message so that the new one can be
      // displayed.
      DismissMessage();
    }
    message_ = std::make_unique<messages::MessageWrapper>(
        messages::MessageIdentifier::OFFER_NOTIFICATION,
        base::BindOnce(&OfferNotificationControllerAndroid::HandleMessageAction,
                       base::Unretained(this), offer->GetOfferDetailsUrl()),
        base::BindOnce(
            &OfferNotificationControllerAndroid::HandleMessageDismiss,
            base::Unretained(this)));
    message_->SetTitle(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_MESSAGE_TITLE));
    message_->SetDescription(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_OFFERS_REMINDER_DESCRIPTION_TEXT,
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
  } else {
    infobars::ContentInfoBarManager::FromWebContents(&GetWebContents())
        ->AddInfoBar(std::make_unique<AutofillOfferNotificationInfoBar>(
            std::make_unique<AutofillOfferNotificationInfoBarDelegateMobile>(
                offer->GetOfferDetailsUrl(), *card)));
  }
}

void OfferNotificationControllerAndroid::Dismiss() {
  if (messages::IsOfferNotificationMessagesUiEnabled()) {
    DismissMessage();
  } else {
    infobars::ContentInfoBarManager* content_infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(&GetWebContents());
    if (!content_infobar_manager)
      return;

    for (size_t i = 0; i < content_infobar_manager->infobar_count(); ++i) {
      infobars::InfoBar* infobar = content_infobar_manager->infobar_at(i);
      if (infobar->delegate()->GetIdentifier() ==
          infobars::InfoBarDelegate::
              AUTOFILL_OFFER_NOTIFICATION_INFOBAR_DELEGATE) {
        content_infobar_manager->RemoveInfoBar(infobar);
        return;
      }
    }
  }
}

void OfferNotificationControllerAndroid::DismissMessage() {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

void OfferNotificationControllerAndroid::HandleMessageAction(const GURL& url) {
  GetWebContents().OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void OfferNotificationControllerAndroid::HandleMessageDismiss(
    messages::DismissReason dismiss_reason) {
  message_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OfferNotificationControllerAndroid);

}  // namespace autofill
