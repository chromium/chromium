// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_blocked_message_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
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

void AdsBlockedMessageDelegate::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (reprompt_required_) {
    // This will be true only if the user has been redirected to
    // a new tab by clicking on 'Learn more' on the dialog.
    // Upon returning to the original tab from the redirected tab,
    // the dialog will be restored.
    reprompt_required_ = false;
    ShowDialog(/*should_post_dialog=*/true);
  }
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
  message->SetSecondaryActionCallback(base::BindRepeating(
      &AdsBlockedMessageDelegate::HandleMessageManageClicked,
      base::Unretained(this)));

  // TODO(crbug.com/40774444): On rare occasions, such as the moment when
  // activity is being recreated or destroyed, ads blocked message will not be
  // displayed.
  message_dispatcher_bridge->EnqueueMessage(
      message.get(), web_contents(), messages::MessageScopeType::NAVIGATION,
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

void AdsBlockedMessageDelegate::DismissMessageForTesting(
    messages::DismissReason dismiss_reason) {
  HandleMessageDismissed(dismiss_reason);
}

AdsBlockedMessageDelegate::AdsBlockedMessageDelegate(
    content::WebContents* web_contents)
    : AdsBlockedMessageDelegate(web_contents,
                                base::BindRepeating(AdsBlockedDialog::Create)) {
}

AdsBlockedMessageDelegate::AdsBlockedMessageDelegate(
    content::WebContents* web_contents,
    AdsBlockedDialogFactory ads_blocked_dialog_factory)
    : content::WebContentsUserData<AdsBlockedMessageDelegate>(*web_contents),
      content::WebContentsObserver(web_contents),
      ads_blocked_dialog_factory_(std::move(ads_blocked_dialog_factory)) {}

void AdsBlockedMessageDelegate::HandleMessageOkClicked() {}

void AdsBlockedMessageDelegate::HandleMessageManageClicked() {
  DismissMessage(messages::DismissReason::SECONDARY_ACTION);
  ShowDialog(/*should_post_dialog=*/false);
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kDetailsShown);
}

void AdsBlockedMessageDelegate::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  CHECK(message_, base::NotFatalUntil::M129);
  message_.reset();
}

void AdsBlockedMessageDelegate::HandleDialogAllowAdsClicked() {
  subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
      web_contents()->GetPrimaryPage())
      ->OnReloadRequested();
}

void AdsBlockedMessageDelegate::HandleDialogLearnMoreClicked() {
  reprompt_required_ = true;
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kClickedLearnMore);
  web_contents()->OpenURL(
      content::OpenURLParams(GURL(subresource_filter::kLearnMoreLink),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void AdsBlockedMessageDelegate::HandleDialogDismissed() {
  if (reprompt_required_) {
    // When the dialog has been dismissed due to the user clicking on
    // 'Learn more', do not clean up the dialog instance as the dialog
    // will be restored when the user navigates back to the original tab.
    return;
  }
  CHECK(ads_blocked_dialog_, base::NotFatalUntil::M129);
  ads_blocked_dialog_.reset();
}

void AdsBlockedMessageDelegate::ShowDialog(bool should_post_dialog) {
  CHECK(!reprompt_required_, base::NotFatalUntil::M129);
  // Binding with base::Unretained(this) is safe here because
  // AdsBlockedMessageDelegate owns ads_blocked_dialog_. Callbacks won't be
  // called after the AdsBlockedMessageDelegate object is destroyed.
  ads_blocked_dialog_ = ads_blocked_dialog_factory_.Run(
      web_contents(),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDialogAllowAdsClicked,
                     base::Unretained(this)),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDialogLearnMoreClicked,
                     base::Unretained(this)),
      base::BindOnce(&AdsBlockedMessageDelegate::HandleDialogDismissed,
                     base::Unretained(this)));

  // Ads blocked dialog factory method can return nullptr when web_contents()
  // is not attached to a window. See crbug.com/1049090 for details.
  if (!ads_blocked_dialog_)
    return;
  ads_blocked_dialog_->Show(should_post_dialog);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AdsBlockedMessageDelegate);

}  // namespace subresource_filter
