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

void AutofillCvcSaveMessageDelegate::ShowMessage() {
  if (message_.has_value()) {
    DismissMessage();
  }

  message_.emplace(messages::MessageIdentifier::CVC_SAVE);
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

void AutofillCvcSaveMessageDelegate::DismissMessage() {
  if (message_.has_value()) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        &message_.value(), messages::DismissReason::DISMISSED_BY_FEATURE);
  }
}

}  // namespace autofill
