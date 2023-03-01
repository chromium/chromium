// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace views {
class StyledLabel;
}  // namespace views

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoCookiesContentView,
                                      kCookieDialogButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoCookiesContentView, kCookiesPage);

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
  cookies_buttons_container_view_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_BUTTONS_CONTAINER);

  presenter_->InitializeUiState(this, base::DoNothing());

  SetProperty(views::kElementIdentifierKey, kCookiesPage);
}

PageInfoCookiesContentView::~PageInfoCookiesContentView() = default;

void PageInfoCookiesContentView::SetInitializedCallbackForTesting(
    base::OnceClosure initialized_callback) {
  if (cookies_dialog_button_) {
    std::move(initialized_callback).Run();
  } else {
    initialized_callback_ = std::move(initialized_callback);
  }
}

void PageInfoCookiesContentView::InitCookiesDialogButton() {
  if (cookies_dialog_button_)
    return;
  // Get the icon.
  PageInfo::PermissionInfo info;
  info.type = ContentSettingsType::COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;
  const ui::ImageModel icon = PageInfoViewFactory::GetPermissionIcon(info);

  const std::u16string& tooltip =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TOOLTIP);

  // Create the cookie button, with a temporary value for the subtitle text
  // since the site count is not yet known.
  cookies_dialog_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              [](PageInfoCookiesContentView* view) {
                view->presenter_->OpenCookiesDialog();
              },
              this),
          icon,
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TITLE),
          std::u16string(),

          tooltip, /*subtitle_text=*/u" ",
          PageInfoViewFactory::GetLaunchIcon()));
  cookies_dialog_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  cookies_dialog_button_->SetProperty(views::kElementIdentifierKey,
                                      kCookieDialogButton);
}

void PageInfoCookiesContentView::CookiesSettingsLinkClicked(
    const ui::Event& event) {
  presenter_->OpenCookiesSettingsView();
}

void PageInfoCookiesContentView::SetCookieInfo(
    const CookiesNewInfo& cookie_info) {
  const bool is_fps_allowed =
      base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxFirstPartySetsUI) &&
      cookie_info.fps_info;

  const std::u16string num_allowed_sites_text =
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
          cookie_info.allowed_sites_count);

  // Create the cookie dialog button, blocking third-party cookies button
  // (only if third-party cookies are blocked in settings) and FPS button
  // (only if fps are not blocked) if they don't yet exist. Those methods get
  // called each time site data is updated, so if they *do* already exist,
  // skip creating the buttons and just update the texts.
  SetBlockingThirdPartyCookiesInfo(cookie_info);

  InitCookiesDialogButton();
  // Update the text displaying the number of allowed sites.
  cookies_dialog_button_->SetSubtitleText(num_allowed_sites_text);

  SetFpsCookiesInfo(cookie_info.fps_info, is_fps_allowed);

  PreferredSizeChanged();
  if (!initialized_callback_.is_null())
    std::move(initialized_callback_).Run();
}

void PageInfoCookiesContentView::SetBlockingThirdPartyCookiesInfo(
    const CookiesNewInfo& cookie_info) {
  bool show_cookies_block_control = false;
  bool are_cookies_blocked = false;
  switch (cookie_info.status) {
    case CookieControlsStatus::kEnabled:
      show_cookies_block_control = true;
      are_cookies_blocked = true;
      break;
    case CookieControlsStatus::kDisabledForSite:
      show_cookies_block_control = true;
      break;
    case CookieControlsStatus::kDisabled:
      break;
    case CookieControlsStatus::kUninitialized:
      NOTREACHED_NORETURN();
  }

  if (show_cookies_block_control) {
    InitBlockingThirdPartyCookiesRow();
    blocking_third_party_cookies_row_->SetVisible(true);
    InitBlockingThirdPartyCookiesToggleOrIcon(cookie_info.enforcement);
    if (blocking_third_party_cookies_toggle_)
      UpdateBlockingThirdPartyCookiesToggle(are_cookies_blocked);

    if (are_cookies_blocked) {
      // TODO(crbug.com/1349370): Use
      // IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT_WHEN_FPS_BLOCKED when FPS are
      // disabled and the site belongs to a set.
      const auto blocked_sites_count_message_id =
          IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT;
      const std::u16string num_blocked_sites_text =
          l10n_util::GetPluralStringFUTF16(blocked_sites_count_message_id,
                                           cookie_info.blocked_sites_count);

      // Update the text displaying the number of blocked sites.
      blocking_third_party_cookies_subtitle_label_->SetText(
          num_blocked_sites_text);
    }

    // If third party cookies are being blocked the subtitle should be visible.
    blocking_third_party_cookies_subtitle_label_->SetVisible(
        are_cookies_blocked);
  } else if (blocking_third_party_cookies_row_) {
    blocking_third_party_cookies_row_->SetVisible(false);
  }
}

void PageInfoCookiesContentView::UpdateBlockingThirdPartyCookiesToggle(
    bool are_cookies_blocked) {
  DCHECK(blocking_third_party_cookies_toggle_);
  blocking_third_party_cookies_toggle_->SetIsOn(are_cookies_blocked);
}

void PageInfoCookiesContentView::InitBlockingThirdPartyCookiesToggleOrIcon(
    CookieControlsEnforcement enforcement) {
  // The row needs to be initiated before with
  // |InitBlockingThirdPartyCookiesRow| because we're adding subview to it.
  DCHECK(blocking_third_party_cookies_row_);

  // TODO(crbug.com/1346305): Add correct tooltips when known.
  int tooltip_id = 0;
  bool enforced = false;
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByExtension:
      tooltip_id = IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION;
      enforced = true;
      break;
    case CookieControlsEnforcement::kEnforcedByPolicy:
      tooltip_id = IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY;
      enforced = true;
      break;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      // TODO(crbug.com/1346305): Add what should happen when it's managed by
      // cookies settings.
      tooltip_id =
          IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_MANAGED_BY_SETTINGS_TOOLTIP;
      enforced = true;
      break;
    case CookieControlsEnforcement::kNoEnforcement:
      break;
  }

  // Set correct visibility for existing views.
  if (enforced_icon_)
    enforced_icon_->SetVisible(enforced);
  if (blocking_third_party_cookies_toggle_)
    blocking_third_party_cookies_toggle_->SetVisible(!enforced);

  // If it's not enforced then toggle is for sure not being changed.
  if (!enforced && blocking_third_party_cookies_toggle_)
    return;

  // Newly created views are visible by default.
  if (enforced) {
    if (!enforced_icon_) {
      enforced_icon_ = blocking_third_party_cookies_row_->AddControl(
          std::make_unique<NonAccessibleImageView>());
      enforced_icon_->SetTooltipText(l10n_util::GetStringUTF16(tooltip_id));
    }
    // If it's enforced then the icon might need to be changed.
    enforced_icon_->SetImage(
        PageInfoViewFactory::GetEnforcedCookieControlsIcon(enforcement));
  } else {
    const auto tooltip = l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TOGGLE_TOOLTIP);
    blocking_third_party_cookies_toggle_ =
        blocking_third_party_cookies_row_->AddControl(
            std::make_unique<views::ToggleButton>(base::BindRepeating(
                &PageInfoCookiesContentView::OnToggleButtonPressed,
                base::Unretained(this))));
    blocking_third_party_cookies_toggle_->SetAccessibleName(tooltip);
    blocking_third_party_cookies_toggle_->SetPreferredSize(gfx::Size(
        blocking_third_party_cookies_toggle_->GetPreferredSize().width(),
        blocking_third_party_cookies_row_->GetFirstLineHeight()));
    blocking_third_party_cookies_toggle_->SetID(
        PageInfoViewFactory::
            VIEW_ID_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TOGGLE);
  }
}

void PageInfoCookiesContentView::InitBlockingThirdPartyCookiesRow() {
  if (blocking_third_party_cookies_row_)
    return;

  const auto title =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TITLE);
  const auto icon = PageInfoViewFactory::GetBlockingThirdPartyCookiesIcon();

  // |blocking_third_party_cookies_row_| has to be the first cookie button.
  blocking_third_party_cookies_row_ =
      cookies_buttons_container_view_->AddChildViewAt(
          std::make_unique<PageInfoRowView>(), 0);
  blocking_third_party_cookies_row_->SetTitle(title);
  blocking_third_party_cookies_row_->SetIcon(icon);
  blocking_third_party_cookies_row_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_ROW);

  // The subtext is only visible when third-party cookies are being blocked.
  // At the beginning it's not.
  blocking_third_party_cookies_subtitle_label_ =
      blocking_third_party_cookies_row_->AddSecondaryLabel(u"");
  blocking_third_party_cookies_subtitle_label_->SetVisible(false);
  blocking_third_party_cookies_subtitle_label_->SetID(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_SUBTITLE);
}

void PageInfoCookiesContentView::OnToggleButtonPressed() {
  presenter_->OnThirdPartyToggleClicked(
      blocking_third_party_cookies_toggle_->GetIsOn());
}

void PageInfoCookiesContentView::SetFpsCookiesInfo(
    absl::optional<CookiesFpsInfo> fps_info,
    bool is_fps_allowed) {
  if (is_fps_allowed) {
    InitFpsButton(fps_info->is_managed);
    fps_button_->SetVisible(true);

    const std::u16string fps_button_title =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_FPS_BUTTON_TITLE);
    const std::u16string fps_button_subtitle = l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_FPS_BUTTON_SUBTITLE, fps_info->owner_name);

    // Update the text displaying the name of FPS owner.
    fps_button_->SetTitleText(fps_button_title);
    fps_button_->SetSubtitleText(fps_button_subtitle);
  } else if (fps_button_) {
    fps_button_->SetVisible(false);
  }
  if (!fps_histogram_recorded_) {
    fps_histogram_recorded_ = true;
    base::UmaHistogramBoolean("Security.PageInfo.Cookies.HasFPSInfo",
                              is_fps_allowed);
  }
}

void PageInfoCookiesContentView::InitFpsButton(bool is_managed) {
  if (fps_button_)
    return;

  const std::u16string& tooltip =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_FPS_BUTTON_TOOLTIP);

  // Create the fps_button with temporary values for title and subtitle
  // as we don't have data yet, it will be updated.
  fps_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              &PageInfoCookiesContentView::FpsSettingsButtonClicked,
              base::Unretained(this)),
          PageInfoViewFactory::GetFpsIcon(),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES), std::u16string(),
          tooltip, /*secondary_text=*/u" ",
          PageInfoViewFactory::GetLaunchIcon(),
          is_managed ? absl::optional<ui::ImageModel>(
                           PageInfoViewFactory::GetEnforcedByPolicyIcon())
                     : absl::nullopt));
  fps_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_FPS_SETTINGS);
}

void PageInfoCookiesContentView::FpsSettingsButtonClicked(ui::Event const&) {
  presenter_->OpenAllSitesViewFilteredToFps();
}
