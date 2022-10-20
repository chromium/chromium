// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info_ui.h"
#include "ui/views/view.h"

class ChromePageInfoUiDelegate;
class PageInfo;

// The view that is used as a content view of the "About this site" subpage in
// page info. It contains short description about the website with the source
// (usually from Wikipedia).
class PageInfoAboutThisSiteContentView : public views::View, public PageInfoUI {
 public:
  PageInfoAboutThisSiteContentView(PageInfo* presenter,
                                   ChromePageInfoUiDelegate* ui_delegate,
                                   const page_info::proto::SiteInfo& info);
  ~PageInfoAboutThisSiteContentView() override;

 private:
  [[nodiscard]] std::unique_ptr<views::View> CreateDescriptionLabel(
      const page_info::proto::SiteInfo& info);
  [[nodiscard]] std::unique_ptr<views::View> CreateSourceLabel(
      const page_info::proto::SiteInfo& info);
  void SourceLinkClicked(const ui::Event& event);

  raw_ptr<PageInfo, DanglingUntriaged> presenter_;
  raw_ptr<ChromePageInfoUiDelegate, DanglingUntriaged> ui_delegate_;
  page_info::proto::SiteInfo info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_ABOUT_THIS_SITE_CONTENT_VIEW_H_
