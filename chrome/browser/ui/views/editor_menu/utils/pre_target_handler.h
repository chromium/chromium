// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/events/event_handler.h"
#include "ui/views/view_tracker.h"

namespace ui {
class KeyEvent;
class LocatedEvent;
}  // namespace ui

namespace views {
class ExternalFocusTracker;
class FocusManager;
class View;
}  // namespace views

namespace chromeos::editor_menu {

class QuickAnswersView;
class UserConsentView;

// This class handles mouse events, and update background color or
// dismiss for Quick Answers, Editor Menu, or Mahi Menu views.
// TODO (siabhijeet): Migrate to using two-phased event dispatching.
// TODO(b/329770382): Move this class to read_write_cards directory since it is
// used outside of editor_menu.
class PreTargetHandler : public ui::EventHandler {
 public:
  class Delegate {
   public:
    // `PreTargetHandler` will track the events happen on this root view,
    // dispatching the events to its children views, and focus the root view
    // when needed.
    virtual views::View* GetRootView() = 0;

    // Returns a list of views to go through with up/down keys. Note that the
    // order of the view within this list will determine the order of traversal.
    // - Hitting the down key will take the focus to the next view in the list
    //   (or back to the context menu if the last item in the list is currently
    //   in focus).
    // - Hitting the up key will take the focus to the previous view in the list
    //   (or back to the context menu if the first item in the list is currently
    //   in focus).
    virtual std::vector<views::View*> GetTraversableViewsByUpDownKeys() = 0;
  };

  explicit PreTargetHandler(Delegate& delegate,
                            const CardType& type = CardType::kDefault);

  // Disallow copy and assign.
  PreTargetHandler(const PreTargetHandler&) = delete;
  PreTargetHandler& operator=(const PreTargetHandler&) = delete;

  ~PreTargetHandler() override;

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

  void set_dismiss_anchor_menu_on_view_closed(bool dismiss) {
    dismiss_anchor_menu_on_view_closed_ = dismiss;
  }

 private:
  void Init();

  // Returns true if event was consumed by |view| or its children.
  bool DoDispatchEvent(views::View* view, ui::LocatedEvent* event);

  void ProcessKeyEvent(ui::KeyEvent* key_event);

  // Helper function for processing VKEY_UP/VKEY_DOWN.
  void ProcessKeyUpAndDown(ui::KeyEvent* key_event);

  // Moves focus to the context menu using `context_menu_focus_tracker_`.
  void MoveFocusToContextMenu();

  // Moves focus to the particular `view_gain_focus` as a result from a
  // `key_event`.
  void MoveFocusTo(views::View* view_gain_focus,
                   ui::KeyEvent* key_event,
                   views::FocusManager* current_focus_manager);

  const raw_ref<Delegate> delegate_;

  // Whether any active menus, |view_| is a companion Quick-Answers related view
  // of which, should be dismissed when it is deleted.
  bool dismiss_anchor_menu_on_view_closed_ = true;

  std::unique_ptr<views::ExternalFocusTracker> context_menu_focus_tracker_;

  const CardType card_type_ = CardType::kDefault;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_H_
