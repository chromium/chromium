// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/site_data_row_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

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

SiteDataRowView::SiteDataRowView(const GURL& url) {
  const int icon_size = 16;
  views::TableLayout* layout = SetLayoutManager(SetupTableLayout());
  auto* icon = AddChildView(std::make_unique<NonAccessibleImageView>());
  // TODO(crbug.com/1344787): Use the favicon if available.
  icon->SetImage(
      ui::ImageModel::FromVectorIcon(kGlobeIcon, ui::kColorIcon, icon_size));

  // TODO(crbug.com/1344787): Use proper formatting of the host.
  auto* label = AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(url.host())));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // TODO(crbug.com/1344787): Use actual strings.
  auto* menu = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SiteDataRowView::OnMenuIconClicked,
                          base::Unretained(this)),
      kBrowserToolsIcon, icon_size));
  menu->SetAccessibleName(u"Open context menu");

  // TODO(crbug.com/1344787): Set the actual state based on the cookie setting.
  // Show the state label when the state != allowed.
  // Use actual strings.
  layout->AddRows(1, views::TableLayout::kFixedSize);
  AddChildView(std::make_unique<views::View>());
  auto* state_label = AddChildView(std::make_unique<views::Label>(
      u"Blocked", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  state_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void SiteDataRowView::OnMenuIconClicked() {
  // TODO(crbug.com/1344787): Use actual strings.
  // TODO(crbug.com/1344787): Don't show the menu item that is equal to the
  // current content setting.
  auto dialog_model = std::make_unique<ui::DialogModelMenuModelAdapter>(
      ui::DialogModel::Builder()
          .AddMenuItem(
              ui::ImageModel(), u"Delete",
              base::BindRepeating(&SiteDataRowView::OnDeleteMenuItemClicked,
                                  base::Unretained(this)))
          .AddMenuItem(
              ui::ImageModel(), u"Don't allow",
              base::BindRepeating(&SiteDataRowView::OnBlockMenuItemClicked,
                                  base::Unretained(this)))
          .AddMenuItem(
              ui::ImageModel(), u"Allow",
              base::BindRepeating(&SiteDataRowView::OnAllowMenuItemClicked,
                                  base::Unretained(this)))
          .AddMenuItem(ui::ImageModel(), u"Clear when you close Chrome",
                       base::BindRepeating(
                           &SiteDataRowView::OnClearOnExitMenuItemClicked,
                           base::Unretained(this)))
          .Build());
  auto menu_runner = std::make_unique<views::MenuRunner>(
      dialog_model.get(), views::MenuRunner::CONTEXT_MENU);
  menu_runner->RunMenuAt(GetWidget(), nullptr, GetBoundsInScreen(),
                         views::MenuAnchorPosition::kTopLeft,
                         ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void SiteDataRowView::OnDeleteMenuItemClicked(int event_flags) {
  // TODO(crbug.com/1344787): Handle the action.
}

void SiteDataRowView::OnBlockMenuItemClicked(int event_flags) {
  // TODO(crbug.com/1344787): Handle the action.
}

void SiteDataRowView::OnAllowMenuItemClicked(int event_flags) {
  // TODO(crbug.com/1344787): Handle the action.
}

void SiteDataRowView::OnClearOnExitMenuItemClicked(int event_flags) {
  // TODO(crbug.com/1344787): Handle the action.
}
