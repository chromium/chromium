// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_AD_PERSONALIZATION_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_AD_PERSONALIZATION_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info_ui.h"
#include "ui/views/layout/flex_layout_view.h"

class ChromePageInfoUiDelegate;
class PageInfo;

namespace views {
class BoxLayoutView;
}

// The view that is used as a content view of the "Ad personalization" subpage
// in page info.
class PageInfoAdPersonalizationContentView : public views::FlexLayoutView,
                                             public PageInfoUI {
 public:
  explicit PageInfoAdPersonalizationContentView(
      PageInfo* presenter,
      ChromePageInfoUiDelegate* ui_delegate);
  ~PageInfoAdPersonalizationContentView() override;

  // PageInfoUI implementations.
  void SetAdPersonalizationInfo(const AdPersonalizationInfo& info) override;

 private:
  const raw_ptr<PageInfo, DanglingUntriaged> presenter_;
  const raw_ptr<ChromePageInfoUiDelegate, DanglingUntriaged> ui_delegate_;

  raw_ptr<views::BoxLayoutView> info_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_AD_PERSONALIZATION_CONTENT_VIEW_H_
