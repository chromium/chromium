// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_UI_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace chromeos {

class ReadWriteCardsView;

// The controller that manages all the behaviors of the UI widget containing
// some of the read write cards (currently these are quick answers and mahi
// cards).
class ReadWriteCardsUiController : public views::ViewObserver {
 public:
  static constexpr char kWidgetName[] = "ReadWriteCardsWidget";

  ReadWriteCardsUiController();

  ReadWriteCardsUiController(const ReadWriteCardsUiController&) = delete;
  ReadWriteCardsUiController& operator=(const ReadWriteCardsUiController&) =
      delete;

  ~ReadWriteCardsUiController() override;

  // Sets/removes a view for quick answers ui. This view will be added into this
  // widget and used to calculate widget bounds. This can be either
  // `QuickAnswersView` or `UserConsentView`.
  ReadWriteCardsView* SetQuickAnswersUi(
      std::unique_ptr<ReadWriteCardsView> view);
  void RemoveQuickAnswersUi();

  // Sets/removes mahi views. This view will be added into this widget and used
  // to calculate widget bounds.
  // TODO(b/331132971): Use `ReadWriteCardsView` for Mahi view.
  // TODO(b/331271987): Make this API symmetric with `SetQuickAnswersUi`, both
  //                    name and behavior.
  views::View* SetMahiView(std::unique_ptr<views::View> view);
  void RemoveMahiView();

  ReadWriteCardsView* GetQuickAnswersViewForTest();
  views::View* GetMahiViewForTest();

  void SetContextMenuBounds(const gfx::Rect& context_menu_bounds);

  const gfx::Rect& context_menu_bounds() const { return context_menu_bounds_; }

  views::Widget* widget_for_test() const { return widget_.get(); }

 private:
  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewLayoutInvalidated(views::View* view) override;

  // Initializes `widget_` if needed.
  void CreateWidgetIfNeeded();

  // Hides `widget_` if necessary.
  void MaybeHideWidget();

  // Reorder the child views inside `widget_`, depending on if the widget is
  // above or below the context menu.
  void ReorderChildViews(bool widget_above_context_menu);

  // Re-layout a widget and views. This includes updating the widget bounds and
  // reorder child views, if needed.
  void Relayout();

  ReadWriteCardsView* quick_answers_ui() {
    return quick_answers_ui_observation_.GetSource();
  }

  base::ScopedObservation<ReadWriteCardsView, ReadWriteCardsUiController>
      quick_answers_ui_observation_{this};

  views::ViewTracker mahi_view_;

  views::UniqueWidgetPtr widget_;

  // The bounds of the context menu, used to calculate the widget bounds.
  gfx::Rect context_menu_bounds_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_UI_CONTROLLER_H_
