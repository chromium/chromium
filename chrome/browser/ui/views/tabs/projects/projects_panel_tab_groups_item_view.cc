// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

// Horizontal spacing between child views.
constexpr int kSpacingBetweenChildViews = 12;

// The size of the Tab groups icon.
constexpr int kTabGroupIconSize = 12;

// Insets for share Tab icon.
constexpr gfx::Insets kShareIconMargins = gfx::Insets::VH(4, 4);

// Height and width of share Tab icon.
constexpr int kShareIconSize = 16;

// The size of the Tab groups image icon.
constexpr gfx::Size kTabGroupIconImageSize =
    gfx::Size(kTabGroupIconSize, kTabGroupIconSize);

// The margins for the Tab groups icon.
constexpr auto kTabGroupsIconMargins = gfx::Insets::VH(6, 6);

// The margins for the Tab groups.
constexpr auto kTabGroupsItemMargins = gfx::Insets::VH(4, 4);

// Margins for Tab group title.
constexpr gfx::Insets kTitleMargins = gfx::Insets::VH(2, 0);

// Size of Tab group title.
constexpr gfx::Size kTitleSize = gfx::Size(148, 20);

}  // namespace

ProjectsPanelTabGroupsItemView::ProjectsPanelTabGroupsItemView(
    const tab_groups::SavedTabGroup& group)
    : tab_group_color_id_(group.color()),
      tab_group_vector_icon_(group.local_group_id().has_value()
                                 ? kTabGroupIcon
                                 : kTabGroupClosedIcon) {
  // TODO(crbug.com/478980331) Investigate migrating to FlexLayout
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->set_cross_axis_alignment(views::LayoutAlignment::kCenter);
  layout->set_between_child_spacing(kSpacingBetweenChildViews);
  layout->SetCollapseMarginsSpacing(true);
  SetBackground(views::CreateSolidBackground(ui::kColorFrameActive));

  tab_group_icon_ = AddChildView(std::make_unique<views::ImageView>());
  tab_group_icon_->SetImageSize(kTabGroupIconImageSize);
  tab_group_icon_->SetProperty(views::kMarginsKey, kTabGroupsIconMargins);

  title_ = AddChildView(std::make_unique<views::Label>(group.title()));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetTextStyle(views::style::STYLE_BODY_3);
  title_->SetPreferredSize(kTitleSize);
  title_->SetProperty(views::kMarginsKey, kTitleMargins);

  if (group.is_shared_tab_group()) {
    auto* shared_group_icon =
        AddChildView(std::make_unique<views::ImageView>());
    shared_group_icon->SetPreferredSize(
        gfx::Size(kShareIconSize, kShareIconSize));
    shared_group_icon->SetProperty(views::kMarginsKey, kShareIconMargins);

    ui::ImageModel shared_group_image_model = ui::ImageModel::FromVectorIcon(
        kPeopleGroupIcon, gfx::kGoogleGrey700, kShareIconSize);
    shared_group_icon->SetImage(shared_group_image_model);
  }

  SetProperty(views::kMarginsKey, kTabGroupsItemMargins);
  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelTabGroupsItemViewElementId);
  GetViewAccessibility().SetName(group.title());
}

ProjectsPanelTabGroupsItemView::~ProjectsPanelTabGroupsItemView() = default;

void ProjectsPanelTabGroupsItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  ui::ColorId color_id = GetTabGroupContextMenuColorId(tab_group_color_id_);
  tab_group_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *tab_group_vector_icon_, GetColorProvider()->GetColor(color_id),
      kTabGroupIconSize));
}

BEGIN_METADATA(ProjectsPanelTabGroupsItemView)
END_METADATA
