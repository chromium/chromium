// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_info/core/page_info_types.h"
#include "url/gurl.h"

class ChromePageInfoUiDelegate;
class PageInfoMerchantTrustContentView;

class PageInfoMerchantTrustController {
 public:
  PageInfoMerchantTrustController(
      PageInfoMerchantTrustContentView* content_view,
      ChromePageInfoUiDelegate* ui_delegate);
  virtual ~PageInfoMerchantTrustController();

 private:
  void OnMerchantTrustDataFetched(
      const GURL& url,
      std::optional<page_info::MerchantData> merchant_data);

  raw_ptr<PageInfoMerchantTrustContentView> content_view_;
  raw_ptr<ChromePageInfoUiDelegate> ui_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTROLLER_H_
