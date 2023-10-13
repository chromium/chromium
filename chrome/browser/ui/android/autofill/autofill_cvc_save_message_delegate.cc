// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"

#include "chrome/browser/android/resource_mapper.h"
#include "components/grit/components_scaled_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

AutofillCvcSaveMessageDelegate::AutofillCvcSaveMessageDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutofillCvcSaveMessageDelegate::~AutofillCvcSaveMessageDelegate() {
  DismissMessage();
}

void AutofillCvcSaveMessageDelegate::ShowMessage(
    std::unique_ptr<AutofillSaveCardDelegateAndroid> delegate) {
  CHECK(delegate);
  save_card_delegate_ = std::move(delegate);

  if (message_.has_value()) {
    DismissMessage();
  }

  message_.emplace(
      messages::MessageIdentifier::CVC_SAVE,
      base::BindOnce(&AutofillCvcSaveMessageDelegate::OnMessageAccepted,
                     base::Unretained(this)),
      base::BindOnce(&AutofillCvcSaveMessageDelegate::OnMessageDismissed,
                     base::Unretained(this)));
  message_->SetTitle(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CVC_PROMPT_TITLE_TO_CLOUD));
  message_->SetDescription(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CVC_PROMPT_EXPLANATION_UPLOAD));
  message_->SetPrimaryButtonText(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CVC_MESSAGE_SAVE_ACCEPT));
  message_->SetIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_CC_GENERIC_PRIMARY));
  message_->DisableIconTint();
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      &message_.value(), web_contents_,
      messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);
}

void AutofillCvcSaveMessageDelegate::OnMessageAccepted() {
  save_card_delegate_->OnUiAccepted(base::BindOnce(
      &AutofillCvcSaveMessageDelegate::DeleteSaveCardDelegateSoon,
      base::Unretained(this)));
}

void AutofillCvcSaveMessageDelegate::OnMessageDismissed(
    messages::DismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case messages::DismissReason::PRIMARY_ACTION:
      // Primary action is handled in `OnMessageAccepted`.
      break;
    case messages::DismissReason::SECONDARY_ACTION:
      // No secondary action.
      NOTREACHED_NORETURN();
    case messages::DismissReason::GESTURE:
      // User explicitly dismissed the message.
      save_card_delegate_->OnUiCanceled();
      save_card_delegate_.reset();
      break;
    default:
      // User ignored the message.
      save_card_delegate_->OnUiIgnored();
      save_card_delegate_.reset();
      break;
  }
  message_.reset();
}

void AutofillCvcSaveMessageDelegate::DismissMessage() {
  if (message_.has_value()) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        &message_.value(), messages::DismissReason::DISMISSED_BY_FEATURE);
  }
}

void AutofillCvcSaveMessageDelegate::DeleteSaveCardDelegateSoon() {
  content::GetUIThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(save_card_delegate_));
}

}  // namespace autofill
