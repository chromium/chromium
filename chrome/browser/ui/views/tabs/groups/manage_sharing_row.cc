// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/manage_sharing_row.h"

#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tabs/groups/avatar_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// width in dips of the spacing between the icon and the label.
constexpr int kImageLabelSpacing = 12;

gfx::Insets GetControlInsets() {
  const int horizontal_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  return ui::TouchUiController::Get()->touch_ui()
             ? gfx::Insets::VH(5 * vertical_spacing / 4, horizontal_spacing)
             : gfx::Insets::VH(vertical_spacing, horizontal_spacing);
}

}  // namespace

ManageSharingRow::ManageSharingRow(
    Profile* profile,
    const syncer::CollaborationId& collaboration_id,
    PressedCallback callback)
    : Button(std::move(callback)),
      profile_(profile),
      collaboration_id_(collaboration_id) {
  SetNotifyEnterExitOnChild(true);

  // Similar implementation to HoverButton.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/false,
                                               /*highlight_on_focus=*/true);
  views::InkDrop::Get(this)->SetBaseColor(kColorHoverButtonBackgroundHovered);
  views::InkDrop::Get(this)->SetVisibleOpacity(1.0f);
  views::InkDrop::Get(this)->SetHighlightOpacity(1.0f);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  RebuildChildren();
}
ManageSharingRow::~ManageSharingRow() = default;

void ManageSharingRow::RebuildChildren() {
  manage_group_icon_ = nullptr;
  manage_group_label_ = nullptr;
  avatar_container_ = nullptr;
  ink_drop_container_ = nullptr;
  RemoveAllChildViews();

  size_t member_size =
      tab_groups::SavedTabGroupUtils::GetMembersOfSharedTabGroup(
          profile_, collaboration_id_)
          .size();
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_MANAGE_GROUP) +
      u" " +
      l10n_util::GetPluralStringFUTF16(
          IDS_TAB_GROUP_HEADER_CXMENU_TAB_GROUP_FACE_PILE_ACCESSIBLE_NAME,
          member_size));

  manage_group_icon_ = AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(kTabGroupSharingIcon)));
  manage_group_icon_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kImageLabelSpacing));
  manage_group_icon_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  manage_group_icon_->SetPaintToLayer();
  manage_group_icon_->layer()->SetFillsBoundsOpaquely(false);

  manage_group_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_MANAGE_GROUP)));
  manage_group_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_group_label_->SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS);
  manage_group_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));
  manage_group_label_->SetSkipSubpixelRenderingOpacityCheck(true);
  manage_group_label_->SetPaintToLayer();
  manage_group_label_->layer()->SetFillsBoundsOpaquely(false);
  manage_group_label_->SetBackgroundColor(SK_ColorTRANSPARENT);

  avatar_container_ =
      AddChildView(std::make_unique<ManageSharingAvatarContainer>(
          profile_, collaboration_id_));
  avatar_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  avatar_container_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, kImageLabelSpacing, 0, 0));
  avatar_container_->SetPaintToLayer();
  avatar_container_->layer()->SetFillsBoundsOpaquely(false);

  ink_drop_container_ =
      AddChildView(views::Builder<views::InkDropContainerView>()
                       .SetAutoMatchParentBounds(true)
                       .Build());
  ink_drop_container_->SetVisible(false);
  ink_drop_container_->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

void ManageSharingRow::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  UpdateBackgroundColor();
}

void ManageSharingRow::UpdateInsetsToMatchLabelButton(
    views::LabelButton* button) {
  gfx::Insets save_group_margins = GetControlInsets();
  const int label_height = button->GetPreferredSize().height();
  const int control_height = std::max(
      manage_group_label_
          ->GetPreferredSize(
              views::SizeBounds(manage_group_label_->width(), {}))
          .height(),
      avatar_container_ ? avatar_container_->GetPreferredSize().height() : 0);
  save_group_margins.set_top((label_height - control_height) / 2);
  save_group_margins.set_bottom(save_group_margins.top());

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(save_group_margins);
}

void ManageSharingRow::AddedToWidget() {
  views::View::AddedToWidget();
  if (avatar_container_) {
    avatar_container_->AddedToWidget();
  }

  RebuildChildren();
}

void ManageSharingRow::OnThemeChanged() {
  Button::OnThemeChanged();
  UpdateBackgroundColor();
  RebuildChildren();
}

void ManageSharingRow::OnFocus() {
  Button::OnFocus();
  SchedulePaint();
}

void ManageSharingRow::OnBlur() {
  Button::OnBlur();
  SchedulePaint();
}

void ManageSharingRow::UpdateBackgroundColor() {
  SkColor bg_color;

  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }

  switch (GetState()) {
    case STATE_HOVERED:
      bg_color = color_provider->GetColor(ui::kColorSysStateHoverOnSubtle);
      break;
    case STATE_PRESSED:
      bg_color =
          color_provider->GetColor(ui::kColorSysStateRippleNeutralOnSubtle);
      break;
    case STATE_DISABLED:  // fallthrough
    case STATE_NORMAL:    // fallthrough
    default:
      bg_color = color_provider->GetColor(ui::kColorPrimaryBackground);
      break;
  }

  SetBackground(views::CreateSolidBackground(bg_color));
}

void ManageSharingRow::AddLayerToRegion(ui::Layer* new_layer,
                                        views::LayerRegion region) {
  ink_drop_container_->SetVisible(true);
  ink_drop_container_->AddLayerToRegion(new_layer, region);
}

void ManageSharingRow::RemoveLayerFromRegions(ui::Layer* old_layer) {
  ink_drop_container_->RemoveLayerFromRegions(old_layer);
  ink_drop_container_->SetVisible(false);
}

BEGIN_METADATA(ManageSharingRow)
END_METADATA
