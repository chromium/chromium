// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
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
    base::RepeatingClosure on_any_button_pressed,
    uint8_t buttons)
    : window_(window),
      on_any_button_pressed_(std::move(on_any_button_pressed)) {
  DCHECK(window);
  DCHECK(on_any_button_pressed_);
  SetUseDefaultFillLayout(true);

  // The display orientation. This determines whether menu is in
  // landscape/portrait mode.
  const bool is_portrait_mode = !chromeos::IsDisplayLayoutHorizontal(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));

  // Half button.
  if (buttons & kHalfSplit) {
    auto half_button = std::make_unique<SplitButtonView>(
        SplitButtonView::SplitButtonType::kHalfButtons,
        base::BindRepeating(&MultitaskMenuView::SplitButtonPressed,
                            base::Unretained(this)),
        window, is_portrait_mode);
    half_button_for_testing_ = half_button.get();
    AddChildView(CreateButtonContainer(std::move(half_button),
                                       IDS_MULTITASK_MENU_HALF_BUTTON_NAME));
  }

  // Partial button.
  if (buttons & kPartialSplit &&
      chromeos::wm::features::IsPartialSplitEnabled()) {
    auto partial_button = std::make_unique<SplitButtonView>(
        SplitButtonView::SplitButtonType::kPartialButtons,
        base::BindRepeating(&MultitaskMenuView::PartialButtonPressed,
                            base::Unretained(this)),
        window, is_portrait_mode);
    partial_button_ = partial_button.get();
    AddChildView(CreateButtonContainer(std::move(partial_button),
                                       IDS_MULTITASK_MENU_PARTIAL_BUTTON_NAME));
  }

  // Full screen button.
  if (buttons & kFullscreen) {
    const bool fullscreened = window->GetProperty(kWindowStateTypeKey) ==
                              WindowStateType::kFullscreen;
    int message_id = fullscreened
                         ? IDS_MULTITASK_MENU_EXIT_FULLSCREEN_BUTTON_NAME
                         : IDS_MULTITASK_MENU_FULLSCREEN_BUTTON_NAME;
    auto full_button = std::make_unique<MultitaskButton>(
        base::BindRepeating(&MultitaskMenuView::FullScreenButtonPressed,
                            base::Unretained(this)),
        MultitaskButton::Type::kFull, is_portrait_mode,
        /*paint_as_active=*/fullscreened,
        l10n_util::GetStringUTF16(message_id));
    full_button_for_testing_ = full_button.get();
    AddChildView(CreateButtonContainer(std::move(full_button), message_id));
  }

  // Float on top button.
  if (buttons & kFloat) {
    const bool floated =
        window->GetProperty(kWindowStateTypeKey) == WindowStateType::kFloated;
    int message_id = floated ? IDS_MULTITASK_MENU_EXIT_FLOAT_BUTTON_NAME
                             : IDS_MULTITASK_MENU_FLOAT_BUTTON_NAME;
    auto float_button = std::make_unique<MultitaskButton>(
        base::BindRepeating(&MultitaskMenuView::FloatButtonPressed,
                            base::Unretained(this)),
        MultitaskButton::Type::kFloat, is_portrait_mode,
        /*paint_as_active=*/floated, l10n_util::GetStringUTF16(message_id));
    float_button_for_testing_ = float_button.get();
    AddChildView(CreateButtonContainer(std::move(float_button), message_id));
  }
}

MultitaskMenuView::~MultitaskMenuView() = default;

void MultitaskMenuView::SplitButtonPressed(SnapDirection direction) {
  SnapController::Get()->CommitSnap(window_, direction, kDefaultSnapRatio);
  on_any_button_pressed_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kHalfSplitButton);
}

void MultitaskMenuView::PartialButtonPressed(SnapDirection direction) {
  SnapController::Get()->CommitSnap(window_, direction,
                                    direction == SnapDirection::kPrimary
                                        ? kTwoThirdSnapRatio
                                        : kOneThirdSnapRatio);
  on_any_button_pressed_.Run();

  base::RecordAction(base::UserMetricsAction(
      direction == SnapDirection::kPrimary ? kPartialSplitTwoThirdsUserAction
                                           : kPartialSplitOneThirdUserAction));
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kPartialSplitButton);
}

void MultitaskMenuView::FullScreenButtonPressed() {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  widget->SetFullscreen(!widget->IsFullscreen());
  on_any_button_pressed_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFullscreenButton);
}

void MultitaskMenuView::FloatButtonPressed() {
  FloatControllerBase::Get()->ToggleFloat(window_);
  on_any_button_pressed_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFloatButton);
}

BEGIN_METADATA(MultitaskMenuView, View)
END_METADATA

}  // namespace chromeos
