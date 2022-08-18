// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace views {
class StyledLabel;
}  // namespace views

PageInfoCookiesContentView::PageInfoCookiesContentView(PageInfo* presenter)
    : presenter_(presenter) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // The top and bottom margins should be the same as for buttons shown below.
  const auto button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);

  // The left and right margins should align with the title labels inside other
  // buttons in this subpage (as if there was place for an icon).
  const int horizontal_offset = button_insets.left() +
                                GetLayoutConstant(PAGE_INFO_ICON_SIZE) +
                                layout_provider->GetDistanceMetric(
                                    views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  // Text on cookies description label has an embedded link to cookies settings.
  size_t offset;
  auto settings_text_for_link =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SETTINGS_LINK);
  auto description_text = l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_COOKIES_DESCRIPTION, settings_text_for_link, &offset);

  gfx::Range link_range(offset, offset + settings_text_for_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoCookiesContentView::CookiesSettingsLinkClicked,
          base::Unretained(this)));

  auto* cookies_description_label =
      AddChildView(std::make_unique<views::StyledLabel>());

  cookies_description_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(button_insets.top(), horizontal_offset,
                        button_insets.bottom(), horizontal_offset));
  cookies_description_label->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_DESCRIPTION_LABEL);
  cookies_description_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  cookies_description_label->SetText(description_text);
  cookies_description_label->AddStyleRange(link_range, link_style);

  // We need the container to have a placeholder to put the buttons in,
  // to ensure the views order.
  cookies_buttons_container_view_ =
      AddChildView(std::make_unique<PageInfoMainView::ContainerView>());

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoCookiesContentView::~PageInfoCookiesContentView() = default;

void PageInfoCookiesContentView::InitCookiesDialogButton() {
  if (cookies_dialog_button_)
    return;
  // Get the icon.
  PageInfo::PermissionInfo info;
  info.type = ContentSettingsType::COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;
  const ui::ImageModel icon = PageInfoViewFactory::GetPermissionIcon(info);

  const std::u16string& tooltip =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

  // Create the cookie button, with a temporary value for the subtitle text
  // since the site count is not yet known.
  // TODO(crbug.com/1346305): Change to correct final string.
  cookies_dialog_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<PageInfoHoverButton>(
          base::BindRepeating(
              [](PageInfoCookiesContentView* view) {
                view->presenter_->OpenCookiesDialog();
              },
              this),
          icon, IDS_PAGE_INFO_COOKIES, std::u16string(),
          PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG,
          tooltip, /*subtitle_text=*/u" ",
          PageInfoViewFactory::GetLaunchIcon()));
}

void PageInfoCookiesContentView::CookiesSettingsLinkClicked(
    const ui::Event& event) {
  presenter_->OpenCookiesSettingsView();
}

void PageInfoCookiesContentView::SetCookieInfo(
    const CookiesNewInfo& cookie_info) {
  const std::u16string num_allowed_sites_text =
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
          cookie_info.allowed_sites_count);

  // TODO(crbug.com/1346305): Add different text when FPS blocked.
  const std::u16string num_blocked_sites_text =
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT,
          cookie_info.blocked_sites_count);

  // Create the cookie dialog button and blocking third-party cookies button
  // if they don't yet exist. Those methods get called each time site data is
  // updated, so if they *do* already exist, skip this part and just update the
  // text.
  InitBlockingThirdPartyCookiesRow();
  InitCookiesDialogButton();

  // Update the text displaying the number of blocked sites.
  blocking_third_party_cookies_subtitle_label_->SetText(num_blocked_sites_text);

  // TODO(crbug.com/1346305): Add checking if FPS is blocked.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI)) {
    const std::u16string fps_button_title = l10n_util::GetStringFUTF16(
        IDS_PAGE_FPS_BUTTON_TITLE, cookie_info.fps_info.owner_name);

    const std::u16string fps_button_subtitle = l10n_util::GetStringFUTF16(
        IDS_PAGE_FPS_BUTTON_SUBTITLE, cookie_info.fps_info.owner_name);

    InitFPSButton();
    fps_button_->SetTitleText(fps_button_title);
    fps_button_->SetSubtitleText(fps_button_subtitle);
  }

  // Update the text displaying the number of allowed sites.
  cookies_dialog_button_->SetSubtitleText(num_allowed_sites_text);

  // Update the text displaying the number of blocked sites.
  blocking_third_party_cookies_subtitle_label_->SetText(num_blocked_sites_text);

  PreferredSizeChanged();
}

void PageInfoCookiesContentView::InitBlockingThirdPartyCookiesRow() {
  if (blocking_third_party_cookies_row_)
    return;

  const auto title =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TITLE);
  const auto tooltip = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TOGGLE_TOOLTIP);
  // TODO(crbug.com/1346305): Add correct icon.
  PageInfo::PermissionInfo info;
  info.type = ContentSettingsType::COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;
  const auto icon = PageInfoViewFactory::GetPermissionIcon(info);

  blocking_third_party_cookies_row_ =
      cookies_buttons_container_view_->AddChildView(
          std::make_unique<PageInfoRowView>());
  blocking_third_party_cookies_row_->SetTitle(title);
  blocking_third_party_cookies_row_->SetIcon(icon);
  blocking_third_party_cookies_subtitle_label_ =
      blocking_third_party_cookies_row_->AddSecondaryLabel(u"");

  auto* toggle_button = blocking_third_party_cookies_row_->AddControl(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoCookiesContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  toggle_button->SetAccessibleName(tooltip);
  toggle_button->SetPreferredSize(
      gfx::Size(toggle_button->GetPreferredSize().width(),
                blocking_third_party_cookies_row_->GetFirstLineHeight()));

  // TODO(crbug.com/1346305): Add checking current state.
  toggle_button->SetIsOn(true);
}

void PageInfoCookiesContentView::OnToggleButtonPressed() {
  // TODO(crbug.com/1346305): Add reaction to clicking the toggle.
}

void PageInfoCookiesContentView::InitFPSButton() {
  if (fps_button_)
    return;

  PageInfo::PermissionInfo info;
  info.type = ContentSettingsType::COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;
  // TODO(crbug.com/1346305): Change to the correct icon.
  const ui::ImageModel icon_fps = PageInfoViewFactory::GetPermissionIcon(info);

  const std::u16string& tooltip =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

  // Create the fps_button with temporary values for title and subtitle
  // as we don't have data yet, it will be updated.
  fps_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<PageInfoHoverButton>(
          base::BindRepeating(
              &PageInfoCookiesContentView::FPSSettingsButtonClicked,
              base::Unretained(this)),
          icon_fps, IDS_PAGE_INFO_COOKIES, std::u16string(),
          PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_FPS_SETTINGS,
          tooltip, /*secondary_text=*/u" ",
          PageInfoViewFactory::GetLaunchIcon()));
}

void PageInfoCookiesContentView::FPSSettingsButtonClicked(ui::Event const&) {
  // TODO(crbug.com/1346305): Add passing current FPS owner to filter by it.
  presenter_->OpenAllSitesView();
}
