// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/undo_window.h"

#include <iostream>

#include "ash/public/cpp/style/color_provider.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ui/ash/input_method/border_factory.h"
#include "chrome/browser/ui/ash/input_method/colors.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/wm/core/window_animations.h"
namespace ui {
namespace ime {

namespace {
constexpr int kHeight = 28;
constexpr int kPadding = 0;
constexpr int kIconSize = 16;
// TODO(crbug/1099044): Update and use cros_colors.json5
constexpr cros_styles::ColorName kButtonHighlightColor =
    cros_styles::ColorName::kRippleColor;

void SetHighlighted(views::View& view, bool highlighted) {
  if (!!view.background() != highlighted) {
    view.SetBackground(highlighted
                           ? views::CreateRoundedRectBackground(
                                 ResolveSemanticColor(kButtonHighlightColor), 2)
                           : nullptr);
  }
}

}  // namespace

UndoWindow::UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate)
    : delegate_(delegate) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets(kPadding));
  SetArrow(views::BubbleBorder::Arrow::BOTTOM_LEFT);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  std::u16string undo_button_text =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_AUTOCORRECT_UNDO_TEXT);

  undo_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&UndoWindow::UndoButtonPressed,
                          base::Unretained(this)),
      undo_button_text));
  undo_button_->SetText(undo_button_text);
  undo_button_->SetImageLabelSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  undo_button_->SetBackground(nullptr);
  undo_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  undo_button_->SetMaxSize(
      gfx::Size(std::numeric_limits<int>::max(), kHeight - 2 * kPadding));

  learn_more_button_ =
      AddChildView(std::make_unique<views::ImageButton>(base::BindRepeating(
          &AssistiveDelegate::AssistiveWindowButtonClicked,
          base::Unretained(delegate_),
          AssistiveWindowButton{
              .id = ui::ime::ButtonId::kLearnMore,
              .window_type = ash::ime::AssistiveWindowType::kLearnMore})));
  learn_more_button_->SetImageHorizontalAlignment(
      views::ImageButton::ALIGN_CENTER);
  learn_more_button_->SetImageVerticalAlignment(
      views::ImageButton::ALIGN_MIDDLE);
  learn_more_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_button_->SetVisible(false);
}

void UndoWindow::OnThemeChanged() {
  undo_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kAutocorrectUndoIcon,
          ash::ColorProvider::Get()->GetContentLayerColor(
              ash::ColorProvider::ContentLayerType::kIconColorPrimary),
          kIconSize));
  undo_button_->SetEnabledTextColors(
      ash::ColorProvider::Get()->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kTextColorSecondary));

  const auto* const color_provider = GetColorProvider();
  learn_more_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateSolidSidedBorder(
          gfx::Insets::TLBR(4, 0, 4, 4),
          color_provider->GetColor(ui::kColorButtonBackground)),
      views::LayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_VECTOR_IMAGE_BUTTON)));

  // TODO(crbug.com/1099044): Update and use cros colors.
  learn_more_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsOutlineIcon,
                                     ui::kColorIconSecondary));

  BubbleDialogDelegateView::OnThemeChanged();
}

UndoWindow::~UndoWindow() = default;

views::Widget* UndoWindow::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      GetBorderForWindow(WindowBorderType::Undo));
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void UndoWindow::Hide() {
  GetWidget()->Close();
}

void UndoWindow::Show(const bool show_setting_link) {
  learn_more_button_->SetVisible(show_setting_link);
  GetWidget()->Show();
  SizeToContents();
}

void UndoWindow::SetBounds(const gfx::Rect& word_bounds) {
  SetAnchorRect(word_bounds);
}

void UndoWindow::SetButtonHighlighted(const AssistiveWindowButton& button,
                                      bool highlighted) {
  if (button.id == ButtonId::kUndo) {
    SetHighlighted(*undo_button_, highlighted);
  } else if (button.id == ButtonId::kLearnMore) {
    SetHighlighted(*learn_more_button_, highlighted);
  }
}

views::Button* UndoWindow::GetUndoButtonForTesting() {
  return undo_button_;
}

void UndoWindow::UndoButtonPressed() {
  const AssistiveWindowButton button = {
      .id = ButtonId::kUndo,
      .window_type = ash::ime::AssistiveWindowType::kUndoWindow};
  SetButtonHighlighted(button, true);
  delegate_->AssistiveWindowButtonClicked(button);
}

BEGIN_METADATA(UndoWindow)
END_METADATA

}  // namespace ime
}  // namespace ui
