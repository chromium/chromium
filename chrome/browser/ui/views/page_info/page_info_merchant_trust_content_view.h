// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/flex_layout_view.h"

// The view that is used as a content view of the "Merchant trust" subpage
// in page info.
class PageInfoMerchantTrustContentView : public views::FlexLayoutView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementIdForTesting);

  PageInfoMerchantTrustContentView();
  ~PageInfoMerchantTrustContentView() override;

 private:
  [[nodiscard]] std::unique_ptr<views::View> CreateDescriptionLabel();

  void LearnMoreLinkClicked(const ui::Event& event);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_
