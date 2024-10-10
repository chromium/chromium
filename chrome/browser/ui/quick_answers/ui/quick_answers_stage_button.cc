// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_stage_button.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/painter.h"

namespace quick_answers {
namespace {
constexpr gfx::Insets kHighlightInsets = gfx::Insets::VH(4, 0);

class HighlightBackground : public views::Background {
 public:
  // `OnViewThemeChanged` is called when a background is added to a view.
  void OnViewThemeChanged(views::View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(
        ui::kColorMenuItemBackgroundHighlighted));

    // Use `SolidRoundRectPainter` to get a rectangle painter with insets.
    painter_ = views::Painter::CreateSolidRoundRectPainter(get_color(),
                                                           /*radius=*/0,
                                                           kHighlightInsets);

    view->SchedulePaint();
  }

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    CHECK(painter_);
    views::Painter::PaintPainterAt(canvas, painter_.get(),
                                   view->GetLocalBounds());
  }

 private:
  std::unique_ptr<views::Painter> painter_;
};

}  // namespace

QuickAnswersStageButton::QuickAnswersStageButton() {
  SetInstallFocusRingOnFocus(false);

  // This is because waiting for mouse-release to fire buttons would be too
  // late, since mouse-press dismisses the menu.
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  UpdateBackground();

  GetViewAccessibility().SetRole(ax::mojom::Role::kNone);
}

QuickAnswersStageButton::~QuickAnswersStageButton() = default;

void QuickAnswersStageButton::StateChanged(
    views::Button::ButtonState old_state) {
  views::Button::ButtonState state = GetState();
  if (state != views::Button::ButtonState::STATE_NORMAL &&
      state != views::Button::ButtonState::STATE_HOVERED) {
    return;
  }

  UpdateBackground();
}

void QuickAnswersStageButton::OnFocus() {
  UpdateBackground();
}

void QuickAnswersStageButton::OnBlur() {
  UpdateBackground();
}

void QuickAnswersStageButton::UpdateBackground() {
  bool highlight =
      GetState() == views::Button::ButtonState::STATE_HOVERED || HasFocus();
  SetBackground(highlight ? std::make_unique<HighlightBackground>() : nullptr);
}

BEGIN_METADATA(QuickAnswersStageButton)
END_METADATA

}  // namespace quick_answers
