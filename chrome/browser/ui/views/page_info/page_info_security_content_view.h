// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_SECURITY_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_SECURITY_CONTENT_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/security_information_view.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// The view that contains `SecurityInformationView` and a certificate button.
// It is used as a content of the security subpage or is directly integrated in
// the main page if connection isn't secure.
class PageInfoSecurityContentView : public views::View, public PageInfoUI {
  METADATA_HEADER(PageInfoSecurityContentView, views::View)

 public:
  // `is_standalone_page` is true, when this view is used as a content view of
  // a subpage and this view becomes current UI for `PageInfo` by calling
  // `InitializeUiState()`. Otherwise, it is part of another page (part of the
  // main page if connection isn't secure).
  PageInfoSecurityContentView(PageInfo* presenter, bool is_standalone_page);
  ~PageInfoSecurityContentView() override;

  // PageInfoUI implementations.
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PageInfoBubbleViewTest, CheckHeaderInteractions);

  void ResetDecisionsClicked();

  void SecurityDetailsClicked(const ui::Event& event);

  raw_ptr<PageInfo, DanglingUntriaged> presenter_;

  // The button that opens the "Certificate" dialog.
  raw_ptr<RichHoverButton> certificate_button_ = nullptr;

  // The views that shows the status of the site's identity check.
  raw_ptr<SecurityInformationView> security_view_ = nullptr;

  // The certificate provided by the site, if one exists.
  scoped_refptr<net::X509Certificate> certificate_;

  // TODO(crbug.com/40754666): Add plumbing to check this in tests or rewrite
  // tests not use it.
  PageInfoUI::SecurityDescriptionType security_description_type_ =
      PageInfoUI::SecurityDescriptionType::CONNECTION;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_SECURITY_CONTENT_VIEW_H_
