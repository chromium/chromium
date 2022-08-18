// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/site_data_row_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"
#include "url/origin.h"

namespace {

std::u16string GetSettingStateString(ContentSetting setting) {
  // TODO(crbug.com/1344787): Return actual strings.
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return u"Allowed";
    case CONTENT_SETTING_BLOCK:
      return u"Blocked";
    case CONTENT_SETTING_SESSION_ONLY:
      return u"Clear on close";
    default:
      NOTREACHED();
      return u"";
  }
}

std::unique_ptr<views::TableLayout> SetupTableLayout() {
  const auto dialog_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG);
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
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
      // Menu icon.
      .AddColumn(views::LayoutAlignment::kEnd, views::LayoutAlignment::kStretch,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, dialog_insets.right())
      .AddRows(1, views::TableLayout::kFixedSize);
  return layout;
}

}  // namespace

SiteDataRowView::SiteDataRowView(const url::Origin& origin,
                                 ContentSetting setting,
                                 FaviconCache* favicon_cache)
    : setting_(setting) {
  const int icon_size = 16;
  views::TableLayout* layout = SetLayoutManager(SetupTableLayout());
  favicon_image_ = AddChildView(std::make_unique<NonAccessibleImageView>());
  favicon_image_->SetImage(
      ui::ImageModel::FromVectorIcon(kGlobeIcon, ui::kColorIcon, icon_size));

  // It's safe to bind to this here because both the row view and the favicon
  // service have the same lifetime and all be destroyed when the dialog is
  // being destroyed.
  const auto favicon = favicon_cache->GetFaviconForPageUrl(
      origin.GetURL(), base::BindOnce(&SiteDataRowView::SetFaviconImage,
                                      base::Unretained(this)));
  if (!favicon.IsEmpty())
    SetFaviconImage(favicon);

  // TODO(crbug.com/1344787): Use proper formatting of the host.
  auto* label = AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(origin.host())));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // TODO(crbug.com/1344787): Use actual strings.
  auto* menu = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SiteDataRowView::OnMenuIconClicked,
                          base::Unretained(this)),
      kBrowserToolsIcon, icon_size));
  menu->SetAccessibleName(u"Open context menu");

  layout->AddRows(1, views::TableLayout::kFixedSize);
  AddChildView(std::make_unique<views::View>());
  state_label_ = AddChildView(std::make_unique<views::Label>(
      GetSettingStateString(setting_), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  state_label_->SetVisible(setting_ != CONTENT_SETTING_ALLOW);
}

void SiteDataRowView::SetFaviconImage(const gfx::Image& image) {
  favicon_image_->SetImage(ui::ImageModel::FromImage(image));
}

void SiteDataRowView::OnMenuIconClicked() {
  // TODO(crbug.com/1344787): Use actual strings.
  // TODO(crbug.com/1344787): Respect partitioned cookies state and provide
  // special options for it.
  auto builder = ui::DialogModel::Builder();
  builder.AddMenuItem(
      ui::ImageModel(), u"Delete",
      base::BindRepeating(&SiteDataRowView::OnDeleteMenuItemClicked,
                          base::Unretained(this)));

  if (setting_ != CONTENT_SETTING_BLOCK) {
    builder.AddMenuItem(
        ui::ImageModel(), u"Don't allow",
        base::BindRepeating(&SiteDataRowView::OnBlockMenuItemClicked,
                            base::Unretained(this)));
  }
  if (setting_ != CONTENT_SETTING_ALLOW) {
    builder.AddMenuItem(
        ui::ImageModel(), u"Allow",
        base::BindRepeating(&SiteDataRowView::OnAllowMenuItemClicked,
                            base::Unretained(this)));
  }
  if (setting_ != CONTENT_SETTING_SESSION_ONLY) {
    builder.AddMenuItem(
        ui::ImageModel(), u"Clear when you close Chrome",
        base::BindRepeating(&SiteDataRowView::OnClearOnExitMenuItemClicked,
                            base::Unretained(this)));
  }

  auto dialog_model =
      std::make_unique<ui::DialogModelMenuModelAdapter>(builder.Build());
  auto menu_runner = std::make_unique<views::MenuRunner>(
      dialog_model.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner->RunMenuAt(GetWidget(), nullptr, GetBoundsInScreen(),
                         views::MenuAnchorPosition::kTopLeft,
                         ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void SiteDataRowView::OnDeleteMenuItemClicked(int event_flags) {
  // TODO(crbug.com/1344787): Delete the stored data.
  // Hiding the view instead of trying to delete makes the lifecycle management
  // easier. All the related items to the dialog have the same lifecycle and are
  // created when dialog is shown and are deleted when the dialog is destroyed.
  SetVisible(false);
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
  DCHECK_NE(setting_, setting);
  // TODO(crbug.com/1344787): Create the exception.

  setting_ = setting;
  state_label_->SetVisible(true);
  state_label_->SetText(GetSettingStateString(setting_));
}
