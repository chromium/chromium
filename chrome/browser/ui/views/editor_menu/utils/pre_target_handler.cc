// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"

#include "base/containers/adapters.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/aura/env.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

PreTargetHandler::PreTargetHandler(views::View* view, const CardType& type)
    : view_(view), card_type_(type) {
  Init();
}

PreTargetHandler::~PreTargetHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);

  // Cancel any active context-menus, |view_| was a companion-view of which,
  // unless it had requested otherwise for some cases.
  if (dismiss_anchor_menu_on_view_closed_) {
    auto* active_menu_instance = views::MenuController::GetActiveInstance();
    if (active_menu_instance) {
      active_menu_instance->Cancel(views::MenuController::ExitType::kAll);
    }
  }
}

void PreTargetHandler::Init() {
  // QuickAnswersView is a companion view of a menu. Menu host widget sets
  // mouse capture as well as a pre-target handler, so we need to register one
  // here as well to intercept events for QuickAnswersView.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
  external_focus_tracker_ =
      std::make_unique<views::ExternalFocusTracker>(view_, nullptr);
}

void PreTargetHandler::OnEvent(ui::Event* event) {
  if (event->IsKeyEvent()) {
    ProcessKeyEvent(static_cast<ui::KeyEvent*>(event));
    return;
  }

  if (!event->IsLocatedEvent()) {
    return;
  }

  // Filter scroll event due to potential timing issue (b/258750397).
  if (event->IsScrollEvent() || event->type() == ui::ET_MOUSEWHEEL) {
    return;
  }

  // Clone event to forward down the view-hierarchy.
  auto clone = event->Clone();
  ui::Event::DispatcherApi(clone.get()).set_target(event->target());
  auto* to_dispatch = clone->AsLocatedEvent();
  auto location = to_dispatch->target()->GetScreenLocation(*to_dispatch);
  bool contains_location = view_->GetBoundsInScreen().Contains(location);

  // `ET_MOUSE_MOVED` events outside the top-view's bounds are also dispatched
  // to clear any set hover-state.
  bool dispatch_event = (contains_location ||
                         to_dispatch->type() == ui::EventType::ET_MOUSE_MOVED);
  if (dispatch_event) {
    // Convert to local coordinates and forward to the top-view.
    views::View::ConvertPointFromScreen(view_, &location);
    to_dispatch->set_location(location);
    ui::Event::DispatcherApi(to_dispatch).set_target(view_);

    // Convert touch-event to gesture before dispatching since views do not
    // process touch-events.
    std::unique_ptr<ui::GestureEvent> gesture_event;
    if (to_dispatch->type() == ui::ET_TOUCH_PRESSED) {
      gesture_event = std::make_unique<ui::GestureEvent>(
          to_dispatch->x(), to_dispatch->y(), ui::EF_NONE,
          base::TimeTicks::Now(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
      to_dispatch = gesture_event.get();
    }

    DoDispatchEvent(view_, to_dispatch);

    // Clicks inside Quick-Answers views can dismiss the menu since they are
    // outside menu-bounds and are thus not propagated to it to prevent so.
    // Active menu instance will instead be cancelled when |view_| is deleted.
    if (event->type() != ui::ET_MOUSE_MOVED) {
      event->StopPropagation();
    }
  }

  // Show tooltips.
  auto* tooltip_manager = view_->GetWidget()->GetTooltipManager();
  if (tooltip_manager) {
    tooltip_manager->UpdateTooltip();
  }
}

// TODO(siabhijeet): Investigate using SendEventsToSink() for dispatching.
bool PreTargetHandler::DoDispatchEvent(views::View* view,
                                       ui::LocatedEvent* event) {
  DCHECK(view && event);

  // Out-of-bounds `ET_MOUSE_MOVED` events are allowed to sift through to
  // clear any set hover-state.
  // TODO (siabhijeet): Two-phased dispatching via widget should fix this.
  if (!view->HitTestPoint(event->location()) &&
      event->type() != ui::ET_MOUSE_MOVED) {
    return false;
  }

  // Post-order dispatch the event on child views in reverse Z-order.
  auto children = view->GetChildrenInZOrder();
  for (auto* child : base::Reversed(children)) {
    // Dispatch a fresh event to preserve the |event| for the parent target.
    std::unique_ptr<ui::Event> to_dispatch;
    if (event->IsMouseEvent()) {
      to_dispatch =
          std::make_unique<ui::MouseEvent>(*event->AsMouseEvent(), view, child);
    } else if (event->IsGestureEvent()) {
      to_dispatch = std::make_unique<ui::GestureEvent>(*event->AsGestureEvent(),
                                                       view, child);
    } else {
      return false;
    }
    ui::Event::DispatcherApi(to_dispatch.get()).set_target(child);
    if (DoDispatchEvent(child, to_dispatch.get()->AsLocatedEvent())) {
      return true;
    }
  }

  view->OnEvent(event);
  return event->handled();
}

void PreTargetHandler::ProcessKeyEvent(ui::KeyEvent* key_event) {
  if (key_event->type() != ui::ET_KEY_PRESSED) {
    return;
  }

  auto* focus_manager = view_->GetFocusManager();
  auto* currently_focused_view = focus_manager->GetFocusedView();
  bool view_has_pane_focus = view_->Contains(currently_focused_view);

  // |view_| will insert itself between the cyclic keyboard traversal order of
  // the last and the first menu items of the active menu by commandeering the
  // selection from these terminal items.

  // VKEY_UP/VKEY_DOWN will move focus between Quick Answers (or Editor Menu)
  // and context menu UIs. One difference is that When focus is moved to Editor
  // Menu, it will get activated, and the context menu will be dismissed.
  // VKEY_LEFT/VKEY_RIGHT will move focus inside Quick Answers UI.
  auto key_code = key_event->key_code();
  switch (key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      auto* active_menu = views::MenuController::GetActiveInstance();
      // When Editor Menu is active, the context menu could be dismissed.
      if (!active_menu) {
        return;
      }

      if (view_has_pane_focus) {
        // Allow key-events to pass on as-usual to the context menu and restore
        // focus to wherever |view_| borrowed it from.
        external_focus_tracker_->FocusLastFocusedExternalView();
        external_focus_tracker_->SetFocusManager(nullptr);

        // Explicitly lose focus if restoring to last focused did not work.
        if (view_->Contains(focus_manager->GetFocusedView())) {
          focus_manager->SetFocusedView(nullptr);
        }

        return;
      }

      // Get the selected item, if any, in the currently active menu.
      auto* const selected_item = active_menu->GetSelectedMenuItem();
      if (!selected_item) {
        return;
      }

      auto* const parent = selected_item->GetParentMenuItem();
      bool view_should_gain_focus = false;
      if (parent) {
        // Check if the item is within the outer-most menu, since we do not want
        // the selection to loop back to |view_| for submenus.
        if (parent->GetParentMenuItem()) {
          return;
        }

        // |view_| should gain focus only when the selected item is first or
        // last within the menu.
        bool first_item_selected =
            selected_item == parent->GetSubmenu()->children().front();
        bool last_item_selected =
            selected_item == parent->GetSubmenu()->children().back();
        view_should_gain_focus =
            (first_item_selected || last_item_selected) &&
            first_item_selected == (key_code == ui::VKEY_UP);
      } else {
        // Selected menu-item will have no parent only when there are no nested
        // menus and no items are visibly selected, and |view_| should gain
        // focus for Up-key press in such scenario.
        view_should_gain_focus = key_code == ui::VKEY_UP;
      }

      // Focus |view_| if compatible key-event should transfer the selection to
      // it from within the menu.
      if (view_should_gain_focus) {
        // Track currently focused view to restore back to later and send focus
        // to |view_|.
        external_focus_tracker_->SetFocusManager(focus_manager);
        view_->RequestFocus();
        key_event->StopPropagation();

        // The context menu will be dismissed when the Editor Menu requests
        // focus. The `active_menu` will be nullptr.
        active_menu = views::MenuController::GetActiveInstance();
        if (card_type_ == CardType::kEditorMenu) {
          CHECK(!active_menu);
        }

        // Reopen the sub-menu owned by |parent| to clear the currently selected
        // boundary menu-item.
        if (parent && active_menu) {
          active_menu->SelectItemAndOpenSubmenu(parent);
        }
      }

      return;
    }
    case ui::VKEY_RETURN: {
      if (view_has_pane_focus) {
        auto* button_in_focus = views::Button::AsButton(currently_focused_view);
        if (button_in_focus) {
          button_in_focus->OnKeyPressed(*key_event);
        }
        key_event->StopPropagation();
      }
      return;
    }
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      if (view_has_pane_focus) {
        bool reverse = key_code == ui::VKEY_LEFT;
        focus_manager->AdvanceFocus(reverse);
        key_event->StopPropagation();
      }
      return;
    }
    default:
      return;
  }
}

}  // namespace chromeos::editor_menu
