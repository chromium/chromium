// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"

#include <cstddef>

#include "base/containers/adapters.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/aura/env.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

// If true. Focus can be moved into the root view by using the tab key.
// Otherwise, users can move focus into the root view using up and down keys.
bool IsNavigatableUsingTabKey(CardType card_type) {
  return card_type == CardType::kEditorMenu ||
         card_type == CardType::kMahiDefaultMenu ||
         card_type == CardType::kMagicBoostOptInCard;
}

}  // namespace

PreTargetHandler::PreTargetHandler(Delegate& delegate, const CardType& type)
    : delegate_(delegate), card_type_(type) {
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
  context_menu_focus_tracker_ = std::make_unique<views::ExternalFocusTracker>(
      delegate_->GetRootView(), /*focus_manager=*/nullptr);
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
  if (event->IsScrollEvent() || event->type() == ui::EventType::kMousewheel) {
    return;
  }

  auto* root_view = delegate_->GetRootView();

  CHECK(root_view);

  // Clone event to forward down the view-hierarchy.
  auto clone = event->Clone();
  ui::Event::DispatcherApi(clone.get()).set_target(event->target());
  auto* to_dispatch = clone->AsLocatedEvent();
  auto location = to_dispatch->target()->GetScreenLocation(*to_dispatch);
  bool contains_location = root_view->GetBoundsInScreen().Contains(location);

  // Use a local `ViewTracker` in case `this` is deleted after
  // `DoDispatchEvent()` is run.
  views::ViewTracker view_tracker(root_view);

  // `EventType::kMouseMoved` events outside the top-view's bounds are also
  // dispatched to clear any set hover-state.
  bool dispatch_event =
      (contains_location || to_dispatch->type() == ui::EventType::kMouseMoved);
  if (dispatch_event) {
    // Convert to local coordinates and forward to the top-view.
    views::View::ConvertPointFromScreen(root_view, &location);
    to_dispatch->set_location(location);
    ui::Event::DispatcherApi(to_dispatch).set_target(root_view);

    // Convert touch-event to gesture before dispatching since views do not
    // process touch-events.
    std::unique_ptr<ui::GestureEvent> gesture_event;
    if (to_dispatch->type() == ui::EventType::kTouchPressed) {
      gesture_event = std::make_unique<ui::GestureEvent>(
          to_dispatch->x(), to_dispatch->y(), ui::EF_NONE,
          base::TimeTicks::Now(),
          ui::GestureEventDetails(ui::EventType::kGestureTap));
      to_dispatch = gesture_event.get();
    }

    DoDispatchEvent(root_view, to_dispatch);

    // Clicks inside Quick-Answers views can dismiss the menu since they are
    // outside menu-bounds and are thus not propagated to it to prevent so.
    // Active menu instance will instead be cancelled when |root_view| is
    // deleted.
    if (event->type() != ui::EventType::kMouseMoved) {
      event->StopPropagation();
    }
  }

  // After `DoDispatchEvent()` is run, the event dispatch can cause `root_view`
  // and `this` to be deleted.
  if (!view_tracker) {
    return;
  }

  // Show tooltips.
  auto* tooltip_manager = root_view->GetWidget()->GetTooltipManager();
  if (tooltip_manager) {
    tooltip_manager->UpdateTooltip();
  }
}

// TODO(siabhijeet): Investigate using SendEventsToSink() for dispatching.
bool PreTargetHandler::DoDispatchEvent(views::View* view,
                                       ui::LocatedEvent* event) {
  DCHECK(view && event);

  // Out-of-bounds `EventType::kMouseMoved` events are allowed to sift through
  // to clear any set hover-state.
  // TODO (siabhijeet): Two-phased dispatching via widget should fix this.
  if (!view->HitTestPoint(event->location()) &&
      event->type() != ui::EventType::kMouseMoved) {
    return false;
  }

  views::ViewTracker view_tracker(view);

  // Post-order dispatch the event on child views in reverse Z-order.
  auto children = view->GetChildrenInZOrder();
  for (views::View* child : base::Reversed(children)) {
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

    // `view` can be deleted after `child` handles the event.
    if (!view_tracker) {
      return false;
    }
  }

  view->OnEvent(event);
  return event->handled();
}

void PreTargetHandler::ProcessKeyEvent(ui::KeyEvent* key_event) {
  if (key_event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  auto* root_view = delegate_->GetRootView();

  CHECK(root_view);

  auto* focus_manager = root_view->GetFocusManager();
  auto* currently_focused_view = focus_manager->GetFocusedView();
  bool view_has_pane_focus = root_view->Contains(currently_focused_view);

  // |view_| will insert itself between the cyclic keyboard traversal order of
  // the last and the first menu items of the active menu by commandeering the
  // selection from these terminal items.

  // - VKEY_UP/VKEY_DOWN will move focus between the tracked view and the
  // context menu UIs if the tracking view is of type `kDefault`.
  // - VKEY_LEFT/VKEY_RIGHT will move focus inside the tracked view if the view
  // is in focus.
  // - VKEY_TAB will move the focus to the tracked view if it is an Editor
  // Menu or Mahi default menu. Note that in this case the card will get
  // activated, and the context menu will be dismissed.
  auto key_code = key_event->key_code();
  switch (key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      if (!IsNavigatableUsingTabKey(card_type_)) {
        ProcessKeyUpAndDown(key_event);
      }
      return;
    }
    case ui::VKEY_TAB: {
      if (IsNavigatableUsingTabKey(card_type_) && !view_has_pane_focus) {
        root_view->RequestFocus();
        key_event->StopPropagation();
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

void PreTargetHandler::ProcessKeyUpAndDown(ui::KeyEvent* key_event) {
  auto* active_menu = views::MenuController::GetActiveInstance();
  CHECK(active_menu);

  auto* root_view = delegate_->GetRootView();
  CHECK(root_view);

  std::vector<views::View*> views =
      delegate_->GetTraversableViewsByUpDownKeys();
  if (views.empty()) {
    return;
  }

  auto* focus_manager = root_view->GetFocusManager();
  auto* currently_focused_view = focus_manager->GetFocusedView();
  auto key_code = key_event->key_code();

  for (size_t index = 0; index < views.size(); ++index) {
    auto* view = views[index];
    CHECK(view);

    // Only handle the case that view has focus.
    if (view != currently_focused_view &&
        !view->Contains(currently_focused_view)) {
      continue;
    }

    // Move focus to context menu if needed if the view is the first or last
    // view on the list.
    if ((index == 0 && key_code == ui::VKEY_UP) ||
        (index == views.size() - 1 && key_code == ui::VKEY_DOWN)) {
      MoveFocusToContextMenu();
      return;
    }

    // Move focus to the previous/next view depending on the key code.
    MoveFocusTo(views[index + (key_code == ui::VKEY_UP ? -1 : 1)], key_event,
                focus_manager);
    return;
  }

  // Get the selected item, if any, in the currently active menu.
  auto* const selected_item = active_menu->GetSelectedMenuItem();
  if (!selected_item) {
    return;
  }

  auto* const parent = selected_item->GetParentMenuItem();

  if (!parent) {
    // The root menu item (which doesn't have a parent) can be marked as
    // selected when there are no nested menus and no items are visibly
    // selected. In this case, the first view in the list should gain focus for
    // Up-key press.
    if (key_code == ui::VKEY_UP) {
      MoveFocusTo(views[0], key_event, focus_manager);
    }
    return;
  }

  // Check if the item is within the outer-most menu, since we do not want
  // the selection to loop back to any views for submenus.
  if (parent->GetParentMenuItem()) {
    return;
  }

  // Handle key events that transition from context menu to the traversable
  // view.
  bool first_item_selected =
      selected_item == parent->GetSubmenu()->children().front();
  if (first_item_selected && key_code == ui::VKEY_UP) {
    MoveFocusTo(views.back(), key_event, focus_manager);
    return;
  }

  bool last_item_selected =
      selected_item == parent->GetSubmenu()->children().back();
  if (last_item_selected && key_code == ui::VKEY_DOWN) {
    MoveFocusTo(views.front(), key_event, focus_manager);
  }
}

void PreTargetHandler::MoveFocusToContextMenu() {
  // Moves the focus back to context menu using `context_menu_focus_tracker_`.
  // The logic of selecting a correct menu item is done by the context menu.
  context_menu_focus_tracker_->FocusLastFocusedExternalView();
  context_menu_focus_tracker_->SetFocusManager(nullptr);

  auto* root_view = delegate_->GetRootView();
  CHECK(root_view);

  auto* focus_manager = root_view->GetFocusManager();

  // Explicitly lose focus if restoring to last focused did not work.
  if (root_view->Contains(focus_manager->GetFocusedView())) {
    focus_manager->SetFocusedView(nullptr);
  }
}

void PreTargetHandler::MoveFocusTo(views::View* view_gain_focus,
                                   ui::KeyEvent* key_event,
                                   views::FocusManager* current_focus_manager) {
  if (!view_gain_focus) {
    return;
  }

  // Track currently focused view to restore back to later and send focus
  // to `view_gain_focus`.
  context_menu_focus_tracker_->SetFocusManager(current_focus_manager);
  view_gain_focus->RequestFocus();
  key_event->StopPropagation();

  auto* active_menu = views::MenuController::GetActiveInstance();
  CHECK(active_menu);
  auto* const selected_item = active_menu->GetSelectedMenuItem();
  CHECK(selected_item);
  auto* const parent = selected_item->GetParentMenuItem();

  // Reopen the sub-menu owned by `parent` to clear the currently selected
  // boundary menu-item.
  if (parent) {
    active_menu->SelectItemAndOpenSubmenu(parent);
  }
}

}  // namespace chromeos::editor_menu
