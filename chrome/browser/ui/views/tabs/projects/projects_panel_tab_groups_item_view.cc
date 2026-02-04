// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

// The size of the more button.
constexpr auto kMoreButtonSize = gfx::Size(24, 24);

// The spacing between children in the Tab group item. Since FlexLayout does not
// provide an easy way to apply this, the spacing is added to the children's
// margins.
constexpr int kSpacingBetweenChildren = 10;

// Insets for share Tab icon.
constexpr gfx::Insets kShareIconMargins =
    gfx::Insets::TLBR(4, 4 + kSpacingBetweenChildren, 4, 4);

// The size of the Tab groups icon.
constexpr int kTabGroupIconSize = 12;

// The margins for the Tab groups icon.
constexpr auto kTabGroupsIconMargins = gfx::Insets(6);

// The margins for the Tab groups.
constexpr auto kTabGroupsItemMargins = gfx::Insets(4);

// The preferred size for the Tab groups item.
constexpr auto kTabGroupsItemPreferredSize = gfx::Size(0, 32);

// Margins for Tab group title.
constexpr gfx::Insets kTitleMargins =
    gfx::Insets::TLBR(2, 2 + kSpacingBetweenChildren, 2, 2);

// Height and width of shared Tab group icon and more button icon.
constexpr int kTrailingIconSize = 16;

}  // namespace

ProjectsPanelTabGroupsItemView::ProjectsPanelTabGroupsItemView(
    const tab_groups::SavedTabGroup& group,
    TabGroupPressedCallback pressed_callback,
    MoreButtonPressedCallback more_button_callback)
    : group_guid_(group.saved_guid()),
      more_button_callback_(std::move(more_button_callback)),
      tab_group_color_id_(group.color()),
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
    shared_icon_ = AddChildView(std::make_unique<views::ImageView>());
    shared_icon_->SetProperty(views::kMarginsKey, kShareIconMargins);
    ui::ImageModel shared_group_image_model = ui::ImageModel::FromVectorIcon(
        kPeopleGroupIcon, ui::kColorSysOnSurfaceSubtle, kTrailingIconSize);
    shared_icon_->SetImage(shared_group_image_model);
    shared_icon_->SetProperty(
        views::kElementIdentifierKey,
        kProjectsPanelTabGroupsItemViewSharedIconElementId);
  }

  // TODO(crbug.com/480260037): Fade between more button and shared icon when
  // hover state changes.
  more_button_ = AddChildView(std::make_unique<views::MenuButton>(
      base::BindRepeating(&ProjectsPanelTabGroupsItemView::OnMoreButtonPressed,
                          base::Unretained(this))));
  ui::ImageModel menu_icon_image_model = ui::ImageModel::FromVectorIcon(
      kBrowserToolsChromeRefreshIcon, ui::kColorSysOnSurfaceSubtle,
      kTrailingIconSize);
  more_button_->SetPreferredSize(kMoreButtonSize);
  more_button_->SetImageModel(ButtonState::STATE_NORMAL, menu_icon_image_model);
  more_button_->SetVisible(false);
  auto more_button_accessibility_label =
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_MORE_OPTIONS);
  more_button_->GetViewAccessibility().SetName(more_button_accessibility_label);
  more_button_->SetTooltipText(more_button_accessibility_label);
  more_button_->SetProperty(views::kElementIdentifierKey,
                            kProjectsPanelTabGroupsItemViewMoreButtonElementId);
  more_button_->SetProperty(views::kMarginsKey, kShareIconMargins);
  more_button_state_subscription_ =
      more_button_->AddStateChangedCallback(base::BindRepeating(
          &ProjectsPanelTabGroupsItemView::OnMoreButtonStateChanged,
          base::Unretained(this)));
  ConfigureInkDropForToolbar(more_button_);
  SetNotifyEnterExitOnChild(true);

  SetCallback(base::BindRepeating(
      [](TabGroupPressedCallback callback, const base::Uuid& group_guid) {
        std::move(callback).Run(group_guid);
      },
      std::move(pressed_callback), group.saved_guid()));

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

void ProjectsPanelTabGroupsItemView::OnMouseEntered(
    const ui::MouseEvent& event) {
  UpdateHoverState();
}

void ProjectsPanelTabGroupsItemView::OnMouseExited(
    const ui::MouseEvent& event) {
  UpdateHoverState();
}

void ProjectsPanelTabGroupsItemView::OnMouseMoved(const ui::MouseEvent& event) {
  // Mouse enter and exit events are flaky on Linux, so this ensures the hover
  // state is still applied.
  UpdateHoverState();
}

void ProjectsPanelTabGroupsItemView::OnMoreButtonPressed() {
  more_button_callback_.Run(group_guid_, *more_button_);
  UpdateHoverState();
}

void ProjectsPanelTabGroupsItemView::OnMoreButtonStateChanged() {
  UpdateHoverState();
}

void ProjectsPanelTabGroupsItemView::UpdateHoverState() {
  const bool show_more =
      IsMouseHovered() || (more_button_ && more_button_->GetState() ==
                                               views::Button::STATE_PRESSED);

  if (shared_icon_) {
    shared_icon_->SetVisible(!show_more);
  }
  more_button_->SetVisible(show_more);

  if (auto* ink_drop = views::InkDrop::Get(this)->GetInkDrop()) {
    ink_drop->SetHovered(show_more);
  }
}

BEGIN_METADATA(ProjectsPanelTabGroupsItemView)
END_METADATA
