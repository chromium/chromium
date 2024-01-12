// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ROW_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace payments {

// This class implements a clickable row of the Payment Request dialog that
// darkens on hover and displays a horizontal ruler on its lower bound.
class PaymentRequestRowView : public views::Button {
  METADATA_HEADER(PaymentRequestRowView, views::Button)

 public:
  PaymentRequestRowView();
  // Creates a row view. If |clickable| is true, the row will be shaded on hover
  // and handle click events. |insets| are used as padding around the content.
  PaymentRequestRowView(PressedCallback callback,
                        bool clickable,
                        const gfx::Insets& insets);
  PaymentRequestRowView(const PaymentRequestRowView&) = delete;
  PaymentRequestRowView& operator=(const PaymentRequestRowView&) = delete;
  ~PaymentRequestRowView() override;

  gfx::Insets GetRowInsets() const;
  void SetRowInsets(const gfx::Insets& row_insets);

  bool GetClickable() const;
  void SetClickable(bool clickable);

  void set_previous_row(base::WeakPtr<PaymentRequestRowView> previous_row) {
    previous_row_ = previous_row;
  }

  // Deriving classes must override this and provide their own factory.
  virtual base::WeakPtr<PaymentRequestRowView> AsWeakPtr();

 private:
  // Show/hide the separator at the bottom of the row. This is used to hide the
  // separator when the row is hovered.
  void SetBottomSeparatorVisible(bool visible);

  // Updates the visual state to reflect `bottom_separator_visible_`.
  void UpdateBottomSeparatorVisualState();

  // Sets the row as |highlighted| or not. A row is highlighted if it's hovered
  // on or focused, in which case it hides its bottom separator and gets a light
  // colored background color.
  void SetHighlighted(bool highlighted);

  // Updates the button state to reflect the |clickable_| state.
  void UpdateButtonState();

  // views::Button:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void StateChanged(ButtonState old_state) override;
  void OnThemeChanged() override;

  // views::View:
  void OnFocus() override;
  void OnBlur() override;

  bool clickable_ = true;
  bool bottom_separator_visible_ = false;
  gfx::Insets row_insets_;

  // A non-owned pointer to the previous row object in the UI. Used to hide the
  // bottom border of the previous row when highlighting this one. May be null.
  base::WeakPtr<PaymentRequestRowView> previous_row_;

  base::WeakPtrFactory<PaymentRequestRowView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(, PaymentRequestRowView, views::Button)
VIEW_BUILDER_PROPERTY(bool, Clickable)
VIEW_BUILDER_PROPERTY(gfx::Insets, RowInsets)
END_VIEW_BUILDER

}  // namespace payments

DEFINE_VIEW_BUILDER(, payments::PaymentRequestRowView)

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ROW_VIEW_H_
