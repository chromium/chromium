// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_main_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
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

// The column set id of the permissions table for |permissions_view_|.
constexpr int kPermissionColumnSetId = 0;
// The column set id of the `ChosenObjectView` instances for |selector_rows_|.
constexpr int kChosenObjectSectionId = 1;
// The column set id for separators between and after permissions section.
constexpr int kSeparatorSectionId = 2;

int GetSideMargin() {
  return ChromeLayoutProvider::Get()
      ->GetInsetsMetric(views::INSETS_DIALOG)
      .left();
}

int GetImageButtonRightPadding() {
  return ChromeLayoutProvider::Get()
      ->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON)
      .right();
}

}  // namespace

PageInfoMainView::PageInfoMainView(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    PageInfoNavigationHandler* navigation_handler)
    : presenter_(presenter),
      ui_delegate_(ui_delegate),
      navigation_handler_(navigation_handler) {
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
  layout->AddPaddingRow(views::GridLayout::kFixedSize, hover_list_spacing);

#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  page_feature_info_view_ = layout->AddView(std::make_unique<views::View>());
#endif

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  security_container_view_ = layout->AddView(CreateContainerView());

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  permissions_view_ = layout->AddView(std::make_unique<views::View>());

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  site_settings_view_ = layout->AddView(CreateContainerView());

  if (ui_delegate_->ShouldShowSiteSettings()) {
    layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP);
    site_settings_link_ = layout->AddView(std::make_unique<PageInfoHoverButton>(
        base::BindRepeating(
            [](PageInfoMainView* view) {
              view->HandleMoreInfoRequest(view->site_settings_link_);
            },
            this),
        PageInfoUI::GetSiteSettingsIcon(), IDS_PAGE_INFO_SITE_SETTINGS_LINK,
        std::u16string(),
        PageInfoMainView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS,
        tooltip, std::u16string(), PageInfoUI::GetLaunchIcon()));
  }

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
      IDS_PAGE_INFO_NUM_COOKIES, total_allowed);

  // Create the cookie button if it doesn't yet exist. This method gets called
  // each time site data is updated, so if it *does* already exist, skip this
  // part and just update the text.
  if (cookie_button_ == nullptr) {
    // Get the icon.
    PageInfo::PermissionInfo info;
    info.type = ContentSettingsType::COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    const ui::ImageModel icon = PageInfoUI::GetPermissionIcon(info);

    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

    cookie_button_ =
        std::make_unique<PageInfoHoverButton>(
            base::BindRepeating(
                [](PageInfoMainView* view) {
                  view->HandleMoreInfoRequest(view->cookie_button_);
                },
                this),
            icon, IDS_PAGE_INFO_COOKIES, num_cookies_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG, tooltip,
            std::u16string(), PageInfoUI::GetLaunchIcon())
            .release();
    site_settings_view_->AddChildView(cookie_button_);
  }

  // Update the text displaying the number of allowed cookies.
  cookie_button_->SetTitleText(IDS_PAGE_INFO_COOKIES, num_cookies_text);

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

  if (permission_info_list.empty() && chosen_object_info_list.empty())
    return;

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int hover_list_spacing =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);

  views::GridLayout* layout = permissions_view_->SetLayoutManager(
      std::make_unique<views::GridLayout>());

  views::ColumnSet* separator_set = layout->AddColumnSet(kSeparatorSectionId);
  separator_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                           1.0, views::GridLayout::ColumnSize::kUsePreferred,
                           views::GridLayout::kFixedSize, 0);

  layout->StartRowWithPadding(
      views::GridLayout::kFixedSize, kSeparatorSectionId,
      views::GridLayout::kFixedSize, hover_list_spacing);
  layout->AddView(std::make_unique<views::Separator>());
  layout->AddPaddingRow(views::GridLayout::kFixedSize, hover_list_spacing);

  LayoutPermissionsLikeUiRow(layout, kPermissionColumnSetId);

  // |ChosenObjectView| will layout itself, so just add the missing padding
  // here.
  const int side_margin = GetSideMargin();
  views::ColumnSet* chosen_object_set =
      layout->AddColumnSet(kChosenObjectSectionId);
  chosen_object_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      side_margin);
  chosen_object_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                               1.0,
                               views::GridLayout::ColumnSize::kUsePreferred,
                               views::GridLayout::kFixedSize, 0);
  // Adjust right padding by the delete button's insets to align all icons on
  // the right side.
  chosen_object_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      side_margin - GetImageButtonRightPadding());
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
                     min_height_for_permission_rows);
    // The view takes ownership of the object info.
    auto object_view = std::make_unique<ChosenObjectView>(
        std::move(object),
        presenter_->GetChooserContextFromUIInfo(object->ui_info)
            ->GetObjectDisplayName(object->chooser_object->value));
    object_view->AddObserver(this);
    layout->AddView(std::move(object_view));
  }

  layout->StartRowWithPadding(
      views::GridLayout::kFixedSize, kSeparatorSectionId,
      views::GridLayout::kFixedSize, hover_list_spacing);
  layout->AddView(std::make_unique<views::Separator>());
  layout->AddPaddingRow(views::GridLayout::kFixedSize, hover_list_spacing);

  layout->Layout(permissions_view_);
  PreferredSizeChanged();
}

void PageInfoMainView::SetIdentityInfo(const IdentityInfo& identity_info) {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);

  title_->SetText(base::UTF8ToUTF16(identity_info.site_identity));

  security_container_view_->RemoveAllChildViews(true);
  if (security_description->summary_style == SecuritySummaryColor::GREEN) {
    // base::Unretained(navigation_handler_) is safe because navigation_handler_
    // is the bubble view which is the owner of this view and therefore will
    // always exist when this view exists.
    connection_button_ = security_container_view_->AddChildView(
        std::make_unique<PageInfoHoverButton>(
            base::BindRepeating(&PageInfoNavigationHandler::OpenSecurityPage,
                                base::Unretained(navigation_handler_)),
            PageInfoUI::GetConnectionSecureIcon(), 0, std::u16string(),
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION,
            std::u16string(), std::u16string(),
            PageInfoUI::GetOpenSubpageIcon())
            .release());
    connection_button_->SetTitleText(security_description->summary);
  } else {
    security_content_view_ = security_container_view_->AddChildView(
        std::make_unique<PageInfoSecurityContentView>(
            presenter_, /*is_standalone_page=*/false));
    security_content_view_->SetIdentityInfo(identity_info);
  }

  details_text_ = security_description->details;
  PreferredSizeChanged();
}

void PageInfoMainView::SetPageFeatureInfo(const PageFeatureInfo& info) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  // For now, this has only VR settings.
  if (!info.is_vr_presentation_in_headset)
    return;

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  page_feature_info_view_
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  auto* content_view =
      page_feature_info_view_->AddChildView(std::make_unique<views::View>());
  auto* flex_layout =
      content_view->SetLayoutManager(std::make_unique<views::FlexLayout>());

  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(PageInfoUI::GetVrSettingsIcon());
  content_view->AddChildView(std::move(icon));

  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_PRESENTING_TEXT),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  label->SetProperty(views::kMarginsKey, gfx::Insets(0, icon_label_spacing));
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  content_view->AddChildView(std::move(label));

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
  // Set views::kInternalPaddingKey for flex layout to account for internal
  // button padding when calculating margins.
  exit_button->SetProperty(views::kInternalPaddingKey,
                           gfx::Insets(exit_button->GetInsets().top(), 0));
  content_view->AddChildView(std::move(exit_button));

  flex_layout->SetInteriorMargin(layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON));

  // Distance for multi content list is used, but split in half, since there is
  // a separator in the middle of it.
  const int separator_spacing =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI) /
      2;
  auto* separator = page_feature_info_view_->AddChildView(
      std::make_unique<views::Separator>());
  separator->SetProperty(views::kMarginsKey, gfx::Insets(separator_spacing, 0));

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

std::unique_ptr<views::View> PageInfoMainView::CreateContainerView() {
  auto container_view = std::make_unique<views::View>();
  container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  return container_view;
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
    default:
      NOTREACHED();
  }
}

void PageInfoMainView::LayoutPermissionsLikeUiRow(views::GridLayout* layout,
                                                  int column_id) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int side_margin = GetSideMargin();
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
  title_->SetMultiLine(true);
  title_->SetAllowCharacterBreak(true);
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  auto close_button = views::BubbleFrameView::CreateCloseButton(
      base::BindRepeating(&PageInfoNavigationHandler::CloseBubble,
                          base::Unretained(navigation_handler_)));

  close_button->SetVisible(true);
  close_button->SetProperty(views::kCrossAxisAlignmentKey,
                            views::LayoutAlignment::kStart);
  // Set views::kInternalPaddingKey for flex layout to account for internal
  // button padding when calculating margins.
  close_button->SetProperty(views::kInternalPaddingKey,
                            close_button->GetInsets());
  header->AddChildView(close_button.release());

  return header;
}
