// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/views/view_observer.h"

class PageInfoMerchantTrustContentView;
class PageInfoMerchantTrustController;
class ChromePageInfoUiDelegate;
class PageInfoViewFactory;

class PageInfoMerchantTrustCoordinator : public views::ViewObserver {
 public:
  PageInfoMerchantTrustCoordinator(ChromePageInfoUiDelegate* ui_delegate,
                                   PageInfoViewFactory* view_factory);
  ~PageInfoMerchantTrustCoordinator() override;

  std::unique_ptr<views::View> CreatePage();

 private:
  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

  raw_ptr<ChromePageInfoUiDelegate> ui_delegate_;
  raw_ptr<PageInfoViewFactory> view_factory_;

  std::unique_ptr<PageInfoMerchantTrustController> controller_;
  raw_ptr<PageInfoMerchantTrustContentView> content_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_COORDINATOR_H_
