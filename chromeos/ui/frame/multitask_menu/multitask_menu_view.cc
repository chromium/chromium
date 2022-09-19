// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/check.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "ui/aura/window.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr int kCenterPadding = 4;
constexpr int kLabelFontSize = 13;

// Creates multitask button with label.
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

MultitaskMenuView::MultitaskMenuView(
    aura::Window* window,
    base::RepeatingClosure on_any_button_pressed)
    : window_(window),
      on_any_button_pressed_(std::move(on_any_button_pressed)) {
  DCHECK(window);
  DCHECK(on_any_button_pressed_);
  SetUseDefaultFillLayout(true);

  // Half button.
  auto half_button = std::make_unique<SplitButtonView>(
      SplitButton::SplitButtonType::kHalfButtons,
      base::BindRepeating(&MultitaskMenuView::SplitButtonPressed,
                          base::Unretained(this), SnapDirection::kPrimary),
      base::BindRepeating(&MultitaskMenuView::SplitButtonPressed,
                          base::Unretained(this), SnapDirection::kSecondary));
  half_button_ = half_button.get();
  AddChildView(
      CreateButtonContainer(std::move(half_button), IDS_APP_ACCNAME_HALF));

  auto partial_button = std::make_unique<SplitButtonView>(
      SplitButton::SplitButtonType::kPartialButtons,
      base::BindRepeating(&MultitaskMenuView::PartialButtonPressed,
                          base::Unretained(this), SnapDirection::kPrimary),
      base::BindRepeating(&MultitaskMenuView::PartialButtonPressed,
                          base::Unretained(this), SnapDirection::kSecondary));
  partial_button_ = partial_button.get();
  AddChildView(CreateButtonContainer(std::move(partial_button),
                                     IDS_APP_ACCNAME_PARTIAL));

  // Full screen button.
  auto full_button = std::make_unique<MultitaskBaseButton>(
      base::BindRepeating(&MultitaskMenuView::FullScreenButtonPressed,
                          base::Unretained(this)),
      MultitaskBaseButton::Type::kFull,
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_FULL));
  full_button_ = full_button.get();
  AddChildView(
      CreateButtonContainer(std::move(full_button), IDS_APP_ACCNAME_FULL));

  // Float on top button.
  auto float_button = std::make_unique<MultitaskBaseButton>(
      base::BindRepeating(&MultitaskMenuView::FloatButtonPressed,
                          base::Unretained(this)),
      MultitaskBaseButton::Type::kFloat,
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_FLOAT_ON_TOP));
  float_button_ = float_button.get();
  AddChildView(CreateButtonContainer(std::move(float_button),
                                     IDS_APP_ACCNAME_FLOAT_ON_TOP));
}

MultitaskMenuView::~MultitaskMenuView() = default;

void MultitaskMenuView::SplitButtonPressed(SnapDirection snap) {
  SnapController::Get()->CommitSnap(window_, snap);
  on_any_button_pressed_.Run();
}

void MultitaskMenuView::PartialButtonPressed(SnapDirection snap) {
  // TODO(crbug.com/1350197): Implement partial split for tablet mode. It
  // currently splits to the default half ratio.
  SnapController::Get()->CommitSnap(
      window_, snap,
      snap == SnapDirection::kPrimary
          ? chromeos::SnapRatio::kTwoThirdSnapRatio
          : chromeos::SnapRatio::kOneThirdSnapRatio);
  on_any_button_pressed_.Run();
}

void MultitaskMenuView::FullScreenButtonPressed() {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  widget->SetFullscreen(!widget->IsFullscreen());
  on_any_button_pressed_.Run();
}

void MultitaskMenuView::FloatButtonPressed() {
  FloatControllerBase::Get()->ToggleFloat(window_);
  on_any_button_pressed_.Run();
}

}  // namespace chromeos