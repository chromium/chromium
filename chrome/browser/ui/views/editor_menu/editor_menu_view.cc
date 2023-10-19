// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_chip_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_gradient_badge.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_textfield_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/badge_painter.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
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

namespace chromeos::editor_menu {

namespace {

constexpr char kWidgetName[] = "EditorMenuViewWidget";

constexpr gfx::Insets kTitleContainerInsets = gfx::Insets::TLBR(12, 16, 12, 14);

// Min width in rewrite mode to ensure there is space for the chips.
constexpr int kEditorMenuRewriteModeMinWidth = 288;

// Spacing to apply between and around chips.
constexpr int kChipsHorizontalPadding = 8;
constexpr int kChipsVerticalPadding = 12;
constexpr gfx::Insets kChipsContainerInsets = gfx::Insets::TLBR(0, 16, 16, 16);

constexpr gfx::Insets kTextfieldContainerInsets =
    gfx::Insets::TLBR(0, 16, 12, 16);

}  // namespace

EditorMenuView::EditorMenuView(EditorMenuMode editor_menu_mode,
                               const PresetTextQueries& preset_text_queries,
                               const gfx::Rect& anchor_view_bounds,
                               EditorMenuViewDelegate* delegate)
    : editor_menu_mode_(editor_menu_mode),
      pre_target_handler_(
          std::make_unique<PreTargetHandler>(this, CardType::kEditorMenu)),
      delegate_(delegate) {
  CHECK(delegate_);
  InitLayout(preset_text_queries);
}

EditorMenuView::~EditorMenuView() = default;

// static
views::UniqueWidgetPtr EditorMenuView::CreateWidget(
    EditorMenuMode editor_menu_mode,
    const PresetTextQueries& preset_text_queries,
    const gfx::Rect& anchor_view_bounds,
    EditorMenuViewDelegate* delegate) {
  views::Widget::InitParams params;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  EditorMenuView* editor_menu_view =
      widget->SetContentsView(std::make_unique<EditorMenuView>(
          editor_menu_mode, preset_text_queries, anchor_view_bounds, delegate));
  editor_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

void EditorMenuView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

void EditorMenuView::RequestFocus() {
  views::View::RequestFocus();
  settings_button_->RequestFocus();
}

void EditorMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(
      l10n_util::GetStringUTF16(editor_menu_mode_ == EditorMenuMode::kWrite
                                    ? IDS_EDITOR_MENU_WRITE_CARD_TITLE
                                    : IDS_EDITOR_MENU_REWRITE_CARD_TITLE));
}

bool EditorMenuView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  GetWidget()->Close();
  return true;
}

void EditorMenuView::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
}

void EditorMenuView::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  // When the widget is active, will use default focus behavior.
  if (active) {
    // Reset `pre_target_handler_` immediately causes problems if the events are
    // not all precessed. Reset it asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EditorMenuView::ResetPreTargetHandler,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  // Close widget When it is deactivated.
  GetWidget()->Close();
}

void EditorMenuView::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  CHECK(delegate_);
  delegate_->OnEditorMenuVisibilityChanged(visible);
}

void EditorMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  const int editor_menu_width = editor_menu_mode_ == EditorMenuMode::kWrite
                                    ? anchor_view_bounds.width()
                                    : std::max(anchor_view_bounds.width(),
                                               kEditorMenuRewriteModeMinWidth);
  UpdateChipsContainer(editor_menu_width);

  GetWidget()->SetBounds(GetEditorMenuBounds(
      anchor_view_bounds,
      gfx::Size(editor_menu_width, GetHeightForWidth(editor_menu_width))));
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

void EditorMenuView::AddTitleContainer() {
  title_container_ = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* layout =
      title_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* title = title_container_->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(editor_menu_mode_ == EditorMenuMode::kWrite
                                    ? IDS_EDITOR_MENU_WRITE_CARD_TITLE
                                    : IDS_EDITOR_MENU_REWRITE_CARD_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_HEADLINE_5));
  title->SetEnabledColorId(ui::kColorSysOnSurface);

  auto* badge = title_container_->AddChildView(
      std::make_unique<EditorMenuGradientBadge>());
  badge->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, views::BadgePainter::kBadgeHorizontalMargin));

  auto* spacer =
      title_container_->AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacer, 1);

  settings_button_ =
      title_container_->AddChildView(views::ImageButton::CreateIconButton(
          base::BindRepeating(&EditorMenuView::OnSettingsButtonPressed,
                              weak_factory_.GetWeakPtr()),
          vector_icons::kSettingsOutlineIcon,
          l10n_util::GetStringUTF16(IDS_EDITOR_MENU_SETTINGS_TOOLTIP)));

  title_container_->SetProperty(views::kMarginsKey, kTitleContainerInsets);
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
  textfield_ = AddChildView(
      std::make_unique<EditorMenuTextfieldView>(editor_menu_mode_, delegate_));
  textfield_->SetProperty(views::kMarginsKey, kTextfieldContainerInsets);
}

void EditorMenuView::UpdateChipsContainer(int editor_menu_width) {
  // Remove chips from their current rows and transfer ownership since we want
  // to add the chips to new rows.
  std::vector<std::unique_ptr<EditorMenuChipView>> chips;
  for (auto* row : chips_container_->children()) {
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

void EditorMenuView::ResetPreTargetHandler() {
  pre_target_handler_.reset();
}

BEGIN_METADATA(EditorMenuView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
