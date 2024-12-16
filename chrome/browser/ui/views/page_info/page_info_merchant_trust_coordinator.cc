// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_coordinator.h"

#include <memory>

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

PageInfoMerchantTrustCoordinator::PageInfoMerchantTrustCoordinator(
    ChromePageInfoUiDelegate* ui_delegate,
    PageInfoViewFactory* view_factory)
    : ui_delegate_(ui_delegate), view_factory_(view_factory) {}

PageInfoMerchantTrustCoordinator::~PageInfoMerchantTrustCoordinator() = default;

std::unique_ptr<views::View> PageInfoMerchantTrustCoordinator::CreatePage() {
  auto content_view = std::make_unique<PageInfoMerchantTrustContentView>();
  content_view_ = content_view.get();
  content_view_->View::AddObserver(this);

  controller_ = std::make_unique<PageInfoMerchantTrustController>(content_view_,
                                                                  ui_delegate_);

  auto title = l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HEADER);
  auto page_view =
      view_factory_->CreatePageView(title, std::move(content_view));
  page_view->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  return page_view;
}

void PageInfoMerchantTrustCoordinator::OnViewIsDeleting(
    views::View* observed_view) {
  content_view_ = nullptr;
  controller_ = nullptr;
}
