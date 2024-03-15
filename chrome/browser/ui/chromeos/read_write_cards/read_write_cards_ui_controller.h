// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_UI_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace chromeos {

// The controller that manages all the behaviors of the UI widget containing
// some of the read write cards (currently these are quick answers and mahi
// cards).
class ReadWriteCardsUiController {
 public:
  ReadWriteCardsUiController();

  ReadWriteCardsUiController(const ReadWriteCardsUiController&) = delete;
  ReadWriteCardsUiController& operator=(const ReadWriteCardsUiController&) =
      delete;

  ~ReadWriteCardsUiController();

  // Sets/removes quick answers views. This view will be added into this widget
  // and used to calculate widget bounds.
  views::View* SetQuickAnswersView(std::unique_ptr<views::View> view);
  void RemoveQuickAnswersView();

  // Sets/removes mahi views. This view will be added into this widget and used
  // to calculate widget bounds.
  views::View* SetMahiView(std::unique_ptr<views::View> view);
  void RemoveMahiView();

  views::View* GetQuickAnswersViewForTest();
  views::View* GetMahiViewForTest();

  // Updates widget bounds.
  void UpdateWidgetBounds();

  void SetContextMenuBounds(const gfx::Rect& context_menu_bounds);

  views::Widget* widget_for_test() { return widget_.get(); }

 private:
  // Initializes `widget_` if needed.
  void CreateWidgetIfNeeded();

  // Hides `widget_` if necessary.
  void MaybeHideWidget();

  views::ViewTracker quick_answers_view_;
  views::ViewTracker mahi_view_;

  views::UniqueWidgetPtr widget_;

  // The bounds of the context menu, used to calculate the widget bounds.
  gfx::Rect context_menu_bounds_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_UI_CONTROLLER_H_
