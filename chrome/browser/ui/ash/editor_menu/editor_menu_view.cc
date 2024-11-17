// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_view.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_badge_view.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_chip_view.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_textfield_view.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/ash/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/ash/editor_menu/utils/pre_target_handler_view.h"
#include "chrome/browser/ui/ash/editor_menu/utils/utils.h"
#include "chromeos/components/editor_menu/public/cpp/icon.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace chromeos::editor_menu {

namespace {

constexpr char kWidgetName[] = "EditorMenuViewWidget";
constexpr char16_t kCardShownAnnouncement[] =
    u"Help Me Write, press tab to focus the Help Me Write card.";

constexpr gfx::Insets kTabsTitleContainerInsets =
    gfx::Insets::TLBR(8, 10, 0, 8);
constexpr gfx::Insets kNoTabsTitleContainerInsets =
    gfx::Insets::TLBR(10, 16, 0, 8);

constexpr int kBadgeHorizontalPadding = 8;

constexpr gfx::Insets kSettingsButtonPadding = gfx::Insets(4);

// Spacing to apply between and around chips.
constexpr int kChipsHorizontalPadding = 8;
constexpr int kChipsVerticalPadding = 12;

constexpr gfx::Insets kChipsContainerInsets = gfx::Insets::TLBR(8, 16, 0, 16);

constexpr gfx::Insets kTextfieldContainerInsets = gfx::Insets(16);

constexpr gfx::Size kTabbedPaneSize = gfx::Size(180, 36);

const views::FlexSpecification kTabbedPaneFlexSpecification =
    views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                             views::MaximumFlexSizeRule::kPreferred)
        .WithWeight(1);

const std::u16string GetEditorMenuAccessibilityName(
    TextAndImageMode text_and_image_mode) {
  if (text_and_image_mode == TextAndImageMode::kEditorRewriteAndLobster ||
      text_and_image_mode == TextAndImageMode::kEditorRewriteOnly) {
    return GetEditorMenuRewriteCardTitle();
  }
  if (text_and_image_mode == TextAndImageMode::kEditorWriteAndLobster ||
      text_and_image_mode == TextAndImageMode::kEditorWriteOnly) {
    return GetEditorMenuWriteCardTitle();
  }
  if (text_and_image_mode == TextAndImageMode::kLobsterOnly) {
    return GetEditorMenuLobsterTitle();
  }
  return u"";
}

}  // namespace

int GetChipsContainerHeightWithPaddings(int chip_height, int num_rows) {
  const int total_chips_height = num_rows * chip_height;
  const int total_chips_paddings =
      num_rows >= 1 ? (num_rows - 1) * kChipsVerticalPadding +
                          kChipsContainerInsets.height()
                    : 0;
  return total_chips_height + total_chips_paddings;
}

EditorMenuView::EditorMenuView(TextAndImageMode text_and_image_mode,
                               const PresetTextQueries& preset_text_queries,
                               const gfx::Rect& anchor_view_bounds,
                               EditorMenuViewDelegate* delegate)
    : PreTargetHandlerView(CardType::kEditorMenu),
      text_and_image_mode_(text_and_image_mode),
      delegate_(delegate) {
  CHECK(delegate_);
  InitLayout(preset_text_queries);

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(
      GetEditorMenuAccessibilityName(text_and_image_mode_));
}

EditorMenuView::~EditorMenuView() = default;

// static
std::unique_ptr<views::Widget> EditorMenuView::CreateWidget(
    TextAndImageMode text_and_image_mode,
    const PresetTextQueries& preset_text_queries,
    const gfx::Rect& anchor_view_bounds,
    EditorMenuViewDelegate* delegate) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kMenuRadius);
  params.shadow_elevation = wm::kShadowElevationMenuOrTooltip;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  EditorMenuView* editor_menu_view = widget->SetContentsView(
      std::make_unique<EditorMenuView>(text_and_image_mode, preset_text_queries,
                                       anchor_view_bounds, delegate));
  editor_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuView::AddedToWidget() {
  PreTargetHandlerView::AddedToWidget();
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

void EditorMenuView::RequestFocus() {
  views::View::RequestFocus();
  settings_button_->RequestFocus();
}

gfx::Size EditorMenuView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!available_size.width().is_bounded()) {
    return PreTargetHandlerView::CalculatePreferredSize(available_size);
  }

  int width = available_size.width().value();
  // When the width of editor menu view is updated, we will adjust the number of
  // rows chips (see: UpdateChipsContainer). Thus, here we need to pre-compute
  // the expected number of rows here and so we can estimate the height rather
  // than relying on the default logic.

  const int chip_container_width = width - kChipsContainerInsets.width();
  int running_width = 0;
  int num_rows = 0;
  int chip_height = 0;

  for (views::View* row : chips_container_->children()) {
    for (views::View* chip : row->children()) {
      const int chip_width = chip->GetPreferredSize().width();
      if (num_rows > 0 &&
          running_width + kChipsHorizontalPadding + chip_width <=
              chip_container_width) {
        running_width += kChipsHorizontalPadding + chip_width;
      } else {
        ++num_rows;
        running_width = chip_width;
      }

      chip_height = chip->height();
    }
  }

  const int title_height_with_padding =
      title_container_->height() + GetTitleContainerInsets().height();
  const int chips_height_with_padding =
      GetChipsContainerHeightWithPaddings(chip_height, num_rows);
  const int textfield_height_with_padding =
      textfield_->height() + kTextfieldContainerInsets.height();

  return gfx::Size(width, title_height_with_padding +
                              chips_height_with_padding +
                              textfield_height_with_padding);
}

bool EditorMenuView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  GetWidget()->Close();
  return true;
}

void EditorMenuView::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  CHECK(delegate_);
  delegate_->OnEditorMenuVisibilityChanged(visible);

  if (visible && !queued_announcement_) {
    GetViewAccessibility().AnnounceAlert(kCardShownAnnouncement);
    queued_announcement_ = true;
  }
}

void EditorMenuView::TabSelectedAt(int index) {
  CHECK(delegate_);
  delegate_->OnTabSelected(index);
}

void EditorMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  gfx::Rect editor_menu_bounds = GetEditorMenuBounds(anchor_view_bounds, this);
  GetWidget()->SetBounds(editor_menu_bounds);
  UpdateChipsContainer(/*editor_menu_width=*/editor_menu_bounds.width());
}

void EditorMenuView::DisableMenu() {
  settings_button_->SetEnabled(false);

  for (views::View* row : chips_container_->children()) {
    for (views::View* chip : row->children()) {
      chip->SetEnabled(false);
    }
  }

  textfield_->textfield()->SetEnabled(false);
  textfield_->arrow_button()->SetEnabled(false);
}

void EditorMenuView::InitLayout(const PresetTextQueries& preset_text_queries) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorPrimaryBackground,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius)));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);

  AddTitleContainer();
  AddChipsContainer(preset_text_queries);
  AddTextfield();
}

gfx::Insets EditorMenuView::GetTitleContainerInsets() const {
  switch (text_and_image_mode_) {
    case TextAndImageMode::kEditorWriteAndLobster:
      return kTabsTitleContainerInsets;
    case TextAndImageMode::kEditorWriteOnly:
    case TextAndImageMode::kLobsterOnly:
    case TextAndImageMode::kEditorRewriteOnly:
    case TextAndImageMode::kEditorRewriteAndLobster:
      return kNoTabsTitleContainerInsets;
    case TextAndImageMode::kBlocked:
    default:
      return gfx::Insets();
  }
}

void EditorMenuView::AddTitleContainer() {
  title_container_ = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* layout =
      title_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  if (text_and_image_mode_ == TextAndImageMode::kEditorWriteAndLobster) {
    tabbed_pane_ =
        title_container_->AddChildView(std::make_unique<views::TabbedPane>());
    tabbed_pane_->SetPreferredSize(kTabbedPaneSize);
    tabbed_pane_->SetProperty(views::kFlexBehaviorKey,
                              kTabbedPaneFlexSpecification);
    tabbed_pane_->AddTab(GetEditorMenuWriteCardTitle(),
                         std::make_unique<views::View>());
    tabbed_pane_->AddTab(GetEditorMenuLobsterTitle(),
                         std::make_unique<views::View>());
    tabbed_pane_->set_listener(this);
  } else if (text_and_image_mode_ == TextAndImageMode::kEditorWriteOnly) {
    auto* title = title_container_->AddChildView(std::make_unique<views::Label>(
        GetEditorMenuWriteCardTitle(), views::style::CONTEXT_DIALOG_TITLE,
        views::style::STYLE_HEADLINE_5));
    title->SetEnabledColorId(ui::kColorSysOnSurface);
  } else if (text_and_image_mode_ == TextAndImageMode::kEditorRewriteOnly ||
             text_and_image_mode_ ==
                 TextAndImageMode::kEditorRewriteAndLobster) {
    auto* title = title_container_->AddChildView(std::make_unique<views::Label>(
        GetEditorMenuRewriteCardTitle(), views::style::CONTEXT_DIALOG_TITLE,
        views::style::STYLE_HEADLINE_5));
    title->SetEnabledColorId(ui::kColorSysOnSurface);
  } else if (text_and_image_mode_ == TextAndImageMode::kLobsterOnly) {
    auto* title = title_container_->AddChildView(std::make_unique<views::Label>(
        GetEditorMenuLobsterTitle(), views::style::CONTEXT_DIALOG_TITLE,
        views::style::STYLE_HEADLINE_5));
    title->SetEnabledColorId(ui::kColorSysOnSurface);
  }

  auto* badge =
      title_container_->AddChildView(std::make_unique<EditorMenuBadgeView>());
  badge->SetProperty(views::kMarginsKey,
                     gfx::Insets::VH(0, kBadgeHorizontalPadding));

  auto* spacer =
      title_container_->AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacer, 1);

  settings_button_ =
      title_container_->AddChildView(views::ImageButton::CreateIconButton(
          base::BindRepeating(&EditorMenuView::OnSettingsButtonPressed,
                              weak_factory_.GetWeakPtr()),
          vector_icons::kSettingsOutlineIcon, GetEditorMenuSettingsTooltip()));
  settings_button_->SetProperty(views::kMarginsKey, kSettingsButtonPadding);

  title_container_->SetProperty(views::kMarginsKey, GetTitleContainerInsets());
}

void EditorMenuView::AddChipsContainer(
    const PresetTextQueries& preset_text_queries) {
  chips_container_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  chips_container_->SetOrientation(views::LayoutOrientation::kVertical);
  chips_container_->SetCollapseMargins(true);
  chips_container_->SetIgnoreDefaultMainAxisMargins(true);
  chips_container_->SetProperty(views::kMarginsKey, kChipsContainerInsets);
  chips_container_->SetDefault(views::kMarginsKey,
                               gfx::Insets::VH(kChipsVerticalPadding, 0));

  // Put all the chips in a single row while we are initially creating the
  // editor menu. This layout will be adjusted once the editor menu bounds are
  // set.
  auto* row = AddChipsRow();
  for (const auto& preset_text_query : preset_text_queries) {
    row->AddChildView(std::make_unique<EditorMenuChipView>(
        base::BindRepeating(&EditorMenuView::OnChipButtonPressed,
                            weak_factory_.GetWeakPtr(),
                            preset_text_query.text_query_id),
        preset_text_query));
  }
}

void EditorMenuView::AddTextfield() {
  textfield_ =
      AddChildView(std::make_unique<EditorMenuTextfieldView>(delegate_));
  textfield_->SetProperty(views::kMarginsKey, kTextfieldContainerInsets);
}

void EditorMenuView::UpdateChipsContainer(int editor_menu_width) {
  // Remove chips from their current rows and transfer ownership since we want
  // to add the chips to new rows.
  std::vector<std::unique_ptr<EditorMenuChipView>> chips;
  for (views::View* row : chips_container_->children()) {
    while (!row->children().empty()) {
      chips.push_back(row->RemoveChildViewT(
          views::AsViewClass<EditorMenuChipView>(row->children()[0])));
    }
  }

  // Remove existing rows from the chips container and delete them.
  chips_container_->RemoveAllChildViews();

  // Re-add the chips into new rows in the chips container, adjusting the layout
  // according to the updated editor menu width. We keep track of the running
  // width of the current chip row and start a new row of chips whenever the
  // chip being added can't fit into the current row.
  const int chip_container_width =
      editor_menu_width - kChipsContainerInsets.width();
  int running_width = 0;
  views::View* row = nullptr;
  for (auto& chip : chips) {
    const int chip_width = chip->GetPreferredSize().width();
    if (row != nullptr &&
        running_width + chip_width + kChipsHorizontalPadding <=
            chip_container_width) {
      // Add the chip to the current row if it can fit (including space for
      // padding between chips).
      running_width += chip_width + kChipsHorizontalPadding;
    } else {
      // Otherwise, create a new row for the chip.
      row = AddChipsRow();
      running_width = chip_width;
    }
    row->AddChildView(std::move(chip));
  }
}

views::View* EditorMenuView::AddChipsRow() {
  auto* row =
      chips_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  row->SetCollapseMargins(true);
  row->SetIgnoreDefaultMainAxisMargins(true);
  row->SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(0, kChipsHorizontalPadding));
  return row;
}

void EditorMenuView::OnSettingsButtonPressed() {
  CHECK(delegate_);
  delegate_->OnSettingsButtonPressed();
}

void EditorMenuView::OnChipButtonPressed(const std::string& text_query_id) {
  CHECK(delegate_);
  delegate_->OnChipButtonPressed(text_query_id);
}

BEGIN_METADATA(EditorMenuView)
END_METADATA

}  // namespace chromeos::editor_menu
