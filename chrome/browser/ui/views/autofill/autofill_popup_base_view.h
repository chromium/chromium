// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Point;
}

namespace autofill {

// Class that deals with the event handling for Autofill-style popups. This
// class should only be instantiated by sub-classes.
class AutofillPopupBaseView : public views::WidgetDelegateView,
                              public views::WidgetFocusChangeListener,
                              public views::WidgetObserver {
 public:
  static const SkColor kBackgroundColor;
  static const SkColor kSelectedBackgroundColor;
  static const SkColor kFooterBackgroundColor;
  static const SkColor kSeparatorColor;
  static const SkColor kWarningColor;

  // Consider the input element is |kElementBorderPadding| pixels larger at the
  // top and at the bottom in order to reposition the dropdown, so that it
  // doesn't look too close to the element.
  static const int kElementBorderPadding = 1;

  // Horizontal spacing between value and description in the row.
  // TODO(crbug.com/876364): Replace this with a global constant.
  static const int kValueLabelPadding = 24;

  static int GetCornerRadius();

 protected:
  explicit AutofillPopupBaseView(AutofillPopupViewDelegate* delegate,
                                 views::Widget* parent_widget);
  ~AutofillPopupBaseView() override;

  // Show this popup. Idempotent.
  void DoShow();

  // Hide the widget and delete |this|.
  void DoHide();

  // TODO(crbug.com/831603): make the methods private and non-virtual when
  // AutofillPopupViewViews is gone.
  virtual void AddExtraInitParams(views::Widget::InitParams* params);

  // Returns the widget's contents view.
  // TODO(crbug.com/831603): remove.
  virtual std::unique_ptr<views::View> CreateWrapperView();

  // Returns the border to be applied to the popup.
  virtual std::unique_ptr<views::Border> CreateBorder();

  // Ensure the child views are not rendered beyond the bubble border
  // boundaries. Should be overridden together with CreateBorder.
  virtual void SetClipPath();

  // Update size of popup and paint (virtual for testing).
  virtual void DoUpdateBoundsAndRedrawPopup();

  // Compute the space available for the popup. It's the space between its top
  // and the bottom of its parent view, minus some margin space.
  gfx::Rect CalculateClippingBounds() const;

  const AutofillPopupViewDelegate* delegate() { return delegate_; }

 private:
  friend class AutofillPopupBaseViewTest;

  // views::Views implementation.
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // views::WidgetFocusChangeListener implementation.
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // views::WidgetObserver implementation.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Stop observing the widget.
  void RemoveWidgetObservers();

  void SetSelection(const gfx::Point& point);
  void AcceptSelection(const gfx::Point& point);
  void ClearSelection();

  // Hide the controller of this view. This assumes that doing so will
  // eventually hide this view in the process.
  void HideController();

  // Must return the container view for this popup.
  gfx::NativeView container_view();

  // Controller for this popup. Weak reference.
  AutofillPopupViewDelegate* delegate_;

  // The widget of the window that triggered this popup. Weak reference.
  views::Widget* parent_widget_;

  // The time when the popup was shown.
  base::Time show_time_;

  base::WeakPtrFactory<AutofillPopupBaseView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupBaseView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_BASE_VIEW_H_
