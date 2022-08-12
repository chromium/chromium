// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_

#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_row_view.h"
#include "components/page_info/page_info_ui.h"
#include "ui/views/view.h"

// The view that is used as a content view of the Cookies subpage in page info.
// It contains information about cookies (short description, how many sites
// are allowed).
class PageInfoCookiesContentView : public views::View, public PageInfoUI {
 public:
  explicit PageInfoCookiesContentView(PageInfo* presenter);

  ~PageInfoCookiesContentView() override;

  // PageInfoUI implementations.
  void SetCookieInfo(const CookiesNewInfo& cookie_info) override;

  void CookiesSettingsLinkClicked(const ui::Event& event);

  void FPSSettingsButtonClicked(const ui::Event& event);

  void OnToggleButtonPressed();

 private:
  // Ensures the allowed sites information UI is present, with placeholder
  // information if necessary.
  void InitCookiesDialogButton();

  // Ensures the blocked sites information UI is present, with placeholder
  // information if necessary.
  void InitBlockingThirdPartyCookiesRow();

  // Ensures the first-party sets information UI is present, with
  // placeholder information if necessary.
  void InitFPSButton();

  raw_ptr<PageInfo> presenter_;

  // The view that contains the fps_button and cookies_dialog_button.
  raw_ptr<views::View> cookies_buttons_container_view_ = nullptr;

  // The button that opens Cookie Dialog and displays a number of allowed sites.
  raw_ptr<PageInfoHoverButton> cookies_dialog_button_ = nullptr;

  // The view that contains toggle for blocking third party cookies
  // and displays information with a number of blocked sites.
  // Only displayed when third party cookies are blocked in settings.
  raw_ptr<PageInfoRowView> blocking_third_party_cookies_row_ = nullptr;

  // The Label which is a subtitle of |blocking_third_party_cookies_row|.
  raw_ptr<views::Label> blocking_third_party_cookies_subtitle_label_ = nullptr;

  // The button that displays First-Party-Set information with a link to
  // 'All sites' settings page.
  raw_ptr<PageInfoHoverButton> fps_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_
