// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

// The spacing between children in the Tab group item. Since FlexLayout does not
// provide an easy way to apply this, the spacing is added to the children's
// margins.
constexpr int kSpacingBetweenChildren = 10;

// The size of the Tab groups icon.
constexpr int kTabGroupIconSize = 12;

// Insets for share Tab icon.
constexpr gfx::Insets kShareIconMargins =
    gfx::Insets::TLBR(4, 4 + kSpacingBetweenChildren, 4, 4);

// Height and width of share Tab icon.
constexpr int kShareIconSize = 16;

// The margins for the Tab groups icon.
constexpr auto kTabGroupsIconMargins = gfx::Insets(6);

// The margins for the Tab groups.
constexpr auto kTabGroupsItemMargins = gfx::Insets(4);

// The preferred size for the Tab groups item.
constexpr auto kTabGroupsItemPreferredSize = gfx::Size(0, 32);

// Margins for Tab group title.
constexpr gfx::Insets kTitleMargins =
    gfx::Insets::TLBR(2, 2 + kSpacingBetweenChildren, 2, 2);

}  // namespace

ProjectsPanelTabGroupsItemView::ProjectsPanelTabGroupsItemView(
    const tab_groups::SavedTabGroup& group)
    : tab_group_color_id_(group.color()),
      tab_group_vector_icon_(group.local_group_id().has_value()
                                 ? kTabGroupIcon
                                 : kTabGroupClosedIcon) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(kTabGroupsItemMargins)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetLayerRegion(views::LayerRegion::kBelow);
  ink_drop->SetBaseColor(ui::kColorSysStateHoverOnSubtle);
  ink_drop->SetHighlightOpacity(1.0f);
  views::HighlightPathGenerator::Install(
      this, projects_panel::GetListItemHighlightPathGenerator());
  views::FocusRing::Install(this);
  views::FocusRing::Get(this)->SetPathGenerator(
      projects_panel::GetListItemHighlightPathGenerator());
  views::FocusRing::Get(this)->SetHaloInset(
      projects_panel::kListItemFocusRingHaloInset);

  tab_group_icon_ = AddChildView(std::make_unique<views::ImageView>());
  tab_group_icon_->SetProperty(views::kMarginsKey, kTabGroupsIconMargins);

  auto group_title = tab_groups::TabGroupMenuUtils::GetMenuTextForGroup(group);
  title_ = AddChildView(std::make_unique<views::Label>(group_title));
  title_->SetTextStyle(views::style::STYLE_BODY_3);
  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  title_->SetProperty(views::kMarginsKey, kTitleMargins);
  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  if (group.is_shared_tab_group()) {
    auto* shared_group_icon =
        AddChildView(std::make_unique<views::ImageView>());
    shared_group_icon->SetProperty(views::kMarginsKey, kShareIconMargins);
    ui::ImageModel shared_group_image_model = ui::ImageModel::FromVectorIcon(
        kPeopleGroupIcon, ui::kColorSysOnSurfaceSubtle, kShareIconSize);
    shared_group_icon->SetImage(shared_group_image_model);
  }

  // This item should expand its width to fit its container, but has a
  // preferred height.
  SetPreferredSize(kTabGroupsItemPreferredSize);

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelTabGroupsItemViewElementId);
  GetViewAccessibility().SetName(group_title);
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
