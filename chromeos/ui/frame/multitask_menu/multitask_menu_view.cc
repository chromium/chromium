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
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr int kCenterPadding = 4;
constexpr int kLabelFontSize = 13;

// Dogfood feedback button layout values.
constexpr int kButtonHeight = 28;
// The space between the text and image in the feedback button.
constexpr int kButtonImageSpacing = 4;
// Divisor to determine the radius of the rounded corners for the button.
constexpr float kButtonRadDivisor = 2.f;
constexpr gfx::Insets kButtonInsets = gfx::Insets::TLBR(0, 6, 0, 8);

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

// -----------------------------------------------------------------------------
// MultitaskMenuView::MenuPreTargetHandler:

class MultitaskMenuView::MenuPreTargetHandler : public ui::EventHandler {
 public:
  MenuPreTargetHandler(aura::Window* menu_window,
                       base::RepeatingClosure close_callback)
      : menu_window_(menu_window), close_callback_(std::move(close_callback)) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }

  ~MenuPreTargetHandler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    // TODO(b/266441890): Consider closing the menu on ET_MOUSE_MOVED.
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      ProcessPressedEvent(*event);
    }
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->type() == ui::ET_TOUCH_PRESSED) {
      ProcessPressedEvent(*event);
    }
  }

  void ProcessPressedEvent(const ui::LocatedEvent& event) {
    const gfx::Point screen_location = event.target()->GetScreenLocation(event);
    // If the event is out of menu bounds, close the menu.
    if (!menu_window_->GetBoundsInScreen().Contains(screen_location)) {
      close_callback_.Run();
    }
  }

 private:
  // The multitask menu that is currently shown. Guaranteed to outlive `this`,
  // which will get destroyed when the menu is destructed in `close_callback_`.
  aura::Window* const menu_window_;

  base::RepeatingClosure close_callback_;
};

// -----------------------------------------------------------------------------
// MultitaskMenuView:

MultitaskMenuView::MultitaskMenuView(aura::Window* window,
                                     base::RepeatingClosure close_callback,
                                     uint8_t buttons)
    : window_(window), close_callback_(std::move(close_callback)) {
  DCHECK(window);
  DCHECK(close_callback_);
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
  if (buttons & kPartialSplit) {
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

  // Dogfood feedback button. This button is added as a child view as it
  // prevents having to create separate instances in `MultitaskMenu` and
  // `TabletModeMultitaskMenuView`, and does not require a separate
  // `LayoutManager`.
  feedback_button_ = AddChildView(std::make_unique<views::LabelButton>(
      views::Button::PressedCallback(),
      l10n_util::GetStringUTF16(IDS_MULTITASK_MENU_FEEDBACK_BUTTON_NAME)));

  feedback_button_->SetImageLabelSpacing(kButtonImageSpacing);
  feedback_button_->SetBorder(views::CreateEmptyBorder(kButtonInsets));
  feedback_button_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  feedback_button_->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorMultitaskFeedbackButtonLabelBackground,
      kButtonHeight / kButtonRadDivisor));

  views::InkDropHost* const ink_drop = views::InkDrop::Get(feedback_button_);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->SetBaseColor(SK_ColorGRAY);
  views::InstallRoundRectHighlightPathGenerator(
      feedback_button_, gfx::Insets(), kButtonHeight / kButtonRadDivisor);
}

MultitaskMenuView::~MultitaskMenuView() {
  event_handler_.reset();
}

void MultitaskMenuView::AddedToWidget() {
  // When the menu widget is shown, we install `MenuPreTargetHandler` to close
  // the menu on any events outside.
  event_handler_ = std::make_unique<MultitaskMenuView::MenuPreTargetHandler>(
      GetWidget()->GetNativeWindow(), close_callback_);
}

void MultitaskMenuView::OnThemeChanged() {
  // Must be called at the beginning of the function.
  views::View::OnThemeChanged();

  auto* color_provider = GetColorProvider();
  feedback_button_->SetTextColor(
      views::Button::STATE_NORMAL,
      color_provider->GetColor(
          ui::kColorMultitaskFeedbackButtonLabelForeground));
  feedback_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          kDogfoodPawIcon,
          color_provider->GetColor(
              ui::kColorMultitaskFeedbackButtonLabelForeground)));
}

void MultitaskMenuView::SplitButtonPressed(SnapDirection direction) {
  SnapController::Get()->CommitSnap(window_, direction, kDefaultSnapRatio);
  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kHalfSplitButton);
}

void MultitaskMenuView::PartialButtonPressed(SnapDirection direction) {
  SnapController::Get()->CommitSnap(window_, direction,
                                    direction == SnapDirection::kPrimary
                                        ? kTwoThirdSnapRatio
                                        : kOneThirdSnapRatio);
  close_callback_.Run();

  base::RecordAction(base::UserMetricsAction(
      direction == SnapDirection::kPrimary ? kPartialSplitTwoThirdsUserAction
                                           : kPartialSplitOneThirdUserAction));
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kPartialSplitButton);
}

void MultitaskMenuView::FullScreenButtonPressed() {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  widget->SetFullscreen(!widget->IsFullscreen());
  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFullscreenButton);
}

void MultitaskMenuView::FloatButtonPressed() {
  FloatControllerBase::Get()->ToggleFloat(window_);
  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFloatButton);
}

BEGIN_METADATA(MultitaskMenuView, View)
END_METADATA

}  // namespace chromeos
