// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_blocked_infobar_delegate.h"

#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/content/browser/ads_blocked_infobar.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace subresource_filter {

// static
void AdsBlockedInfobarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  infobar_manager->AddInfoBar(std::make_unique<AdsBlockedInfoBar>(
      base::WrapUnique(new AdsBlockedInfobarDelegate())));
}

AdsBlockedInfobarDelegate::~AdsBlockedInfobarDelegate() = default;

std::u16string AdsBlockedInfobarDelegate::GetExplanationText() const {
  return l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_EXPLANATION);
}

std::u16string AdsBlockedInfobarDelegate::GetToggleText() const {
  return l10n_util::GetStringUTF16(IDS_ALWAYS_ALLOW_ADS);
}

infobars::InfoBarDelegate::InfoBarIdentifier
AdsBlockedInfobarDelegate::GetIdentifier() const {
  return ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID;
}

int AdsBlockedInfobarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_BLOCKED_POPUPS;
}

GURL AdsBlockedInfobarDelegate::GetLinkURL() const {
  CHECK(infobar_expanded_, base::NotFatalUntil::M129);
  return GURL(subresource_filter::kLearnMoreLink);
}

bool AdsBlockedInfobarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  if (infobar_expanded_) {
    subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
        subresource_filter::SubresourceFilterAction::kClickedLearnMore);
    return ConfirmInfoBarDelegate::LinkClicked(disposition);
  }

  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kDetailsShown);
  infobar_expanded_ = true;
  return false;
}

std::u16string AdsBlockedInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_INFOBAR_MESSAGE);
}

int AdsBlockedInfobarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

std::u16string AdsBlockedInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ? IDS_OK : IDS_RELOAD);
}

bool AdsBlockedInfobarDelegate::Cancel() {
  subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar())
          ->GetPrimaryPage())
      ->OnReloadRequested();
  return true;
}

AdsBlockedInfobarDelegate::AdsBlockedInfobarDelegate() = default;

}  // namespace subresource_filter
