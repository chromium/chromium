// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_

#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Label;
}  // namespace views

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

  void RwsSettingsButtonClicked(const ui::Event& event);

  void OnToggleButtonPressed();

  // Sets the callback for when the cookies subpage is fully initialized. If it
  // is already calls the callback
  void SetInitializedCallbackForTesting(base::OnceClosure initialized_callback);

 private:
  friend class PageInfoCookiesContentViewBaseTestClass;
  friend class PageInfoBubbleViewCookiesSubpageBrowserTest;

  // Ensures the allowed sites information UI is present, with placeholder
  // information if necessary.
  void InitCookiesDialogButton();

  // Sets `third_party_cookies_title_` and `third_party_cookies_description_`
  // text using:
  // `protections_on`: status of the COOKIES/TRACKING_PROTECTION content setting
  // `enforcement`: type of enforcement on the protection (e.g. by policy, user
  // setting)
  // `status: current 3PC blocking status
  // `blocking_status`: label for the status of the protection (e.g. allowed,
  // limited, blocked)
  // `expiration`: duration of site exception
  void SetThirdPartyCookiesTitleAndDescription(
      bool protections_on,
      CookieControlsEnforcement enforcement,
      content_settings::TrackingProtectionBlockingStatus status,
      CookieBlocking3pcdStatus blocking_status,
      base::Time expiration);

  // Sets properties for `third_party_cookies_toggle_` using:
  // `protections_on`: status of the COOKIES/TRACKING_PROTECTION content setting
  // `status: current 3PC blocking status
  void SetThirdPartyCookiesToggle(
      bool protections_on,
      content_settings::TrackingProtectionBlockingStatus status);

  // Sets `cookie_description_label_` text and style using:
  // `blocking_status`: label for the status of the protection (e.g. allowed,
  // limited, blocked)
  // `enforcement`: type of enforcement on the protection (e.g. by policy, user
  // setting)
  // `is_otr: whether the current profile is "off the record"
  void SetDescriptionLabel(CookieBlocking3pcdStatus blocking_status,
                           CookieControlsEnforcement enforcement,
                           bool is_otr);

  // Updates the new third-party cookies section using:
  // `protections_on`: status of the COOKIES/TRACKING_PROTECTION content setting
  // `controls_visible`: whether toggle is visible
  // `blocking_status`: label for the status of the protection (e.g. allowed,
  // limited, blocked)
  // `expiration`: duration of site exception
  // `feature: list of tracking protection features
  void SetThirdPartyCookiesInfo(
      bool protections_on,
      bool controls_visible,
      CookieBlocking3pcdStatus blocking_status,
      base::Time expiration,
      content_settings::TrackingProtectionFeature feature);

  // Updates toggles state according to info.
  void UpdateBlockingThirdPartyCookiesToggle(bool are_cookies_blocked);

  //  Checks if `rws_button_` should be initiated and if so does it and sets its
  //  info.
  void SetRwsCookiesInfo(std::optional<CookiesRwsInfo> rws_info,
                         bool is_rws_allowed);

  // Ensures the related website sets information UI is present, with
  // placeholder information if necessary.
  void InitRwsButton(bool is_managed);

  // Initializes the new third-party cookies section. The section starts out
  // hidden and is only shown when third-party cookies are blocked or there is
  // an active exception.
  void AddThirdPartyCookiesContainer();

  std::u16string GetStatusLabel(
      content_settings::TrackingProtectionBlockingStatus blocking_status);

  base::OnceClosure initialized_callback_ = base::NullCallback();

  raw_ptr<PageInfo, DanglingUntriaged> presenter_ = nullptr;

  // The view that contains the `rws_button_` and `cookies_dialog_button_`.
  raw_ptr<views::View> cookies_buttons_container_view_ = nullptr;

  // The button that opens Cookie Dialog and displays a number of allowed sites.
  raw_ptr<RichHoverButton> cookies_dialog_button_ = nullptr;

  // The StyledLabel that appears above |third_party_cookies_container|.
  raw_ptr<views::StyledLabel> cookies_description_label_ = nullptr;

  // The toggle on |blocking_third_party_cookies_row| when state is managed by
  // the user.
  raw_ptr<views::ToggleButton> blocking_third_party_cookies_toggle_ = nullptr;

  // The button that displays Related-Website-Set information with a link to
  // 'All sites' settings page.
  raw_ptr<RichHoverButton> rws_button_ = nullptr;

  // Used to keep track if it's the first time for this instance recording the
  // RWS info histogram. Needed to not record the histogram each time page info
  // status changed.
  bool rws_histogram_recorded_ = false;

  // Third-party cookies section which contains a title, a description and a
  // toggle row view.
  raw_ptr<views::BoxLayoutView> third_party_cookies_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> third_party_cookies_label_wrapper_ = nullptr;
  raw_ptr<views::Label> third_party_cookies_title_ = nullptr;
  raw_ptr<views::Label> third_party_cookies_description_ = nullptr;
  raw_ptr<RichControlsContainerView> third_party_cookies_row_ = nullptr;
  raw_ptr<views::ToggleButton> third_party_cookies_toggle_ = nullptr;
  raw_ptr<views::ImageView> third_party_cookies_enforced_icon_ = nullptr;
  raw_ptr<views::Label> third_party_cookies_toggle_subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_COOKIES_CONTENT_VIEW_H_
