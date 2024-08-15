// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/site_data_row_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/origin.h"

// Not referenced directly but needed to use GetElementForView in tests.
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SiteDataRowView, kMenuButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SiteDataRowView, kDeleteButton);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SiteDataRowView, kAllowMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SiteDataRowView, kBlockMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SiteDataRowView, kClearOnExitMenuItem);

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSiteRowMenuItemClicked);

namespace {

constexpr int kIconSize = 16;

constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kIsolatedWebApp};
constexpr UrlIdentity::FormatOptions kUrlIdentityFormatOptions = {
    .default_options = {UrlIdentity::DefaultFormatOptions::kHostname}};

std::u16string GetSettingStateString(ContentSetting setting,
                                     bool is_fully_partitioned) {
  // TODO(crbug.com/40231917): Return actual strings.
  int message_id = -1;
  switch (setting) {
    case CONTENT_SETTING_ALLOW: {
      message_id =
          is_fully_partitioned
              ? IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE
              : IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE;
      break;
    }
    case CONTENT_SETTING_BLOCK: {
      // Partitioned cookies don't need a special call out because they are
      // blocked with the rest of the cookies.
      message_id = IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE;
      break;
    }
    case CONTENT_SETTING_SESSION_ONLY: {
      message_id =
          is_fully_partitioned
              ? IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_SESSION_ONLY_STATE_SUBTITLE
              : IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_SESSION_ONLY_STATE_SUBTITLE;
      break;
    }
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_ASK:
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_NUM_SETTINGS:
      // Not supported settings for cookies.
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(message_id);
}

std::unique_ptr<views::TableLayout> SetupTableLayout() {
  const auto dialog_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int button_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  auto layout = std::make_unique<views::TableLayout>();
  layout
      ->AddPaddingColumn(views::TableLayout::kFixedSize, dialog_insets.left())
      // Favicon.
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // Host name.
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, icon_label_spacing)
      // Delete icon.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, kIconSize)
      .AddPaddingColumn(views::TableLayout::kFixedSize, button_spacing)
      // Menu icon.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, kIconSize)
      .AddPaddingColumn(views::TableLayout::kFixedSize, dialog_insets.right())
      .AddRows(1, views::TableLayout::kFixedSize);
  return layout;
}

void NotifyMenuItemClicked(views::View* view) {
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      views::ElementTrackerViews::GetInstance()->GetElementForView(view),
      kSiteRowMenuItemClicked);
}

}  // namespace

SiteDataRowView::SiteDataRowView(
    Profile* profile,
    const url::Origin& origin,
    ContentSetting setting,
    bool is_fully_partitioned,
    FaviconCache* favicon_cache,
    base::RepeatingCallback<void(const url::Origin&)> delete_callback,
    base::RepeatingCallback<void(const url::Origin&, ContentSetting)>
        create_exception_callback)
    : origin_(origin),
      setting_(setting),
      is_fully_partitioned_(is_fully_partitioned),
      delete_callback_(std::move(delete_callback)),
      create_exception_callback_(std::move(create_exception_callback)) {
  const int vertical_padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  views::TableLayout* layout = SetLayoutManager(SetupTableLayout());
  favicon_image_ = AddChildView(std::make_unique<NonAccessibleImageView>());
  favicon_image_->SetImage(
      ui::ImageModel::FromVectorIcon(kGlobeIcon, ui::kColorIcon, kIconSize));

  // It's safe to bind to this here because both the row view and the favicon
  // service have the same lifetime and all be destroyed when the dialog is
  // being destroyed.
  const auto favicon = favicon_cache->GetFaviconForPageUrl(
      origin.GetURL(), base::BindOnce(&SiteDataRowView::SetFaviconImage,
                                      base::Unretained(this)));
  if (!favicon.IsEmpty())
    SetFaviconImage(favicon);

  std::u16string origin_display_name =
      UrlIdentity::CreateFromUrl(profile, origin.GetURL(),
                                 kUrlIdentityAllowedTypes,
                                 kUrlIdentityFormatOptions)
          .name;
  hostname_label_ =
      AddChildView(std::make_unique<views::Label>(origin_display_name));
  hostname_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* delete_button_container = AddChildView(std::make_unique<views::View>());
  delete_button_container->SetUseDefaultFillLayout(true);
  delete_button_ = delete_button_container->AddChildView(
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&SiteDataRowView::OnDeleteIconClicked,
                              base::Unretained(this)),
          kTrashCanIcon, kIconSize));
  views::InstallCircleHighlightPathGenerator(delete_button_);
  delete_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_DELETE_BUTTON_TOOLTIP,
      origin_display_name));
  delete_button_->SetVisible(setting_ != CONTENT_SETTING_BLOCK);
  delete_button_->SetProperty(views::kElementIdentifierKey, kDeleteButton);

  // TODO(crbug.com/40231917): Use actual strings.
  menu_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SiteDataRowView::OnMenuIconClicked,
                          base::Unretained(this)),
      kBrowserToolsIcon, kIconSize));
  menu_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_CONTEXT_MENU_TOOLTIP,
      origin_display_name));
  menu_button_->SetProperty(views::kElementIdentifierKey, kMenuButton);
  views::InstallCircleHighlightPathGenerator(menu_button_);

  layout->AddRows(1, views::TableLayout::kFixedSize);
  AddChildView(std::make_unique<views::View>());
  state_label_ = AddChildView(std::make_unique<views::Label>(
      GetSettingStateString(setting_, is_fully_partitioned_),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  state_label_->SetVisible(is_fully_partitioned_ ||
                           setting_ != CONTENT_SETTING_ALLOW);
  state_label_->SetMultiLine(true);
  layout->AddPaddingRow(views::TableLayout::kFixedSize, vertical_padding);
}

SiteDataRowView::~SiteDataRowView() = default;

void SiteDataRowView::SetFaviconImage(const gfx::Image& image) {
  favicon_image_->SetImage(ui::ImageModel::FromImage(image));
}

void SiteDataRowView::OnMenuIconClicked() {
  // TODO(crbug.com/40231917): Use actual strings.
  // TODO(crbug.com/40231917): Respect partitioned cookies state and provide
  // special options for it.
  auto builder = ui::DialogModel::Builder();
  if (setting_ != CONTENT_SETTING_BLOCK) {
    // TODO(crbug.com/40231917): Consider clearing the data before blocking the
    // site to have a clean slate.
    builder.AddMenuItem(
        ui::ImageModel(),
        l10n_util::GetStringUTF16(
            IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCK_MENU_ITEM),
        base::BindRepeating(&SiteDataRowView::OnBlockMenuItemClicked,
                            base::Unretained(this)),
        ui::DialogModelMenuItem::Params().SetId(kBlockMenuItem));
  }
  if (setting_ != CONTENT_SETTING_ALLOW || is_fully_partitioned_) {
    builder.AddMenuItem(
        ui::ImageModel(),
        l10n_util::GetStringUTF16(
            is_fully_partitioned_
                ? IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOW_THIRD_PARTY_MENU_ITEM
                : IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOW_MENU_ITEM),
        base::BindRepeating(&SiteDataRowView::OnAllowMenuItemClicked,
                            base::Unretained(this)),
        ui::DialogModelMenuItem::Params().SetId(kAllowMenuItem));
  }
  if (setting_ != CONTENT_SETTING_SESSION_ONLY) {
    builder.AddMenuItem(
        ui::ImageModel(),
        l10n_util::GetStringUTF16(
            IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_SESSION_ONLY_MENU_ITEM),
        base::BindRepeating(&SiteDataRowView::OnClearOnExitMenuItemClicked,
                            base::Unretained(this)),
        ui::DialogModelMenuItem::Params().SetId(kClearOnExitMenuItem));
  }

  dialog_model_ =
      std::make_unique<ui::DialogModelMenuModelAdapter>(builder.Build());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      dialog_model_.get(), views::MenuRunner::HAS_MNEMONICS,
      base::BindRepeating(&SiteDataRowView::OnMenuClosed,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(GetWidget(), nullptr,
                          menu_button_->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::MenuSourceType::MENU_SOURCE_MOUSE);
  menu_button_->SetState(views::Button::ButtonState::STATE_PRESSED);
}

void SiteDataRowView::OnMenuClosed() {
  menu_runner_.reset();
  dialog_model_.reset();
  menu_button_->SetState(views::Button::ButtonState::STATE_NORMAL);
}

void SiteDataRowView::OnDeleteIconClicked() {
  DCHECK_NE(setting_, CONTENT_SETTING_BLOCK);
  delete_callback_.Run(origin_);

  // Hiding the view instead of trying to delete makes the lifecycle management
  // easier. All the related items to the dialog have the same lifecycle and are
  // created when dialog is shown and are deleted when the dialog is destroyed.
  SetVisible(false);

  // The row is hidden, advance focus to the next row if the delete button was
  // focused.
  if (delete_button_->HasFocus())
    GetFocusManager()->AdvanceFocus(/*reverse=*/false);
}

void SiteDataRowView::OnBlockMenuItemClicked(int event_flags) {
  SetContentSettingException(CONTENT_SETTING_BLOCK);
}

void SiteDataRowView::OnAllowMenuItemClicked(int event_flags) {
  SetContentSettingException(CONTENT_SETTING_ALLOW);
}

void SiteDataRowView::OnClearOnExitMenuItemClicked(int event_flags) {
  SetContentSettingException(CONTENT_SETTING_SESSION_ONLY);
}

void SiteDataRowView::SetContentSettingException(ContentSetting setting) {
  // For partitioned access, it's valid to create an allow exception that
  // matches current effective setting to allow 3PC.
  if (!is_fully_partitioned_ || setting_ != CONTENT_SETTING_ALLOW)
    DCHECK_NE(setting_, setting);

  create_exception_callback_.Run(origin_, setting);

  setting_ = setting;
  // After creating an explicit exception for the site, don't show the state as
  // partitioned because the exception applies to all cookies.
  is_fully_partitioned_ = false;

  state_label_->SetVisible(true);
  state_label_->SetText(GetSettingStateString(setting_, is_fully_partitioned_));
  delete_button_->SetVisible(setting_ != CONTENT_SETTING_BLOCK);

  NotifyMenuItemClicked(this);
}

BEGIN_METADATA(SiteDataRowView)
END_METADATA
