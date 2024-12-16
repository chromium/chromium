// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_controller.h"

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "components/page_info/core/page_info_types.h"

PageInfoMerchantTrustController::PageInfoMerchantTrustController(
    PageInfoMerchantTrustContentView* content_view,
    ChromePageInfoUiDelegate* ui_delegate)
    : content_view_(content_view), ui_delegate_(ui_delegate) {
  ui_delegate_->GetMerchantTrustInfo(base::BindOnce(
      &PageInfoMerchantTrustController::OnMerchantTrustDataFetched,
      base::Unretained(this)));
}

PageInfoMerchantTrustController::~PageInfoMerchantTrustController() = default;

void PageInfoMerchantTrustController::OnMerchantTrustDataFetched(
    const GURL& url,
    std::optional<page_info::MerchantData> merchant_data) {
  if (!merchant_data.has_value()) {
    return;
  }

  content_view_->SetRating(merchant_data->star_rating);
  content_view_->SetReviewCount(merchant_data->count_rating);
}
