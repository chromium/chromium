// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/content_settings/browser/ui/cookie_controls_util.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

using content_settings::CookieControlsUtil;

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

  cookies_description_label_ =
      AddChildView(std::make_unique<views::StyledLabel>());

  // In the new UI iteration, description labels are aligned with the icons on
  // the left, not with the bubble title.
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    cookies_description_label_->SetProperty(views::kMarginsKey, button_insets);
  } else {
    cookies_description_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(button_insets.top(), horizontal_offset,
                          button_insets.bottom(), horizontal_offset));
  }
  cookies_description_label_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_DESCRIPTION_LABEL);
  if (features::IsChromeRefresh2023()) {
    cookies_description_label_->SetDefaultTextStyle(views::style::STYLE_BODY_5);
    cookies_description_label_->SetDefaultEnabledColorId(
        ui::kColorLabelForegroundSecondary);
  } else {
    cookies_description_label_->SetDefaultTextStyle(
        views::style::STYLE_SECONDARY);
  }

  AddThirdPartyCookiesContainer();

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
  if (cookies_dialog_button_) {
    return;
  }
  // Get the icon.
  PageInfo::PermissionInfo info;
  info.type = ContentSettingsType::COOKIES;
  info.setting = CONTENT_SETTING_ALLOW;

  cookies_buttons_container_view_->AddChildView(
      PageInfoViewFactory::CreateSeparator(
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)));

  // Create the cookie button, with a temporary value for the subtitle text
  // since the site count is not yet known.
  cookies_dialog_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              [](PageInfoCookiesContentView* view) {
                view->presenter_->OpenCookiesDialog();
              },
              this),
          PageInfoViewFactory::GetPermissionIcon(info),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TITLE),
          /*secondary_text=*/std::u16string(),
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TOOLTIP),
          /*subtitle_text=*/u" ", PageInfoViewFactory::GetLaunchIcon()));
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
  SetDescriptionLabel(cookie_info.blocking_status, cookie_info.enforcement,
                      cookie_info.is_otr);

  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    SetThirdPartyCookiesInfo(cookie_info);
  } else {
    // Create the cookie dialog button, blocking third-party cookies button
    // (only if third-party cookies are blocked in settings) and FPS button
    // (only if fps are not blocked) if they don't yet exist. Those methods get
    // called each time site data is updated, so if they *do* already exist,
    // skip creating the buttons and just update the texts.
    SetBlockingThirdPartyCookiesInfo(cookie_info);
  }

  InitCookiesDialogButton();
  // Update the text displaying the number of allowed sites.
  cookies_dialog_button_->SetSubtitleText(l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
      cookie_info.allowed_sites_count));

  bool is_fps_allowed = base::FeatureList::IsEnabled(
                            privacy_sandbox::kPrivacySandboxFirstPartySetsUI) &&
                        cookie_info.fps_info;
  SetFpsCookiesInfo(cookie_info.fps_info, is_fps_allowed);

  PreferredSizeChanged();
  if (!initialized_callback_.is_null()) {
    std::move(initialized_callback_).Run();
  }
}

void PageInfoCookiesContentView::SetBlockingThirdPartyCookiesInfo(
    const CookiesNewInfo& cookie_info) {
  bool are_cookies_blocked =
      cookie_info.status == CookieControlsStatus::kEnabled;

  if (cookie_info.status == CookieControlsStatus::kDisabledForSite ||
      are_cookies_blocked) {
    InitBlockingThirdPartyCookiesRow();
    blocking_third_party_cookies_row_->SetVisible(true);
    InitBlockingThirdPartyCookiesToggleOrIcon(cookie_info.enforcement);
    if (blocking_third_party_cookies_toggle_) {
      UpdateBlockingThirdPartyCookiesToggle(are_cookies_blocked);
    }

    if (are_cookies_blocked) {
      // TODO(crbug.com/1349370): Use
      // IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT_WHEN_FPS_BLOCKED when FPS are
      // disabled and the site belongs to a set.
      const auto blocked_sites_count_message_id =
          IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT;
      const std::u16string num_blocked_sites_text =
          l10n_util::GetPluralStringFUTF16(
              blocked_sites_count_message_id,
              cookie_info.blocked_third_party_sites_count);

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

void PageInfoCookiesContentView::SetThirdPartyCookiesTitleAndDescription(
    const CookiesNewInfo& cookie_info) {
  bool tracking_protection_3pcd =
      cookie_info.blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd;

  std::u16string title_text;
  int description;
  if (cookie_info.status == CookieControlsStatus::kEnabled) {
    title_text =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE);
    // Check if site exception would be permanent (no expiration).
    if (content_settings::features::kUserBypassUIExceptionExpiration.Get()
            .is_zero()) {
      description =
          IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_DESCRIPTION_PERMANENT;
    } else {
      description =
          tracking_protection_3pcd
              ? IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY
              : IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY;
    }
  } else if (cookie_info.expiration.is_null() ||
             cookie_info.enforcement ==
                 CookieControlsEnforcement::kEnforcedByCookieSetting) {
    // Handle permanent site exception.
    title_text = l10n_util::GetStringUTF16(
        tracking_protection_3pcd
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE
            : IDS_PAGE_INFO_COOKIES_PERMANENT_ALLOWED_TITLE);
    description =
        tracking_protection_3pcd
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION
            : IDS_PAGE_INFO_COOKIES_PERMANENT_ALLOWED_DESCRIPTION;
  } else {
    // Handle temporary site exception.
    int title;
    if (cookie_info.blocking_status == CookieBlocking3pcdStatus::kAll) {
      title =
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_3PC_BLOCKED_RESTART_TITLE;
    } else if (cookie_info.blocking_status ==
               CookieBlocking3pcdStatus::kLimited) {
      title = IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITING_RESTART_TITLE;
    } else {
      title = IDS_PAGE_INFO_COOKIES_BLOCKING_RESTART_TITLE;
    }
    title_text = l10n_util::GetPluralStringFUTF16(
        title, CookieControlsUtil::GetDaysToExpiration(cookie_info.expiration));
    description =
        tracking_protection_3pcd
            ? IDS_PAGE_INFO_COOKIES_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION
            : IDS_PAGE_INFO_COOKIES_BLOCKING_RESTART_DESCRIPTION_TODAY;
  }
  third_party_cookies_title_->SetText(title_text);
  third_party_cookies_description_->SetText(
      l10n_util::GetStringUTF16(description));
}

void PageInfoCookiesContentView::SetThirdPartyCookiesToggle(
    const CookiesNewInfo& cookie_info) {
  bool are_third_party_cookies_blocked =
      cookie_info.status == CookieControlsStatus::kEnabled;

  std::u16string subtitle, a11y_name;
  if (are_third_party_cookies_blocked) {
    if (cookie_info.blocking_status == CookieBlocking3pcdStatus::kAll) {
      subtitle = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED);
    } else if (cookie_info.blocking_status ==
               CookieBlocking3pcdStatus::kLimited) {
      subtitle = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITED);
    } else {
      subtitle = l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT,
          cookie_info.blocked_third_party_sites_count);
    }
    a11y_name = l10n_util::GetPluralStringFUTF16(
        IDS_PAGE_INFO_COOKIES_THIRD_PARTY_COOKIES_BLOCKED_TOGGLE_A11Y,
        cookie_info.blocked_third_party_sites_count);
  } else {
    subtitle =
        cookie_info.blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd
            ? l10n_util::GetStringUTF16(
                  IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED)
            : l10n_util::GetPluralStringFUTF16(
                  IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                  cookie_info.allowed_third_party_sites_count);
    a11y_name = l10n_util::GetPluralStringFUTF16(
        IDS_PAGE_INFO_COOKIES_THIRD_PARTY_COOKIES_ALLOWED_TOGGLE_A11Y,
        cookie_info.allowed_third_party_sites_count);
  }
  third_party_cookies_toggle_->SetIsOn(!are_third_party_cookies_blocked);
  third_party_cookies_toggle_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_THIRD_PARTY_COOKIES_TOGGLE);
  third_party_cookies_toggle_->SetAccessibleName(a11y_name);
  third_party_cookies_toggle_subtitle_->SetText(subtitle);
  if (features::IsChromeRefresh2023()) {
    third_party_cookies_toggle_subtitle_->SetTextStyle(
        views::style::STYLE_BODY_5);
    third_party_cookies_toggle_subtitle_->SetEnabledColorId(
        ui::kColorLabelForegroundSecondary);
  }
}

void PageInfoCookiesContentView::SetDescriptionLabel(
    CookieBlocking3pcdStatus blocking_status,
    CookieControlsEnforcement enforcement,
    bool is_otr) {
  // Text on cookies description label has an embedded link to cookies settings.
  std::u16string settings_text_for_link = l10n_util::GetStringUTF16(
      blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd
          ? IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK
          : IDS_PAGE_INFO_COOKIES_SETTINGS_LINK);

  size_t offset;
  int description;
  if (enforcement == CookieControlsEnforcement::kEnforcedByTpcdGrant) {
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_GRANT_DESCRIPTION;
  } else if (blocking_status == CookieBlocking3pcdStatus::kLimited) {
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_DESCRIPTION;
  } else if (blocking_status == CookieBlocking3pcdStatus::kAll) {
    // Since prefs are set to default in Guest, we won't ever end up in this
    // branch, so `is_otr` means incognito here.
    description =
        is_otr
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_INCOGNITO_BLOCKED_COOKIES_DESCRIPTION
            : IDS_PAGE_INFO_TRACKING_PROTECTION_BLOCKED_COOKIES_DESCRIPTION;
  } else {
    description = IDS_PAGE_INFO_COOKIES_DESCRIPTION;
  }
  cookies_description_label_->SetText(
      l10n_util::GetStringFUTF16(description, settings_text_for_link, &offset));

  gfx::Range link_range(offset, offset + settings_text_for_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoCookiesContentView::CookiesSettingsLinkClicked,
          base::Unretained(this)));
  cookies_description_label_->AddStyleRange(link_range, link_style);
}

void PageInfoCookiesContentView::SetThirdPartyCookiesInfo(
    const CookiesNewInfo& cookie_info) {
  bool show_control =
      cookie_info.confidence !=
          CookieControlsBreakageConfidenceLevel::kUninitialized &&
      cookie_info.enforcement !=
          CookieControlsEnforcement::kEnforcedByTpcdGrant;

  third_party_cookies_container_->SetVisible(show_control);
  if (!show_control) {
    return;
  }

  SetThirdPartyCookiesTitleAndDescription(cookie_info);
  SetThirdPartyCookiesToggle(cookie_info);
  third_party_cookies_row_->SetIcon(
      PageInfoViewFactory::GetThirdPartyCookiesIcon(
          cookie_info.status != CookieControlsStatus::kEnabled));
  third_party_cookies_row_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_THIRD_PARTY_COOKIES_ROW);

  if (cookie_info.enforcement == CookieControlsEnforcement::kNoEnforcement) {
    third_party_cookies_label_wrapper_->SetVisible(true);
    third_party_cookies_toggle_->SetVisible(true);
    third_party_cookies_enforced_icon_->SetVisible(false);
  } else {
    // In 3PCD, tell the user if they allowed the current site via settings.
    third_party_cookies_label_wrapper_->SetVisible(
        cookie_info.blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd &&
        cookie_info.enforcement ==
            CookieControlsEnforcement::kEnforcedByCookieSetting);
    // In the enforced state, the toggle button is hidden; enforced icon is
    // shown instead of the toggle button.
    third_party_cookies_toggle_->SetVisible(false);
    third_party_cookies_enforced_icon_->SetVisible(true);
    third_party_cookies_enforced_icon_->SetImage(
        PageInfoViewFactory::GetImageModel(
            CookieControlsUtil::GetEnforcedIcon(cookie_info.enforcement)));
    third_party_cookies_enforced_icon_->SetTooltipText(
        l10n_util::GetStringUTF16(CookieControlsUtil::GetEnforcedTooltipTextId(
            cookie_info.enforcement)));
  }

  // Set the preferred width of the label wrapper to the title width. It ensures
  // that the title isn't truncated and it prevents the container expanding to
  // try to fit the description (which should be wrapped).
  const int title_width =
      third_party_cookies_title_->GetPreferredSize().width();
  third_party_cookies_label_wrapper_->SetPreferredSize(gfx::Size(
      title_width,
      third_party_cookies_label_wrapper_->GetHeightForWidth(title_width)));
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
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
    case CookieControlsEnforcement::kNoEnforcement:
      break;
  }

  // Set correct visibility for existing views.
  if (enforced_icon_) {
    enforced_icon_->SetVisible(enforced);
  }
  if (blocking_third_party_cookies_toggle_) {
    blocking_third_party_cookies_toggle_->SetVisible(!enforced);
  }

  // If it's not enforced then toggle is for sure not being changed.
  if (!enforced && blocking_third_party_cookies_toggle_) {
    return;
  }

  // Newly created views are visible by default.
  if (enforced) {
    if (!enforced_icon_) {
      enforced_icon_ = blocking_third_party_cookies_row_->AddControl(
          std::make_unique<NonAccessibleImageView>());
      enforced_icon_->SetTooltipText(l10n_util::GetStringUTF16(tooltip_id));
    }
    // If it's enforced then the icon might need to be changed.
    enforced_icon_->SetImage(PageInfoViewFactory::GetImageModel(
        CookieControlsUtil::GetEnforcedIcon(enforcement)));
  } else {
    blocking_third_party_cookies_toggle_ =
        blocking_third_party_cookies_row_->AddControl(
            std::make_unique<views::ToggleButton>(base::BindRepeating(
                &PageInfoCookiesContentView::OnToggleButtonPressed,
                base::Unretained(this))));
    blocking_third_party_cookies_toggle_->SetAccessibleName(
        l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TOGGLE_TOOLTIP));
    blocking_third_party_cookies_toggle_->SetPreferredSize(gfx::Size(
        blocking_third_party_cookies_toggle_->GetPreferredSize().width(),
        blocking_third_party_cookies_row_->GetFirstLineHeight()));
    blocking_third_party_cookies_toggle_->SetID(
        PageInfoViewFactory::
            VIEW_ID_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TOGGLE);
  }
}

void PageInfoCookiesContentView::InitBlockingThirdPartyCookiesRow() {
  if (blocking_third_party_cookies_row_) {
    return;
  }

  // |blocking_third_party_cookies_row_| has to be the first cookie button.
  blocking_third_party_cookies_row_ =
      cookies_buttons_container_view_->AddChildViewAt(
          std::make_unique<RichControlsContainerView>(), 0);
  blocking_third_party_cookies_row_->SetTitle(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_TITLE));
  blocking_third_party_cookies_row_->SetIcon(
      PageInfoViewFactory::GetBlockingThirdPartyCookiesIcon());
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
  if (base::FeatureList::IsEnabled(content_settings::features::kUserBypassUI)) {
    presenter_->OnThirdPartyToggleClicked(
        /*block_third_party_cookies=*/!third_party_cookies_toggle_->GetIsOn());
  } else {
    presenter_->OnThirdPartyToggleClicked(
        /*block_third_party_cookies=*/blocking_third_party_cookies_toggle_
            ->GetIsOn());
  }
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
  if (fps_button_) {
    return;
  }

  // Create the fps_button with temporary values for title and subtitle
  // as we don't have data yet, it will be updated.
  fps_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              &PageInfoCookiesContentView::FpsSettingsButtonClicked,
              base::Unretained(this)),
          PageInfoViewFactory::GetFpsIcon(),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES), std::u16string(),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_FPS_BUTTON_TOOLTIP),
          /*secondary_text=*/u" ", PageInfoViewFactory::GetLaunchIcon(),
          is_managed ? absl::optional<ui::ImageModel>(
                           PageInfoViewFactory::GetEnforcedByPolicyIcon())
                     : absl::nullopt));
  fps_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_FPS_SETTINGS);
}

void PageInfoCookiesContentView::FpsSettingsButtonClicked(ui::Event const&) {
  presenter_->OpenAllSitesViewFilteredToFps();
}

void PageInfoCookiesContentView::AddThirdPartyCookiesContainer() {
  auto* provider = ChromeLayoutProvider::Get();
  const int vertical_margin =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int side_margin =
      provider->GetInsetsMetric(views::INSETS_DIALOG).left();

  third_party_cookies_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  third_party_cookies_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  third_party_cookies_container_->SetVisible(false);

  third_party_cookies_label_wrapper_ =
      third_party_cookies_container_->AddChildView(
          std::make_unique<views::BoxLayoutView>());
  third_party_cookies_label_wrapper_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  third_party_cookies_label_wrapper_->SetProperty(
      views::kMarginsKey, gfx::Insets::VH(vertical_margin, side_margin));
  third_party_cookies_title_ = third_party_cookies_label_wrapper_->AddChildView(
      std::make_unique<views::Label>());
  third_party_cookies_title_->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  third_party_cookies_title_->SetTextStyle(views::style::STYLE_PRIMARY);
  third_party_cookies_title_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  if (features::IsChromeRefresh2023()) {
    third_party_cookies_title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  }

  third_party_cookies_description_ =
      third_party_cookies_label_wrapper_->AddChildView(
          std::make_unique<views::Label>());
  third_party_cookies_description_->SetTextContext(views::style::CONTEXT_LABEL);
  if (features::IsChromeRefresh2023()) {
    third_party_cookies_description_->SetTextStyle(views::style::STYLE_BODY_5);
    third_party_cookies_description_->SetEnabledColorId(
        ui::kColorLabelForegroundSecondary);
  } else {
    third_party_cookies_description_->SetTextStyle(
        views::style::STYLE_SECONDARY);
  }
  third_party_cookies_description_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  third_party_cookies_description_->SetMultiLine(true);

  third_party_cookies_row_ = third_party_cookies_container_->AddChildView(
      std::make_unique<RichControlsContainerView>());
  third_party_cookies_row_->SetTitle(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_COOKIES_THIRD_PARTY_COOKIES_LABEL));
  third_party_cookies_row_->SetIcon(
      PageInfoViewFactory::GetBlockingThirdPartyCookiesIcon());

  third_party_cookies_toggle_subtitle_ =
      third_party_cookies_row_->AddSecondaryLabel(std::u16string());

  third_party_cookies_toggle_ = third_party_cookies_row_->AddControl(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoCookiesContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  third_party_cookies_enforced_icon_ = third_party_cookies_row_->AddControl(
      std::make_unique<views::ImageView>());
}
