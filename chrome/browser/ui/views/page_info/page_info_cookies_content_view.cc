// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/content_settings/browser/ui/cookie_controls_util.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/content_settings/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#endif

namespace {

using ::content_settings::CookieControlsUtil;

const ui::ImageModel GetThirdPartyCookiesIcon(
    bool third_party_cookies_enabled) {
  return PageInfoViewFactory::GetImageModel(
      third_party_cookies_enabled ? views::kEyeRefreshIcon
                                  : views::kEyeCrossedRefreshIcon);
}

class ThirdPartyCookieLabelWrapper : public views::BoxLayoutView {
  METADATA_HEADER(ThirdPartyCookieLabelWrapper, views::BoxLayoutView)

 public:
  explicit ThirdPartyCookieLabelWrapper(std::unique_ptr<views::View> title) {
    auto* provider = ChromeLayoutProvider::Get();

    const int vertical_margin =
        provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
    const int side_margin =
        provider->GetInsetsMetric(views::INSETS_DIALOG).left();

    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetProperty(views::kMarginsKey,
                gfx::Insets::VH(vertical_margin, side_margin));

    title_ = AddChildView(std::move(title));
  }

  ~ThirdPartyCookieLabelWrapper() override = default;

 private:
  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Set the preferred width of the label wrapper to the title width. It
    // ensures that the title isn't truncated and it prevents the container
    // expanding to try to fit the description (which should be wrapped).
    const int title_width =
        title_->GetPreferredSize(views::SizeBounds(title_->width(), {}))
            .width();
    DCHECK(available_size.width() >= title_width);
    const int available_width = available_size.width().is_bounded() &&
                                        available_size.width() > title_width
                                    ? available_size.width().value()
                                    : title_width;
    return views::BoxLayoutView::CalculatePreferredSize(
        views::SizeBounds(available_width, {}));
  }

  raw_ptr<views::View> title_ = nullptr;
};

BEGIN_METADATA(ThirdPartyCookieLabelWrapper)
END_METADATA

class CookiesDescriptionLabelWrapper : public views::View {
  METADATA_HEADER(CookiesDescriptionLabelWrapper, views::View)

 public:
  explicit CookiesDescriptionLabelWrapper(
      int max_width,
      std::unique_ptr<views::StyledLabel> label)
      : max_width_(max_width) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    label_ = AddChildView(std::move(label));
  }
  ~CookiesDescriptionLabelWrapper() override = default;

 private:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    CHECK(label_);
    const int target_width = available_size.width().is_bounded()
                                 ? available_size.width().value()
                                 : max_width_;

    return gfx::Size(target_width, label_->GetHeightForWidth(target_width));
  }
  const int max_width_;
  raw_ptr<views::StyledLabel> label_ = nullptr;
};

BEGIN_METADATA(CookiesDescriptionLabelWrapper)
END_METADATA

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoCookiesContentView,
                                      kCookieDialogButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoCookiesContentView, kCookiesPage);

PageInfoCookiesContentView::PageInfoCookiesContentView(PageInfo* presenter)
    : presenter_(presenter) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  // The last view is a RichHoverButton, which overrides the bottom
  // dialog inset in favor of its own.
  SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, bottom_margin, 0));
  const auto button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);

  auto label = std::make_unique<views::StyledLabel>();
  cookies_description_label_ = label.get();
  cookies_description_label_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_DESCRIPTION_LABEL);
  cookies_description_label_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  cookies_description_label_->SetDefaultEnabledColorId(
      kColorPageInfoForeground);
  const int max_label_width =
      PageInfoViewFactory::kMinBubbleWidth - button_insets.width();
  // Use a wrapper for the description label to ensure its height is set
  // correctly after subsequent text changes and styling.
  cookies_description_wrapper_ =
      AddChildView(std::make_unique<CookiesDescriptionLabelWrapper>(
          max_label_width, std::move(label)));
  cookies_description_wrapper_->SetProperty(views::kMarginsKey, button_insets);

  AddThirdPartyCookiesContainer();

#if BUILDFLAG(IS_CHROMEOS)
  MaybeAddSyncDisclaimer();
#endif

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
          /*subtitle_text=*/u" ", PageInfoViewFactory::GetLaunchIcon()));
  cookies_dialog_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  cookies_dialog_button_->SetProperty(views::kElementIdentifierKey,
                                      kCookieDialogButton);
  cookies_dialog_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_DIALOG_BUTTON_TOOLTIP));
  cookies_dialog_button_->SetTitleTextStyleAndColor(
      views::style::STYLE_BODY_3_MEDIUM, kColorPageInfoForeground);
  cookies_dialog_button_->SetSubtitleTextStyleAndColor(
      views::style::STYLE_BODY_4, kColorPageInfoSubtitleForeground);
}

void PageInfoCookiesContentView::CookiesSettingsLinkClicked(
    const ui::Event& event) {
  presenter_->OpenCookiesSettingsView();
}

void PageInfoCookiesContentView::SyncSettingsLinkClicked(
    const ui::Event& event) {
  presenter_->OpenSyncSettingsView();
}

void PageInfoCookiesContentView::SetCookieInfo(const CookiesInfo& cookie_info) {
  SetCookiesDescription(cookie_info.blocking_status, cookie_info.enforcement,
                        cookie_info.is_incognito);
  SetThirdPartyCookiesInfo(cookie_info.controls_state, cookie_info.enforcement,
                           cookie_info.blocking_status, cookie_info.expiration);

  // Ensure the separator is only initialized once.
  if (!cookies_dialog_button_) {
    cookies_buttons_container_view_->AddChildView(
        PageInfoViewFactory::CreateSeparator(
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)));
  }
  InitCookiesDialogButton();
  // Update the text displaying the number of allowed sites.
  cookies_dialog_button_->SetSubtitleText(l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
      cookie_info.allowed_sites_count));

  SetRwsCookiesInfo(cookie_info.rws_info);

  PreferredSizeChanged();
  if (!initialized_callback_.is_null()) {
    std::move(initialized_callback_).Run();
  }
}

void PageInfoCookiesContentView::SetThirdPartyCookiesTitleAndDescription(
    CookieControlsState controls_state,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration) {
  std::u16string title_text;
  int description;
  switch (controls_state) {
    case CookieControlsState::kBlocked3pc:
      title_text = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE);
      description =
          IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY;
      break;
    case CookieControlsState::kAllowed3pc:
      if (expiration.is_null() ||
          enforcement == CookieControlsEnforcement::kEnforcedByCookieSetting) {
        // Handle permanent site exception.
        title_text = l10n_util::GetStringUTF16(
            IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE);
        description =
            IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION;
      } else {
        // Handle temporary site exception.
        title_text = l10n_util::GetPluralStringFUTF16(
            blocking_status == CookieBlocking3pcdStatus::kLimited
                ? IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITED_RESTART_TITLE
                : IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED_RESTART_TITLE,
            CookieControlsUtil::GetDaysToExpiration(expiration));
        description =
            IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION;
      }
      break;
    default:
      NOTREACHED();
  }
  third_party_cookies_title_->SetText(title_text);
  third_party_cookies_description_->SetText(
      l10n_util::GetStringUTF16(description));
}

void PageInfoCookiesContentView::SetThirdPartyCookiesToggle(
    CookieControlsState controls_state,
    CookieBlocking3pcdStatus blocking_status) {
  std::u16string subtitle;
  if (controls_state == CookieControlsState::kBlocked3pc) {
    subtitle = l10n_util::GetStringUTF16(
        blocking_status == CookieBlocking3pcdStatus::kLimited
            ? IDS_TRACKING_PROTECTION_BUBBLE_3PC_LIMITED_SUBTITLE
            : IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE);
  } else {
    subtitle = l10n_util::GetStringUTF16(
        IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE);
  }
  third_party_cookies_toggle_->SetIsOn(controls_state ==
                                       CookieControlsState::kAllowed3pc);
  third_party_cookies_toggle_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_THIRD_PARTY_COOKIES_TOGGLE);
  third_party_cookies_toggle_->GetViewAccessibility().SetName(subtitle);
  third_party_cookies_toggle_subtitle_->SetText(subtitle);
}

void PageInfoCookiesContentView::SetCookiesDescription(
    CookieBlocking3pcdStatus blocking_status,
    CookieControlsEnforcement enforcement,
    bool is_incognito) {
  // Text on cookies description label has an embedded link to cookies settings.
  std::u16string settings_text_for_link = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK);

  size_t offset;
  int description;
  if (blocking_status == CookieBlocking3pcdStatus::kNotIn3pcd) {
    description = IDS_PAGE_INFO_COOKIES_DESCRIPTION;
    settings_text_for_link =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SETTINGS_LINK);
  } else if (enforcement == CookieControlsEnforcement::kEnforcedByTpcdGrant) {
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_GRANT_DESCRIPTION;
  } else if (blocking_status == CookieBlocking3pcdStatus::kLimited) {
    description = IDS_PAGE_INFO_TRACKING_PROTECTION_DESCRIPTION;
  } else {
    description =
        is_incognito
            ? IDS_PAGE_INFO_TRACKING_PROTECTION_INCOGNITO_BLOCKED_COOKIES_DESCRIPTION
            : IDS_PAGE_INFO_TRACKING_PROTECTION_BLOCKED_COOKIES_DESCRIPTION;
  }
  cookies_description_label_->SetText(
      l10n_util::GetStringFUTF16(description, settings_text_for_link, &offset));

  gfx::Range link_range(offset, offset + settings_text_for_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoCookiesContentView::CookiesSettingsLinkClicked,
          base::Unretained(this)));
  link_style.text_style = views::style::STYLE_LINK_3;
  cookies_description_label_->AddStyleRange(link_range, link_style);
}

void PageInfoCookiesContentView::SetThirdPartyCookiesInfo(
    CookieControlsState controls_state,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration) {
  if (controls_state == CookieControlsState::kHidden) {
    third_party_cookies_container_->SetVisible(false);
    return;
  }
  third_party_cookies_container_->SetVisible(true);
  SetThirdPartyCookiesTitleAndDescription(controls_state, enforcement,
                                          blocking_status, expiration);
  SetThirdPartyCookiesToggle(controls_state, blocking_status);
  third_party_cookies_row_->SetIcon(GetThirdPartyCookiesIcon(
      controls_state == CookieControlsState::kAllowed3pc));
  third_party_cookies_row_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_THIRD_PARTY_COOKIES_ROW);

  third_party_cookies_row_->SetVisible(true);
  third_party_cookies_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  bool show_controls_description =
      enforcement == CookieControlsEnforcement::kNoEnforcement ||
      (blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd &&
       enforcement == CookieControlsEnforcement::kEnforcedByCookieSetting);
  third_party_cookies_label_wrapper_->SetVisible(show_controls_description);

  if (enforcement == CookieControlsEnforcement::kNoEnforcement) {
    third_party_cookies_toggle_->SetVisible(true);
    third_party_cookies_enforced_icon_->SetVisible(false);
  } else {
    // In the enforced state, the toggle button is hidden; enforced icon is
    // shown instead of the toggle button.
    third_party_cookies_toggle_->SetVisible(false);
    third_party_cookies_enforced_icon_->SetVisible(true);
    third_party_cookies_enforced_icon_->SetImage(
        PageInfoViewFactory::GetImageModel(
            CookieControlsUtil::GetEnforcedIcon(enforcement)));
    third_party_cookies_enforced_icon_->SetTooltipText(
        CookieControlsUtil::GetEnforcedTooltip(enforcement));
  }
}

void PageInfoCookiesContentView::UpdateBlockingThirdPartyCookiesToggle(
    bool are_cookies_blocked) {
  DCHECK(blocking_third_party_cookies_toggle_);
  blocking_third_party_cookies_toggle_->SetIsOn(are_cookies_blocked);
}

void PageInfoCookiesContentView::OnToggleButtonPressed() {
  presenter_->OnThirdPartyToggleClicked(
      /*block_third_party_cookies=*/!third_party_cookies_toggle_->GetIsOn());
  third_party_cookies_container_->NotifyAccessibilityEventDeprecated(
      ax::mojom::Event::kAlert, true);
}

void PageInfoCookiesContentView::SetRwsCookiesInfo(
    std::optional<CookiesRwsInfo> rws_info) {
  if (rws_info.has_value()) {
    InitRwsButton(rws_info->is_managed);
    rws_button_->SetVisible(true);

    // Update the text displaying the name of RWS owner.
    rws_button_->SetTitleText(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_RWS_BUTTON_TITLE));
    rws_button_->SetSubtitleText(l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_RWS_BUTTON_SUBTITLE, rws_info->owner_name));
  } else if (rws_button_) {
    rws_button_->SetVisible(false);
  }
  if (!rws_histogram_recorded_) {
    rws_histogram_recorded_ = true;
    base::UmaHistogramBoolean("Security.PageInfo.Cookies.HasFPSInfo",
                              rws_info.has_value());
  }
}

void PageInfoCookiesContentView::InitRwsButton(bool is_managed) {
  if (rws_button_) {
    return;
  }

  // Create the `rws_button_` with temporary values for title and subtitle
  // as we don't have data yet, it will be updated.
  rws_button_ = cookies_buttons_container_view_->AddChildView(
      std::make_unique<RichHoverButton>(
          base::BindRepeating(
              &PageInfoCookiesContentView::RwsSettingsButtonClicked,
              base::Unretained(this)),
          PageInfoViewFactory::GetImageModel(vector_icons::kTenancyIcon),
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES),
          /*secondary_text=*/u" ", PageInfoViewFactory::GetLaunchIcon(),
          is_managed
              ? PageInfoViewFactory::GetImageModel(vector_icons::kBusinessIcon)
              : ui::ImageModel()));
  rws_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_RWS_SETTINGS);
  rws_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_RWS_BUTTON_TOOLTIP));
  rws_button_->SetTitleTextStyleAndColor(views::style::STYLE_BODY_3_MEDIUM,
                                         kColorPageInfoForeground);
  rws_button_->SetSubtitleTextStyleAndColor(views::style::STYLE_BODY_4,
                                            kColorPageInfoSubtitleForeground);
}

void PageInfoCookiesContentView::RwsSettingsButtonClicked(ui::Event const&) {
  presenter_->OpenAllSitesViewFilteredToRws();
}

void PageInfoCookiesContentView::AddThirdPartyCookiesContainer() {
  third_party_cookies_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  third_party_cookies_container_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kAlert);
  third_party_cookies_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  third_party_cookies_container_->SetVisible(false);
  third_party_cookies_label_wrapper_ =
      third_party_cookies_container_->AddChildView(
          std::make_unique<ThirdPartyCookieLabelWrapper>(
              views::Builder<views::Label>()
                  .CopyAddressTo(&third_party_cookies_title_)
                  .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                  .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
                  .SetEnabledColor(kColorPageInfoForeground)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .Build()));

  third_party_cookies_description_ =
      third_party_cookies_label_wrapper_->AddChildView(
          std::make_unique<views::Label>());
  third_party_cookies_description_->SetTextContext(views::style::CONTEXT_LABEL);
  third_party_cookies_description_->SetTextStyle(views::style::STYLE_BODY_4);
  third_party_cookies_description_->SetEnabledColor(
      kColorPageInfoSubtitleForeground);
  third_party_cookies_description_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  third_party_cookies_description_->SetMultiLine(true);

  third_party_cookies_row_ = third_party_cookies_container_->AddChildView(
      std::make_unique<RichControlsContainerView>());
  third_party_cookies_row_->SetTitle(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_COOKIES_THIRD_PARTY_COOKIES_LABEL));
  third_party_cookies_row_->SetIcon(
      PageInfoViewFactory::GetImageModel(views::kEyeCrossedRefreshIcon));
  third_party_cookies_row_->SetTitleTextStyleAndColor(
      views::style::STYLE_BODY_3_MEDIUM, kColorPageInfoForeground);

  third_party_cookies_toggle_subtitle_ =
      third_party_cookies_row_->AddSecondaryLabel(std::u16string());
  third_party_cookies_toggle_subtitle_->SetTextStyle(
      views::style::STYLE_BODY_4);
  third_party_cookies_toggle_subtitle_->SetEnabledColor(
      kColorPageInfoSubtitleForeground);

  third_party_cookies_toggle_ = third_party_cookies_row_->AddControl(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoCookiesContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  third_party_cookies_enforced_icon_ = third_party_cookies_row_->AddControl(
      std::make_unique<views::ImageView>());
}

#if BUILDFLAG(IS_CHROMEOS)
void PageInfoCookiesContentView::MaybeAddSyncDisclaimer() {
  if (!ash::features::IsFloatingSsoAllowed()) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(
      presenter_->web_contents()->GetBrowserContext());
  // Floating SSO is an internal name for the feature which can sync cookies for
  // ChromeOS enterprise users.
  ash::floating_sso::FloatingSsoService* floating_sso_service =
      ash::floating_sso::FloatingSsoServiceFactory::GetForProfile(profile);
  if (!floating_sso_service) {
    return;
  }
  if (!floating_sso_service->IsFloatingSsoEnabled()) {
    return;
  }
  // Even when cookie sync is enabled, it isn't applied to every site.
  if (!floating_sso_service->ShouldSyncCookiesForUrl(presenter_->site_url())) {
    return;
  }

  const ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  AddChildView(
      PageInfoViewFactory::CreateSeparator(layout_provider->GetDistanceMetric(
          DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)));

  // Cookie sync disclaimer consists of an enterprise icon and a text with a
  // link to Chrome Sync settings.
  cookies_sync_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  cookies_sync_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  const auto button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);
  cookies_sync_container_->SetProperty(views::kMarginsKey, button_insets);
  // Make the distance between the icon and the text be the same as in
  // RichHoverButton. For consistency with children of
  // `cookies_buttons_container_view_`.
  const int child_spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL);
  cookies_sync_container_->SetBetweenChildSpacing(child_spacing);

  // Add the enterprise icon.
  cookies_sync_icon_ = cookies_sync_container_->AddChildView(
      std::make_unique<NonAccessibleImageView>());
  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  cookies_sync_icon_->SetImageSize({icon_size, icon_size});
  cookies_sync_icon_->SetImage(
      PageInfoViewFactory::GetImageModel(vector_icons::kBusinessIcon));

  // Add the description.
  cookies_sync_description_ = cookies_sync_container_->AddChildView(
      std::make_unique<views::StyledLabel>());
  cookies_sync_description_->SetDefaultTextStyle(views::style::STYLE_BODY_3);
  cookies_sync_description_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_COOKIES_SYNC);
  cookies_sync_description_->SetDefaultEnabledColorId(kColorPageInfoForeground);
  cookies_sync_description_->SizeToFit(PageInfoViewFactory::kMinBubbleWidth -
                                       button_insets.width() - icon_size -
                                       child_spacing);
  cookies_sync_description_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);

  std::u16string sync_settings_text_for_link =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SYNC_SETTINGS_LINK);
  size_t offset;
  cookies_sync_description_->SetText(
      l10n_util::GetStringFUTF16(IDS_PAGE_INFO_COOKIE_SYNC_DESCRIPTION,
                                 sync_settings_text_for_link, &offset));

  // Add the link to Chrome Sync settings.
  gfx::Range link_range(offset, offset + sync_settings_text_for_link.length());
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PageInfoCookiesContentView::SyncSettingsLinkClicked,
          base::Unretained(this)));
  link_style.text_style = views::style::STYLE_LINK_3;
  cookies_sync_description_->AddStyleRange(link_range, link_style);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
