// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
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

  //  TODO(crbug.com/1346305): Remove after implementation of data flow.
  EnsureCookieInfo();

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoCookiesContentView::~PageInfoCookiesContentView() = default;

void PageInfoCookiesContentView::EnsureCookieInfo() {
  if (cookies_dialog_button_ == nullptr) {
    // Get the icon.
    PageInfo::PermissionInfo info;
    info.type = ContentSettingsType::COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    const ui::ImageModel icon = PageInfoViewFactory::GetPermissionIcon(info);

    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

    // Create the cookie button, leaving the secondary text blank since the
    // site count is not yet known.
    // TODO(crbug.com/1346305): Change to correct final string.
    cookies_dialog_button_ = AddChildView(std::make_unique<PageInfoHoverButton>(
        base::BindRepeating(
            [](PageInfoCookiesContentView* view) {
              view->presenter_->OpenCookiesDialog();
            },
            this),
        icon, IDS_PAGE_INFO_COOKIES, /*secondary_text=*/u"",
        PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG,
        tooltip, std::u16string(), PageInfoViewFactory::GetLaunchIcon()));
  }
}

void PageInfoCookiesContentView::CookiesSettingsLinkClicked(
    const ui::Event& event) {
  presenter_->OpenCookiesSettingsView();
}
