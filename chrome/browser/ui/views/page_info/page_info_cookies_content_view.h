// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_

#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_row_view.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view.h"

// The view that is used as a content view of the Cookies subpage in page info.
// It contains information about cookies (short description, how many sites
// are allowed).
class PageInfoCookiesContentView : public views::View, public PageInfoUI {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCookieDialogButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCookiesPage);

  explicit PageInfoCookiesContentView(PageInfo* presenter);

  ~PageInfoCookiesContentView() override;

  // PageInfoUI implementations.
  void SetCookieInfo(const CookiesNewInfo& cookie_info) override;

  void CookiesSettingsLinkClicked(const ui::Event& event);

  void FpsSettingsButtonClicked(const ui::Event& event);

  void OnToggleButtonPressed();

  // Sets the callback for when the cookies subpage is fully initialized. If it
  // is already calls the callback
  void SetInitializedCallbackForTesting(base::OnceClosure initialized_callback);

 private:
  // Ensures the allowed sites information UI is present, with placeholder
  // information if necessary.
  void InitCookiesDialogButton();

  //  Checks if |blocking_third_party_cookies_row_| should be initiated and if
  //  so does it  and sets its info.
  void SetBlockingThirdPartyCookiesInfo(const CookiesNewInfo& cookie_info);

  // Updates toggles state according to info.
  void UpdateBlockingThirdPartyCookiesToggle(bool are_cookies_blocked);

  // Creates the child view of |blocking_third_party_cookies_row_| which is
  // either toggle or icon depending on the |enforcement|.
  void InitBlockingThirdPartyCookiesToggleOrIcon(
      CookieControlsEnforcement enforcement);

  // Ensures the blocked sites information UI is present, with placeholder
  // information if necessary.
  void InitBlockingThirdPartyCookiesRow();

  //  Checks if |fps_button_| should be initiated and if so does it and sets its
  //  info.
  void SetFpsCookiesInfo(absl::optional<CookiesFpsInfo> fps_info,
                         bool is_fps_allowed);

  // Ensures the first-party sets information UI is present, with
  // placeholder information if necessary.
  void InitFpsButton(bool is_managed);

  base::OnceClosure initialized_callback_ = base::NullCallback();

  raw_ptr<PageInfo, DanglingUntriaged> presenter_;

  // The view that contains the fps_button and cookies_dialog_button.
  raw_ptr<views::View> cookies_buttons_container_view_ = nullptr;

  // The button that opens Cookie Dialog and displays a number of allowed sites.
  raw_ptr<RichHoverButton> cookies_dialog_button_ = nullptr;

  // The view that contains toggle for blocking third party cookies
  // and displays information with a number of blocked sites.
  // Only displayed when third party cookies are blocked in settings.
  raw_ptr<PageInfoRowView> blocking_third_party_cookies_row_ = nullptr;

  // The Label which is a subtitle of |blocking_third_party_cookies_row|.
  raw_ptr<views::Label> blocking_third_party_cookies_subtitle_label_ = nullptr;

  // The toggle on |blocking_third_party_cookies_row| when state is managed by
  // the user.
  raw_ptr<views::ToggleButton> blocking_third_party_cookies_toggle_ = nullptr;

  // The icon on |blocking_third_party_cookies_row| when state is enforced.
  raw_ptr<NonAccessibleImageView> enforced_icon_ = nullptr;

  // The button that displays First-Party-Set information with a link to
  // 'All sites' settings page.
  raw_ptr<RichHoverButton> fps_button_ = nullptr;

  // Used to keep track if it's the first time for this instance recording the
  // FPS info histogram. Needed to not record the histogram each time page info
  // status changed.
  bool fps_histogram_recorded_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_
