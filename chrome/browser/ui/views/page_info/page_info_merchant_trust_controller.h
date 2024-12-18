// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/page_info/core/page_info_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/events/event.h"

class PageInfoMerchantTrustContentView;

class PageInfoMerchantTrustController : public content::WebContentsObserver {
 public:
  PageInfoMerchantTrustController(
      PageInfoMerchantTrustContentView* content_view,
      content::WebContents* web_contents);
  ~PageInfoMerchantTrustController() override;

 private:
  void OnMerchantTrustDataFetched(
      const GURL& url,
      std::optional<page_info::MerchantData> merchant_data);
  void LearnMoreLinkPressed(const ui::Event& event);
  void ViewReviewsPressed();
  void InitCallbacks();

  raw_ptr<PageInfoMerchantTrustContentView> content_view_;
  page_info::MerchantData merchant_data_;

  base::CallbackListSubscription learn_more_link_callback_;
  base::CallbackListSubscription view_reviews_button_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_
