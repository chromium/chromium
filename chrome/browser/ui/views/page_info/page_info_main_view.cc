// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_main_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

namespace {

SkColor GetRelatedTextColor() {
  views::Label label;
  return views::style::GetColor(label, views::style::CONTEXT_LABEL,
                                views::style::STYLE_PRIMARY);
}

}  // namespace

PageInfoMainView::PageInfoMainView(PageInfo* presenter,
                                   PageInfoUiDelegate* ui_delegate,
                                   Profile* profile)
    : presenter_(presenter), ui_delegate_(ui_delegate) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // In Harmony, the last view is a HoverButton, which overrides the bottom
  // dialog inset in favor of its own. Note the multi-button value is used here
  // assuming that the "Cookies" & "Site settings" buttons will always be shown.
  const int hover_list_spacing =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  layout->AddView(CreateBubbleHeaderView());

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                              views::GridLayout::kFixedSize,
                              hover_list_spacing);
  security_view_ = layout->AddView(std::make_unique<SecurityInformationView>(
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left()));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  permissions_view_ = layout->AddView(std::make_unique<views::View>());

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  layout->AddView(std::make_unique<views::Separator>());

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                              views::GridLayout::kFixedSize,
                              hover_list_spacing);
  site_settings_view_ = layout->AddView(CreateSiteSettingsView());

  if (!profile->IsGuestSession()) {
    layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                                views::GridLayout::kFixedSize, 0);

    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP);
    site_settings_link = layout->AddView(std::make_unique<PageInfoHoverButton>(
        base::BindRepeating(
            [](PageInfoMainView* view) {
              view->HandleMoreInfoRequest(view->site_settings_link);
            },
            this),
        PageInfoUI::GetSiteSettingsIcon(GetRelatedTextColor()),
        IDS_PAGE_INFO_SITE_SETTINGS_LINK, std::u16string(),
        PageInfoMainView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS,
        tooltip, std::u16string()));
  }

#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  page_feature_info_view_ = layout->AddView(std::make_unique<views::View>());
#endif

  presenter_->InitializeUiState(this);
}

PageInfoMainView::~PageInfoMainView() = default;

void PageInfoMainView::SetCookieInfo(const CookieInfoList& cookie_info_list) {
  // Calculate the number of cookies used by this site. |cookie_info_list|
  // should only ever have 2 items: first- and third-party cookies.
  DCHECK_EQ(cookie_info_list.size(), 2u);
  int total_allowed = 0;
  for (const auto& i : cookie_info_list) {
    total_allowed += i.allowed;
  }

  // Get the string to display the number of cookies.
  const std::u16string num_cookies_text = l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_NUM_COOKIES_PARENTHESIZED, total_allowed);

  // Create the cookie button if it doesn't yet exist. This method gets called
  // each time site data is updated, so if it *does* already exist, skip this
  // part and just update the text.
  if (cookie_button_ == nullptr) {
    // Get the icon.
    PageInfo::PermissionInfo info;
    info.type = ContentSettingsType::COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    const gfx::ImageSkia icon =
        PageInfoUI::GetPermissionIcon(info, GetRelatedTextColor());

    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

    cookie_button_ =
        std::make_unique<PageInfoHoverButton>(
            base::BindRepeating(
                [](PageInfoMainView* view) {
                  view->HandleMoreInfoRequest(view->cookie_button_);
                },
                this),
            icon, IDS_PAGE_INFO_COOKIES_BUTTON_TEXT, num_cookies_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG, tooltip,
            std::u16string())
            .release();
    site_settings_view_->AddChildView(cookie_button_);
  }

  // Update the text displaying the number of allowed cookies.
  cookie_button_->SetTitleText(IDS_PAGE_INFO_COOKIES_BUTTON_TEXT,
                               num_cookies_text);

  PreferredSizeChanged();
}

void PageInfoMainView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  // This method is called when Page Info is constructed/displayed, then called
  // again whenever permissions/chosen objects change while the bubble is still
  // opened. Once Page Info is displaying a non-zero number of permissions, all
  // future calls to this will return early, based on the assumption that
  // permission rows won't need to be added or removed. Theoretically this
  // assumption is incorrect and it is actually possible that the number of
  // permission rows will need to change, but this should be an extremely rare
  // case that can be recovered from by closing & reopening the bubble.
  // TODO(patricialor): Investigate removing callsites to this method other than
  // the constructor.
  if (!permissions_view_->children().empty())
    return;

  views::GridLayout* layout = permissions_view_->SetLayoutManager(
      std::make_unique<views::GridLayout>());
  const bool is_list_empty =
      permission_info_list.empty() && chosen_object_info_list.empty();
  LayoutPermissionsLikeUiRow(layout, is_list_empty, kPermissionColumnSetId);

  // |ChosenObjectView| will layout itself, so just add the missing padding
  // here.
  constexpr int kChosenObjectSectionId = 1;
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int list_item_padding =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  views::ColumnSet* chosen_object_set =
      layout->AddColumnSet(kChosenObjectSectionId);
  chosen_object_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      side_margin);
  chosen_object_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                               1.0,
                               views::GridLayout::ColumnSize::kUsePreferred,
                               views::GridLayout::kFixedSize, 0);
  chosen_object_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      side_margin);

  int min_height_for_permission_rows = 0;
  for (const auto& permission : permission_info_list) {
    std::unique_ptr<PermissionSelectorRow> selector =
        std::make_unique<PermissionSelectorRow>(ui_delegate_, permission,
                                                layout);
    selector->AddObserver(this);
    min_height_for_permission_rows = std::max(
        min_height_for_permission_rows, selector->MinHeightForPermissionRow());
    selector_rows_.push_back(std::move(selector));
  }

  // Ensure most comboboxes are the same width by setting them all to the widest
  // combobox size, provided it does not exceed a maximum width.
  // For selected options that are over the maximum width, allow them to assume
  // their full width. If the combobox selection is changed, this may make the
  // widths inconsistent again, but that is OK since the widths will be updated
  // on the next time the bubble is opened.
  const int maximum_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH);
  int combobox_width = 0;
  for (const auto& selector : selector_rows_) {
    int curr_width = selector->GetComboboxWidth();
    if (maximum_width >= curr_width)
      combobox_width = std::max(combobox_width, curr_width);
  }
  for (const auto& selector : selector_rows_)
    selector->SetMinComboboxWidth(combobox_width);

  for (auto& object : chosen_object_info_list) {
    // Since chosen objects are presented after permissions in the same list,
    // make sure its height is the same as the permissions row's minimum height
    // plus padding.
    layout->StartRow(1.0, kChosenObjectSectionId,
                     min_height_for_permission_rows + list_item_padding);
    // The view takes ownership of the object info.
    auto object_view = std::make_unique<ChosenObjectView>(
        std::move(object),
        presenter_->GetChooserContextFromUIInfo(object->ui_info)
            ->GetObjectDisplayName(object->chooser_object->value));
    object_view->AddObserver(this);
    layout->AddView(std::move(object_view));
  }
  layout->AddPaddingRow(views::GridLayout::kFixedSize, list_item_padding);

  layout->Layout(permissions_view_);
  PreferredSizeChanged();
}

void PageInfoMainView::SetIdentityInfo(const IdentityInfo& identity_info) {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);

  title_->SetText(security_description->summary);
  SetSecurityDescriptionType(security_description->type);
  int text_style = views::style::STYLE_PRIMARY;
  switch (security_description->summary_style) {
    case SecuritySummaryColor::RED:
      text_style = STYLE_RED;
      break;
    case SecuritySummaryColor::GREEN:
      text_style = STYLE_GREEN;
      break;
  }
  title_->SetEnabledColor(views::style::GetColor(
      *this, views::style::CONTEXT_DIALOG_TITLE, text_style));

  if (identity_info.certificate) {
    certificate_ = identity_info.certificate;

    if (identity_info.show_ssl_decision_revoke_button) {
      security_view_->AddResetDecisionsLabel(base::BindRepeating(
          &PageInfoMainView::ResetDecisionsClicked, base::Unretained(this)));
    }

    // Show information about the page's certificate.
    // The text of link to the Certificate Viewer varies depending on the
    // validity of the Certificate.
    const bool valid_identity =
        (identity_info.identity_status != PageInfo::SITE_IDENTITY_STATUS_ERROR);
    std::u16string tooltip;
    if (valid_identity) {
      tooltip = l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_CERTIFICATE_VALID_LINK_TOOLTIP,
          base::UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
    } else {
      tooltip = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_CERTIFICATE_INVALID_LINK_TOOLTIP);
    }

    // Add the Certificate Section.
    const gfx::ImageSkia icon =
        PageInfoUI::GetCertificateIcon(GetRelatedTextColor());
    const std::u16string secondary_text = l10n_util::GetStringUTF16(
        valid_identity ? IDS_PAGE_INFO_CERTIFICATE_VALID_PARENTHESIZED
                       : IDS_PAGE_INFO_CERTIFICATE_INVALID_PARENTHESIZED);

    std::u16string subtitle_text;
    if (base::FeatureList::IsEnabled(features::kEvDetailsInPageInfo)) {
      // Only show the EV certificate details if there are no errors or mixed
      // content.
      if (identity_info.identity_status ==
              PageInfo::SITE_IDENTITY_STATUS_EV_CERT &&
          identity_info.connection_status ==
              PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED) {
        // An EV cert is required to have an organization name and a country.
        if (!certificate_->subject().organization_names.empty() &&
            !certificate_->subject().country_name.empty()) {
          subtitle_text = l10n_util::GetStringFUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
              base::UTF8ToUTF16(certificate_->subject().organization_names[0]),
              base::UTF8ToUTF16(certificate_->subject().country_name));
        }
      }
    }

    // If the certificate button has been added previously, remove the old one
    // before recreating it. Re-adding it bumps it to the bottom of the
    // container, but its unlikely that the user will notice, since other things
    // are changing too.
    if (certificate_button_) {
      site_settings_view_->RemoveChildView(certificate_button_);
      auto to_delete = std::make_unique<views::View*>(certificate_button_);
    }
    certificate_button_ = site_settings_view_->AddChildView(
        std::make_unique<PageInfoHoverButton>(
            base::BindRepeating(
                [](PageInfoMainView* view) {
                  view->HandleMoreInfoRequest(view->certificate_button_);
                },
                this),
            icon, IDS_PAGE_INFO_CERTIFICATE_BUTTON_TEXT, secondary_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER, tooltip,
            subtitle_text)
            .release());
  }

  if (identity_info.show_change_password_buttons) {
    security_view_->AddPasswordReuseButtons(
        identity_info.safe_browsing_status,
        base::BindRepeating(
            [](PageInfoMainView* view) {
              view->presenter_->OnChangePasswordButtonPressed();
            },
            this),
        base::BindRepeating(
            [](PageInfoMainView* view) {
              view->GetWidget()->Close();
              view->presenter_->OnAllowlistPasswordReuseButtonPressed();
            },
            this));
  }
  details_text_ = security_description->details;
  security_view_->SetDetails(
      security_description->details,
      base::BindRepeating(&PageInfoMainView::SecurityDetailsClicked,
                          base::Unretained(this)));

  Layout();
  PreferredSizeChanged();
}

void PageInfoMainView::SetPageFeatureInfo(const PageFeatureInfo& info) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  // For now, this has only VR settings.
  if (!info.is_vr_presentation_in_headset)
    return;

  auto* layout = page_feature_info_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(PageInfoUI::GetVrSettingsIcon(GetRelatedTextColor()));
  auto exit_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          [](PageInfoMainView* view) {
            view->GetWidget()->Close();
#if BUILDFLAG(ENABLE_VR)
            vr::VrTabHelper::ExitVrPresentation();
#endif
          },
          this),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_TURN_OFF_BUTTON_TEXT));
  exit_button->SetID(VIEW_ID_PAGE_INFO_BUTTON_END_VR);
  exit_button->SetProminent(true);

  auto button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(), std::move(icon),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_PRESENTING_TEXT),
      std::u16string(), std::move(exit_button),
      false,  // Try not to change the row height while adding secondary view
      true);  // Secondary view can handle events.
  button->SetID(VIEW_ID_PAGE_INFO_HOVER_BUTTON_VR_PRESENTATION);

  page_feature_info_view_->AddChildView(button.release());

  PreferredSizeChanged();
#endif
}

void PageInfoMainView::OnPermissionChanged(
    const PageInfo::PermissionInfo& permission) {
  presenter_->OnSitePermissionChanged(permission.type, permission.setting,
                                      permission.is_one_time);
  // The menu buttons for the permissions might have longer strings now, so we
  // need to layout and size the whole bubble.
  PreferredSizeChanged();
}

void PageInfoMainView::OnChosenObjectDeleted(
    const PageInfoUI::ChosenObjectInfo& info) {
  presenter_->OnSiteChosenObjectDeleted(info.ui_info,
                                        info.chooser_object->value);
}

std::unique_ptr<views::View> PageInfoMainView::CreateSiteSettingsView() {
  auto site_settings_view = std::make_unique<views::View>();
  auto* box_layout =
      site_settings_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  return site_settings_view;
}

void PageInfoMainView::HandleMoreInfoRequest(views::View* source) {
  // The bubble closes automatically when the collected cookies dialog or the
  // certificate viewer opens. So delay handling of the link clicked to avoid
  // a crash in the base class which needs to complete the mouse event handling.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PageInfoMainView::HandleMoreInfoRequestAsync,
                                weak_factory_.GetWeakPtr(), source->GetID()));
}

void PageInfoMainView::HandleMoreInfoRequestAsync(int view_id) {
  switch (view_id) {
    case PageInfoMainView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS:
      presenter_->OpenSiteSettingsView();
      break;
    case PageInfoMainView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG:
      presenter_->OpenCookiesDialog();
      break;
    case PageInfoMainView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER:
      presenter_->OpenCertificateDialog(certificate_.get());
      break;
    default:
      NOTREACHED();
  }
}

void PageInfoMainView::LayoutPermissionsLikeUiRow(views::GridLayout* layout,
                                                  bool is_list_empty,
                                                  int column_id) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  if (is_list_empty) {
    // If nothing to show, just add padding above the separator and exit.
    layout->AddPaddingRow(views::GridLayout::kFixedSize,
                          layout_provider->GetDistanceMetric(
                              views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
    return;
  }

  const int list_item_padding =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  layout->AddPaddingRow(views::GridLayout::kFixedSize, list_item_padding);

  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  // A permissions row will have an icon, title, and combobox, with a padding
  // column on either side to match the dialog insets. Note the combobox can be
  // variable widths depending on the text inside.
  // *----------------------------------------------*
  // |++| Icon | Permission Title     | Combobox |++|
  // *----------------------------------------------*
  views::ColumnSet* permissions_set = layout->AddColumnSet(column_id);
  permissions_set->AddPaddingColumn(views::GridLayout::kFixedSize, side_margin);
  permissions_set->AddColumn(
      views::GridLayout::CENTER, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::ColumnSize::kFixed,
      kIconColumnWidth, 0);
  permissions_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  permissions_set->AddColumn(views::GridLayout::LEADING,
                             views::GridLayout::CENTER, 1.0,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             views::GridLayout::kFixedSize, 0);
  permissions_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  permissions_set->AddColumn(views::GridLayout::TRAILING,
                             views::GridLayout::FILL,
                             views::GridLayout::kFixedSize,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             views::GridLayout::kFixedSize, 0);
  permissions_set->AddPaddingColumn(views::GridLayout::kFixedSize, side_margin);
}

void PageInfoMainView::ResetDecisionsClicked() {
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
  GetWidget()->Close();
}

void PageInfoMainView::SecurityDetailsClicked(const ui::Event& event) {
  if (GetSecurityDescriptionType() == SecurityDescriptionType::SAFETY_TIP)
    presenter_->OpenSafetyTipHelpCenterPage();
  else
    presenter_->OpenConnectionHelpCenterPage(event);
}

PageInfoUI::SecurityDescriptionType
PageInfoMainView::GetSecurityDescriptionType() const {
  return security_description_type_;
}

void PageInfoMainView::SetSecurityDescriptionType(
    const PageInfoUI::SecurityDescriptionType& type) {
  security_description_type_ = type;
}

gfx::Size PageInfoMainView::CalculatePreferredSize() const {
  if (site_settings_view_ == nullptr && permissions_view_ == nullptr) {
    return views::View::CalculatePreferredSize();
  }

  int width = 0;
  if (site_settings_view_) {
    width = std::max(width, site_settings_view_->GetPreferredSize().width());
    width = std::max(width, permissions_view_->GetPreferredSize().width());
  }
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

std::unique_ptr<views::View> PageInfoMainView::CreateBubbleHeaderView() {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets(0, kIconColumnWidth));
  title_ = header->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_TITLE));
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  auto close_button =
      views::BubbleFrameView::CreateCloseButton(base::BindRepeating(
          [](View* view) {
            view->GetWidget()->CloseWithReason(
                views::Widget::ClosedReason::kCloseButtonClicked);
          },
          base::Unretained(this)));
  close_button->SetVisible(true);
  header->AddChildView(close_button.release());

  return header;
}
