// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
class LocatedEvent;
}  // namespace ui

namespace views {
class ExternalFocusTracker;
class View;
}  // namespace views

namespace chromeos::editor_menu {

class QuickAnswersView;
class UserConsentView;

// This class handles mouse events, and update background color or
// dismiss Quick Answers or Editor Menu views.
// TODO (siabhijeet): Migrate to using two-phased event dispatching.
class PreTargetHandler : public ui::EventHandler {
 public:
  explicit PreTargetHandler(views::View* view,
                            const CardType& type = CardType::kQuickAnswers);

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
  void ProcessKeyEvent(ui::KeyEvent* key_event);

  // Returns true if event was consumed by |view| or its children.
  bool DoDispatchEvent(views::View* view, ui::LocatedEvent* event);

  // Associated view handled by this class.
  const raw_ptr<views::View> view_ = nullptr;

  // Whether any active menus, |view_| is a companion Quick-Answers related view
  // of which, should be dismissed when it is deleted.
  bool dismiss_anchor_menu_on_view_closed_ = true;

  std::unique_ptr<views::ExternalFocusTracker> external_focus_tracker_;

  const CardType card_type_ = CardType::kQuickAnswers;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_PRE_TARGET_HANDLER_H_
