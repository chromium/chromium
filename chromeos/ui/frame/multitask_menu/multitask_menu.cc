// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/table_layout.h"

namespace chromeos {

namespace {
constexpr SkColor kMultitaskMenuBackgroundColor =
    SkColorSetARGB(255, 255, 255, 255);
constexpr int kMultitaskMenuBubbleCornerRadius = 8;
constexpr int KMultitaskMenuWidth = 270;
constexpr int kMultitaskMenuHeight = 248;
constexpr int kRowPadding = 16;
constexpr int kCenterPadding = 4;
constexpr int kLabelFontSize = 13;

// Create Multitask button with label.
std::unique_ptr<views::View> CreateButtonContainer(
    std::unique_ptr<views::View> button_view,
    int label_message_id) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(kCenterPadding);
  container->AddChildView(std::move(button_view));
  views::Label* label = container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id)));
  label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                   kLabelFontSize, gfx::Font::Weight::NORMAL));
  label->SetEnabledColor(gfx::kGoogleGrey900);
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  return container;
}

}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor, aura::Window* parent_window) {
  DCHECK(parent_window);
  set_color(kMultitaskMenuBackgroundColor);
  SetAnchorView(anchor);
  SetPaintToLayer();
  set_corner_radius(kMultitaskMenuBubbleCornerRadius);
  // TODO(shidi): Confirm with UX/UI for additional arrow choices when parent
  // window has no space for `MultitaskMenu` to arrow at `TOP_CENTER`.
  SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  SetPreferredSize(gfx::Size(KMultitaskMenuWidth, kMultitaskMenuHeight));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_parent_window(parent_window);
  set_close_on_deactivate(true);

  // TODO(shidi/sophiewen): Needs rework when reuse this class for ARC view or
  // tablet.
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddPaddingColumn(views::TableLayout::kFixedSize, kRowPadding)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kRowPadding)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding)
      .AddRows(1, views::TableLayout::kFixedSize, 0);

  // Half button.
  auto half_button = std::make_unique<SplitButtonView>(
      SplitButton::SplitButtonType::kHalfButtons,
      base::BindRepeating(&MultitaskMenu::SplitButtonPressed,
                          base::Unretained(this), SnapDirection::kPrimary),
      base::BindRepeating(&MultitaskMenu::SplitButtonPressed,
                          base::Unretained(this), SnapDirection::kSecondary));
  half_button_ = half_button.get();
  AddChildView(
      CreateButtonContainer(std::move(half_button), IDS_APP_ACCNAME_HALF));

  // Partial button.
  auto partial_button = std::make_unique<SplitButtonView>(
      SplitButton::SplitButtonType::kPartialButtons,
      base::BindRepeating(&MultitaskMenu::PartialButtonPressed,
                          base::Unretained(this), SnapDirection::kPrimary),
      base::BindRepeating(&MultitaskMenu::PartialButtonPressed,
                          base::Unretained(this), SnapDirection::kSecondary));
  partial_button_ = partial_button.get();
  AddChildView(CreateButtonContainer(std::move(partial_button),
                                     IDS_APP_ACCNAME_PARTIAL));

  // Full screen button.
  auto full_button = std::make_unique<MultitaskBaseButton>(
      base::BindRepeating(&MultitaskMenu::FullScreenButtonPressed,
                          base::Unretained(this)),
      MultitaskBaseButton::Type::kFull,
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_FULL));
  full_button_ = full_button.get();
  AddChildView(
      CreateButtonContainer(std::move(full_button), IDS_APP_ACCNAME_FULL));

  // Float on top button.
  auto float_button = std::make_unique<MultitaskBaseButton>(
      base::BindRepeating(&MultitaskMenu::FloatButtonPressed,
                          base::Unretained(this)),
      MultitaskBaseButton::Type::kFloat,
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_FLOAT_ON_TOP));
  float_button_ = float_button.get();
  AddChildView(CreateButtonContainer(std::move(float_button),
                                     IDS_APP_ACCNAME_FLOAT_ON_TOP));
}

MultitaskMenu::~MultitaskMenu() {
  if (bubble_widget_)
    HideBubble();
  bubble_widget_ = nullptr;
}

void MultitaskMenu::SplitButtonPressed(SnapDirection snap) {
  SnapController::Get()->CommitSnap(parent_window(), snap);
  HideBubble();
}

void MultitaskMenu::PartialButtonPressed(SnapDirection snap) {
  // TODO(shidi/sophiewen): Link Partial Split function here.
  HideBubble();
}

void MultitaskMenu::FullScreenButtonPressed() {
  auto* widget = views::Widget::GetWidgetForNativeWindow(parent_window());
  widget->SetFullscreen(!widget->IsFullscreen());
  HideBubble();
}

void MultitaskMenu::FloatButtonPressed() {
  FloatControllerBase::Get()->ToggleFloat(parent_window());
  HideBubble();
}

void MultitaskMenu::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_widget_, widget);
  bubble_widget_observer_.Reset();
  bubble_widget_ = nullptr;
}

void MultitaskMenu::ShowBubble() {
  DCHECK(parent_window());
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
  bubble_widget_->Show();
  bubble_widget_observer_.Observe(bubble_widget_.get());
  bubble_widget_->Activate();
}

void MultitaskMenu::HideBubble() {
  DCHECK(bubble_widget_);
  // This calls into OnWidgetDestroying() so `bubble_widget_` should have been
  // reset to nullptr.
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
}
}  // namespace chromeos
