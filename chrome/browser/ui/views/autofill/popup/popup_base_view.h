// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BASE_VIEW_H_

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace autofill {

// Class that deals with the event handling for Autofill-style popups. This
// class should only be instantiated by sub-classes.
class PopupBaseView : public PopupRowView::AccessibilitySelectionDelegate,
                      public views::WidgetDelegateView,
                      public views::WidgetFocusChangeListener,
                      public views::WidgetObserver {
  METADATA_HEADER(PopupBaseView, views::WidgetDelegateView)

 public:
  // Consider the input element is `kElementBorderPadding` pixels larger at the
  // top and at the bottom in order to reposition the dropdown, so that it
  // doesn't look too close to the element.
  static constexpr int kElementBorderPadding = 1;

  // Default list of the preferred popup sides adjacent to the target element.
  static constexpr std::array<views::BubbleArrowSide, 4>
      kDefaultPreferredPopupSides = {
          views::BubbleArrowSide::kTop, views::BubbleArrowSide::kBottom,
          views::BubbleArrowSide::kLeft, views::BubbleArrowSide::kRight};

  PopupBaseView(const PopupBaseView&) = delete;
  PopupBaseView& operator=(const PopupBaseView&) = delete;

  static int GetCornerRadius();
  // Returns the horizontal margin between the arrow and the edge of the view.
  // Used to align child elements to the popup arrow.
  static int ArrowHorizontalMargin();

  // Notify accessibility that an item has been selected.
  void NotifyAXSelection(views::View& view) override;

  // Returns the browser in which this popup is shown.
  Browser* GetBrowser();

 protected:
  PopupBaseView(base::WeakPtr<AutofillPopupViewDelegate> delegate,
                views::Widget* parent_widget,
                views::Widget::InitParams::Activatable new_widget_activatable =
                    views::Widget::InitParams::Activatable::kDefault,
                bool show_arrow_pointer = true);
  ~PopupBaseView() override;

  // Show this popup. Idempotent. Returns |true| if popup is shown, |false|
  // otherwise.
  bool DoShow();

  // Hide the widget and delete |this|.
  void DoHide();

  // Ensure the child views are not rendered beyond the popup border
  // boundaries.
  void UpdateClipPath();

  // Returns the bounds of the containing browser window in screen space.
  gfx::Rect GetTopWindowBounds() const;

  // Returns the bounds of the content area in screen space.
  gfx::Rect GetContentAreaBounds() const;

  // Update size of popup and paint. If there is insufficient height to draw the
  // popup, it hides and thus deletes |this| and returns false. (virtual for
  // testing).
  virtual bool DoUpdateBoundsAndRedrawPopup();

  // Returns the optimal bounds to place the popup with `preferred_size` and
  // places an arrow on the popup border to point towards `element_bounds`
  // within `max_bounds_for_popup`. The `preferred_popup_sides` are tried
  // one-by-one until a side with enough space is found.
  virtual gfx::Rect GetOptimalPositionAndPlaceArrowOnPopup(
      const gfx::Rect& element_bounds,
      const gfx::Rect& max_bounds_for_popup,
      const gfx::Size& preferred_size,
      base::span<const views::BubbleArrowSide> preferred_popup_sides);

 private:
  friend class PopupBaseViewBrowsertest;

  class Widget;

  // views::WidgetFocusChangeListener implementation.
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // views::WidgetObserver implementation.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Stop observing the widget.
  void RemoveWidgetObservers();

  // Hide the controller of this view. This assumes that doing so will
  // eventually hide this view in the process.
  void HideController(SuggestionHidingReason reason);

  // Return the web contents related to this.
  content::WebContents* GetWebContents() const;

  // Scoped observation for focus events.
  base::ScopedObservation<views::WidgetFocusManager,
                          views::WidgetFocusChangeListener>
      focus_observation_{this};

  // Controller for this popup. Weak reference.
  base::WeakPtr<AutofillPopupViewDelegate> delegate_;

  // The widget of the window that triggered this popup. Weak reference.
  raw_ptr<views::Widget> parent_widget_ = nullptr;

  // The corresponding parameter for newly created widget (in `DoShow()`).
  const views::Widget::InitParams::Activatable new_widget_activatable_;

  const bool show_arrow_pointer_;

  // Ensures that the menu start event is not fired redundantly.
  bool is_ax_menu_start_event_fired_ = false;

  // Responsible for blocking (and re-enabling) custom cursors across all
  // browser windows.
  CustomCursorSuppressor custom_cursor_suppressor_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_BASE_VIEW_H_
