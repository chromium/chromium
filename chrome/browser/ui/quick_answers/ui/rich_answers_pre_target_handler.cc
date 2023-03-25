// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_pre_target_handler.h"

#include "base/containers/adapters.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

namespace quick_answers {

RichAnswersPreTargetHandler::RichAnswersPreTargetHandler(RichAnswersView* view)
    : view_(view) {
  // Register a pre-target handler to intercept events for RichAnswersView.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

RichAnswersPreTargetHandler::~RichAnswersPreTargetHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

void RichAnswersPreTargetHandler::OnKeyEvent(ui::KeyEvent* key_event) {
  if (key_event->type() != ui::ET_KEY_PRESSED) {
    return;
  }

  auto key_code = key_event->key_code();
  switch (key_code) {
    case ui::VKEY_ESCAPE: {
      QuickAnswersController::Get()->DismissQuickAnswers(
          quick_answers::QuickAnswersExitPoint::kUnspecified);
      return;
    }
    case ui::VKEY_SPACE:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      // TODO(b/275106457): Handle key navigation of focus on the
      // rich answers card view.
      key_event->StopPropagation();
      return;
    }
    default:
      return;
  }
}

void RichAnswersPreTargetHandler::OnMouseEvent(ui::MouseEvent* mouse_event) {
  gfx::Point cursor_point =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  gfx::Rect bounds = view_->GetWidget()->GetWindowBoundsInScreen();

  if (mouse_event->type() == ui::ET_MOUSE_PRESSED &&
      !bounds.Contains(cursor_point)) {
    // Dismiss the rich answers view when the user clicks outside the bounds.
    QuickAnswersController::Get()->DismissQuickAnswers(
        quick_answers::QuickAnswersExitPoint::kUnspecified);
  }

  // While the rich answers view is visible, do not pass on unhandled
  // mouse events up the hierarchy. The rich answers view should be dismissed
  // before allowing mouse event handling by other windows and views.
  if (mouse_event->cancelable()) {
    mouse_event->StopPropagation();
  }
}

void RichAnswersPreTargetHandler::OnScrollEvent(ui::ScrollEvent* scroll_event) {
  // TODO(b/265255821): handle scrolling of the rich answers view card.
  // Limit scroll events to the rich answers card while it is visible.
  // This means other windows and views will not be scrollable until the rich
  // answers view is dismissed.
  if (scroll_event->cancelable()) {
    scroll_event->StopPropagation();
  }
}

}  // namespace quick_answers
