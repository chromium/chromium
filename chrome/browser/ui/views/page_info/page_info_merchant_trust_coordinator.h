// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/page_info/core/page_info_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view_observer.h"

class PageInfoMerchantTrustContentView;
class PageInfoMerchantTrustController;

class PageInfoMerchantTrustCoordinator : public views::ViewObserver,
                                         public content::WebContentsObserver {
 public:
  explicit PageInfoMerchantTrustCoordinator(content::WebContents* web_contents);
  ~PageInfoMerchantTrustCoordinator() override;

  std::unique_ptr<PageInfoMerchantTrustContentView> CreatePageContent();

  void OnBubbleOpened(page_info::MerchantBubbleOpenReferrer referrer);

 private:
  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

  std::unique_ptr<PageInfoMerchantTrustController> controller_;
  raw_ptr<PageInfoMerchantTrustContentView> content_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_COORDINATOR_H_
