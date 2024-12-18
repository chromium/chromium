// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_controller.h"

#include "chrome/browser/page_info/merchant_trust_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/page_info/merchant_trust_side_panel.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "components/page_info/core/merchant_trust_service.h"
#include "components/page_info/core/page_info_types.h"

PageInfoMerchantTrustController::PageInfoMerchantTrustController(
    PageInfoMerchantTrustContentView* content_view,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), content_view_(content_view) {
  InitCallbacks();

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  MerchantTrustServiceFactory::GetForProfile(profile)->GetMerchantTrustInfo(
      web_contents->GetVisibleURL(),
      base::BindOnce(
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

  merchant_data_ = merchant_data.value();
  content_view_->SetReviewsSummary(
      base::ASCIIToUTF16(merchant_data->reviews_summary));
  content_view_->SetRating(merchant_data->star_rating);
  content_view_->SetReviewCount(merchant_data->count_rating);
}

void PageInfoMerchantTrustController::LearnMoreLinkPressed(
    const ui::Event& event) {
  // TODO(crbug.com/381405880): Open learn more link.
}

void PageInfoMerchantTrustController::ViewReviewsPressed() {
  ShowMerchantTrustSidePanel(web_contents(), merchant_data_.page_url);
}

void PageInfoMerchantTrustController::InitCallbacks() {
  learn_more_link_callback_ =
      content_view_->RegisterLearnMoreLinkPressedCallback(base::BindRepeating(
          &PageInfoMerchantTrustController::LearnMoreLinkPressed,
          base::Unretained(this)));

  view_reviews_button_callback_ =
      content_view_->RegisterViewReviewsButtonPressedCallback(
          base::BindRepeating(
              &PageInfoMerchantTrustController::ViewReviewsPressed,
              base::Unretained(this)));
}
