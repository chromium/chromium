// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_row_view.h"

#include <string>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(pbos): Reconsider how to construct accessible names from these nodes.
// Right now this concatenates (with newlines) every Label inside the row to
// ensure that no data is inaccessible.
std::u16string GetAccessibleNameFromTree(views::View* view) {
  if (views::IsViewClass<views::Label>(view))
    return static_cast<views::Label*>(view)
        ->GetViewAccessibility()
        .GetCachedName();

  std::u16string accessible_name;
  for (views::View* child : view->children()) {
    // Skip buttons they will be announced independently. This is used for
    // "more" items.
    if (views::IsViewClass<views::Button>(child))
      continue;
    std::u16string child_accessible_name = GetAccessibleNameFromTree(child);
    if (child_accessible_name.empty())
      continue;
    if (!accessible_name.empty())
      accessible_name += '\n';
    accessible_name += child_accessible_name;
  }
  return accessible_name;
}

}  // namespace

namespace payments {

PaymentRequestRowView::PaymentRequestRowView()
    : PaymentRequestRowView(PressedCallback(),
                            /*clickable=*/true,
                            gfx::Insets()) {}

PaymentRequestRowView::PaymentRequestRowView(PressedCallback callback,
                                             bool clickable,
                                             const gfx::Insets& insets)
    : views::Button(std::move(callback)),
      clickable_(clickable),
      row_insets_(insets) {
  UpdateButtonState();
  SetBottomSeparatorVisible(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
}

PaymentRequestRowView::~PaymentRequestRowView() = default;

bool PaymentRequestRowView::GetClickable() const {
  return clickable_;
}
void PaymentRequestRowView::SetClickable(bool clickable) {
  if (clickable == clickable_)
    return;
  clickable_ = clickable;
  UpdateButtonState();
  OnPropertyChanged(&clickable_, views::PropertyEffects::kPropertyEffectsPaint);
}

base::WeakPtr<PaymentRequestRowView> PaymentRequestRowView::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

gfx::Insets PaymentRequestRowView::GetRowInsets() const {
  return row_insets_;
}

void PaymentRequestRowView::SetRowInsets(const gfx::Insets& row_insets) {
  if (row_insets == row_insets_)
    return;
  row_insets_ = row_insets;
  UpdateBottomSeparatorVisualState();
  OnPropertyChanged(&row_insets_,
                    views::PropertyEffects::kPropertyEffectsPaint);
}

void PaymentRequestRowView::SetBottomSeparatorVisible(bool visible) {
  bottom_separator_visible_ = visible;
  UpdateBottomSeparatorVisualState();
}

void PaymentRequestRowView::UpdateBottomSeparatorVisualState() {
  // Create an empty border even when not present in a Widget hierarchy as the
  // border is needed to correctly compute the bounds of the ScrollView in the
  // PaymentRequestSheetController which is done before this is added to its
  // Widget.
  // TODO(crbug.com/40768647): Update PaymentRequestSheetController to recompute
  // the bounds of its ScrollView in response to changes in preferred size.
  SetBorder(
      bottom_separator_visible_ && GetWidget()
          ? payments::CreatePaymentRequestRowBorder(
                GetColorProvider()->GetColor(ui::kColorSeparator), row_insets_)
          : views::CreateEmptyBorder(row_insets_));
}

void PaymentRequestRowView::SetHighlighted(bool highlighted) {
  if (highlighted) {
    SetBackground(views::CreateThemedSolidBackground(
        kColorPaymentsRequestRowBackgroundHighlighted));
    SetBottomSeparatorVisible(false);
    if (previous_row_)
      previous_row_->SetBottomSeparatorVisible(false);
  } else {
    SetBackground(nullptr);
    SetBottomSeparatorVisible(true);
    if (previous_row_)
      previous_row_->SetBottomSeparatorVisible(true);
  }
}

void PaymentRequestRowView::UpdateButtonState() {
  // When not clickable, use Button's STATE_DISABLED but don't set our
  // View state to disabled. The former ensures we aren't clickable, the
  // latter also disables us and our children for event handling.
  views::Button::SetState(clickable_ ? views::Button::STATE_NORMAL
                                     : views::Button::STATE_DISABLED);
}

void PaymentRequestRowView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Button::GetAccessibleNodeData(node_data);
  node_data->SetNameChecked(GetAccessibleNameFromTree(this));
}

void PaymentRequestRowView::StateChanged(ButtonState old_state) {
  Button::StateChanged(old_state);
  if (!GetClickable())
    return;

  SetHighlighted(GetState() == views::Button::STATE_HOVERED ||
                 GetState() == views::Button::STATE_PRESSED);
}

void PaymentRequestRowView::OnThemeChanged() {
  Button::OnThemeChanged();
  UpdateBottomSeparatorVisualState();
}

void PaymentRequestRowView::OnFocus() {
  if (GetClickable())
    SetHighlighted(true);
  View::OnFocus();
  if (views::FocusRing* focus_ring = views::FocusRing::Get(this)) {
    focus_ring->SetProperty(views::kViewIgnoredByLayoutKey, true);
  }
}

void PaymentRequestRowView::OnBlur() {
  if (GetClickable())
    SetHighlighted(false);
}

BEGIN_METADATA(PaymentRequestRowView)
ADD_PROPERTY_METADATA(bool, Clickable)
ADD_PROPERTY_METADATA(gfx::Insets, RowInsets)
END_METADATA

}  // namespace payments
