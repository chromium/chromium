// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_coordinator.h"

#include <memory>

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_controller.h"
#include "components/page_info/core/page_info_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

PageInfoMerchantTrustCoordinator::PageInfoMerchantTrustCoordinator(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PageInfoMerchantTrustCoordinator::~PageInfoMerchantTrustCoordinator() = default;

std::unique_ptr<PageInfoMerchantTrustContentView>
PageInfoMerchantTrustCoordinator::CreatePageContent() {
  auto content_view = std::make_unique<PageInfoMerchantTrustContentView>();
  content_view_ = content_view.get();
  content_view_->View::AddObserver(this);

  controller_ = std::make_unique<PageInfoMerchantTrustController>(
      content_view_, web_contents());
  return content_view;
}

void PageInfoMerchantTrustCoordinator::OnViewIsDeleting(
    views::View* observed_view) {
  controller_->MerchantBubbleClosed();
  content_view_ = nullptr;
  controller_ = nullptr;
}

void PageInfoMerchantTrustCoordinator::OnBubbleOpened(
    page_info::MerchantBubbleOpenReferrer referrer) {
  controller_->MerchantBubbleOpened(referrer);
}
