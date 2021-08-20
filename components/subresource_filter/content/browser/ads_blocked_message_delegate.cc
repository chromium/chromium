// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_blocked_message_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace subresource_filter {

void AdsBlockedMessageDelegate::ShowMessage() {
  if (message_) {
    // There is already an active "ads blocked" message.
    return;
  }

  // Unretained is safe because |this| will always outlive |message_| which owns
  // the callback.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::ADS_BLOCKED,
      base::BindOnce(&AdsBlockedMessageDelegate::HandleClick,
                     base::Unretained(this)),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));

  message->SetTitle(
      l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_MESSAGE_PRIMARY_TEXT));
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));

  // TODO: Set primary and secondary resource icons.
  // message->SetIconResourceId();
  // message->SetSecondaryIconResourceId();

  // TODO(crbug.com/1223078): On rare occasions, such as the moment when
  // activity is being recreated or destroyed, ads blocked message will not be
  // displayed.
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);

  message_ = std::move(message);
}

AdsBlockedMessageDelegate::~AdsBlockedMessageDelegate() {
  if (message_) {
    // Do not use message_ after this.
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

AdsBlockedMessageDelegate::AdsBlockedMessageDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

void AdsBlockedMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  message_.reset();
}

void AdsBlockedMessageDelegate::HandleClick() {
  // TODO: Add implementation.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AdsBlockedMessageDelegate)

}  // namespace subresource_filter
