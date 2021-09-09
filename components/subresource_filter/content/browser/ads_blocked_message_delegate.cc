// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_blocked_message_delegate.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/android/ads_blocked_dialog.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace subresource_filter {

AdsBlockedMessageDelegate::~AdsBlockedMessageDelegate() {
  // Do not use message_ after this.
  DismissMessage(messages::DismissReason::UNKNOWN);
}

void AdsBlockedMessageDelegate::ShowMessage() {
  if (message_ || ads_blocked_dialog_) {
    // There is already an active ads blocked message or dialog.
    return;
  }

  // Unretained is safe because |this| will always outlive |message_| which owns
  // the callback.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::ADS_BLOCKED,
      base::BindOnce(&AdsBlockedMessageDelegate::HandleMessageOkClicked,
                     base::Unretained(this)),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleMessageDismissed,
                     base::Unretained(this)));

  message->SetTitle(
      l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_MESSAGE_PRIMARY_TEXT));
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_OK));
  messages::MessageDispatcherBridge* message_dispatcher_bridge =
      messages::MessageDispatcherBridge::Get();
  message->SetIconResourceId(message_dispatcher_bridge->MapToJavaDrawableId(
      IDR_ANDROID_INFOBAR_BLOCKED_POPUPS));

  message->SetSecondaryIconResourceId(
      message_dispatcher_bridge->MapToJavaDrawableId(IDR_ANDROID_SETTINGS));
  message->SetSecondaryActionCallback(
      base::BindOnce(&AdsBlockedMessageDelegate::HandleMessageManageClicked,
                     base::Unretained(this)));

  // TODO(crbug.com/1223078): On rare occasions, such as the moment when
  // activity is being recreated or destroyed, ads blocked message will not be
  // displayed.
  message_dispatcher_bridge->EnqueueMessage(
      message.get(), web_contents_, messages::MessageScopeType::WEB_CONTENTS,
      messages::MessagePriority::kNormal);

  message_ = std::move(message);
}

void AdsBlockedMessageDelegate::DismissMessage(
    messages::DismissReason dismiss_reason) {
  if (message_) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                             dismiss_reason);
  }
}

AdsBlockedMessageDelegate::AdsBlockedMessageDelegate(
    content::WebContents* web_contents)
    : AdsBlockedMessageDelegate(web_contents,
                                base::BindRepeating(AdsBlockedDialog::Create)) {
}

AdsBlockedMessageDelegate::AdsBlockedMessageDelegate(
    content::WebContents* web_contents,
    AdsBlockedDialogFactory ads_blocked_dialog_factory)
    : web_contents_(web_contents),
      ads_blocked_dialog_factory_(std::move(ads_blocked_dialog_factory)) {}

void AdsBlockedMessageDelegate::HandleMessageOkClicked() {}

void AdsBlockedMessageDelegate::HandleMessageManageClicked() {
  DismissMessage(messages::DismissReason::SECONDARY_ACTION);
  ShowDialog();
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kDetailsShown);
}

void AdsBlockedMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  DCHECK(message_);
  message_.reset();
}

void AdsBlockedMessageDelegate::HandleDialogAllowAdsClicked() {
  subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
      web_contents_->GetPrimaryPage())
      ->OnReloadRequested();
}

void AdsBlockedMessageDelegate::HandleDialogLearnMoreClicked() {
  // TODO(aishwaryarj): The dialog should be restored once the user
  // navigates back from the Learn More link tab.
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kClickedLearnMore);
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(subresource_filter::kLearnMoreLink), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
}

void AdsBlockedMessageDelegate::HandleDialogDismissed() {
  DCHECK(ads_blocked_dialog_);
  ads_blocked_dialog_.reset();
}

void AdsBlockedMessageDelegate::ShowDialog() {
  // Binding with base::Unretained(this) is safe here because
  // AdsBlockedMessageDelegate owns ads_blocked_dialog_. Callbacks won't be
  // called after the AdsBlockedMessageDelegate object is destroyed.
  ads_blocked_dialog_ = ads_blocked_dialog_factory_.Run(
      web_contents_,
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDialogAllowAdsClicked,
                     base::Unretained(this)),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDialogLearnMoreClicked,
                     base::Unretained(this)),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDialogDismissed,
                     base::Unretained(this)));

  // Ads blocked dialog factory method can return nullptr when web_contents_
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!ads_blocked_dialog_)
    return;
  ads_blocked_dialog_->Show();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AdsBlockedMessageDelegate)

}  // namespace subresource_filter
