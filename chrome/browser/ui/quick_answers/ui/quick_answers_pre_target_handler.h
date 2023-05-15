// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_PRE_TARGET_HANDLER_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_PRE_TARGET_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/views/view.h"

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace views {
class ExternalFocusTracker;
}  // namespace views

namespace quick_answers {
class QuickAnswersView;
class UserConsentView;

// This class handles mouse events, and update background color or
// dismiss quick answers view.
// TODO (siabhijeet): Migrate to using two-phased event dispatching.
class QuickAnswersPreTargetHandler : public ui::EventHandler {
 public:
  explicit QuickAnswersPreTargetHandler(QuickAnswersView* view);
  explicit QuickAnswersPreTargetHandler(quick_answers::UserConsentView* view);

  // Disallow copy and assign.
  QuickAnswersPreTargetHandler(const QuickAnswersPreTargetHandler&) = delete;
  QuickAnswersPreTargetHandler& operator=(const QuickAnswersPreTargetHandler&) =
      delete;

  ~QuickAnswersPreTargetHandler() override;

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
  const raw_ptr<views::View> view_;

  // Whether any active menus, |view_| is a companion Quick-Answers related view
  // of which, should be dismissed when it is deleted.
  bool dismiss_anchor_menu_on_view_closed_ = true;

  std::unique_ptr<views::ExternalFocusTracker> external_focus_tracker_;
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_UI_QUICK_ANSWERS_PRE_TARGET_HANDLER_H_
