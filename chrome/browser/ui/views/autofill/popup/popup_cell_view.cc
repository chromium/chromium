// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace autofill {

PopupCellView::PopupCellView() {
  SetNotifyEnterExitOnChild(true);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  RefreshStyle();
}

PopupCellView::~PopupCellView() = default;

void PopupCellView::SetSelected(bool selected) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;
  RefreshStyle();
  if (base::RepeatingClosure callback =
          selected_ ? on_selected_callback_ : on_unselected_callback_) {
    callback.Run();
  }
}

void PopupCellView::SetTooltipText(std::u16string tooltip_text) {
  if (tooltip_text_ == tooltip_text) {
    return;
  }

  tooltip_text_ = std::move(tooltip_text);
  TooltipTextChanged();
}

std::u16string PopupCellView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_text_;
}

void PopupCellView::SetAccessibilityDelegate(
    std::unique_ptr<AccessibilityDelegate> a11y_delegate) {
  a11y_delegate_ = std::move(a11y_delegate);
}

void PopupCellView::SetOnEnteredCallback(base::RepeatingClosure callback) {
  on_entered_callback_ = std::move(callback);
}

void PopupCellView::SetOnExitedCallback(base::RepeatingClosure callback) {
  on_exited_callback_ = std::move(callback);
}

void PopupCellView::SetOnAcceptedCallback(base::RepeatingClosure callback) {
  on_accepted_callback_ = std::move(callback);
}

void PopupCellView::SetOnSelectedCallback(base::RepeatingClosure callback) {
  on_selected_callback_ = std::move(callback);
}

void PopupCellView::SetOnUnselectedCallback(base::RepeatingClosure callback) {
  on_unselected_callback_ = std::move(callback);
}

void PopupCellView::TrackLabel(views::Label* label) {
  tracked_labels_.push_back(label);
}

bool PopupCellView::OnMouseDragged(const ui::MouseEvent& event) {
  // Return `true` to be informed about subsequent `OnMouseReleased` events.
  return true;
}

bool PopupCellView::OnMousePressed(const ui::MouseEvent& event) {
  // Return `true` to be informed about subsequent `OnMouseReleased` events.
  return true;
}

void PopupCellView::OnMouseEntered(const ui::MouseEvent& event) {
  // `OnMouseEntered()` does not imply that the mouse had been outside of the
  // item's bounds before: `OnMouseEntered()` fires if the mouse moves just
  // a little bit on the item. We don't want to show a preview in such a case.
  if (!mouse_observed_outside_item_bounds_) {
    return;
  }

  if (on_entered_callback_) {
    on_entered_callback_.Run();
  }
}

void PopupCellView::OnMouseExited(const ui::MouseEvent& event) {
  // `OnMouseExited()` does not imply that the mouse has left the item's screen
  // bounds: `OnMouseExited()` fires (on Windows, at least) when another popup
  // overlays this item and the mouse is above the new popup
  // (crbug.com/1287364).
  mouse_observed_outside_item_bounds_ |= !IsMouseInsideItemBounds();

  if (on_exited_callback_) {
    on_exited_callback_.Run();
  }
}

void PopupCellView::OnMouseReleased(const ui::MouseEvent& event) {
  // Ignore mouse clicks unless the user made the explicit choice to selected
  // the current item.
  if (!mouse_observed_outside_item_bounds_) {
    return;
  }

  if (on_accepted_callback_ && event.IsOnlyLeftMouseButton() &&
      HitTestPoint(event.location())) {
    on_accepted_callback_.Run();
  }
}

void PopupCellView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      if (on_entered_callback_) {
        on_entered_callback_.Run();
      }
      break;
    case ui::ET_GESTURE_TAP:
      if (on_accepted_callback_) {
        on_accepted_callback_.Run();
      }
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      if (on_exited_callback_) {
        on_exited_callback_.Run();
      }
      break;
    default:
      return;
  }
}

bool PopupCellView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kFocus && on_entered_callback_) {
    on_entered_callback_.Run();
  }
  return View::HandleAccessibleAction(action_data);
}

void PopupCellView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (a11y_delegate_) {
    a11y_delegate_->GetAccessibleNodeData(GetSelected(), node_data);
  }
}

void PopupCellView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  mouse_observed_outside_item_bounds_ |= !IsMouseInsideItemBounds();
}

void PopupCellView::RefreshStyle() {
  ui::ColorId kBackgroundColorId = GetSelected()
                                       ? ui::kColorDropdownBackgroundSelected
                                       : ui::kColorDropdownBackground;
  if (base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteDeleteButton)) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        kBackgroundColorId, ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                                views::Emphasis::kMedium)));
  } else {
    SetBackground(views::CreateThemedSolidBackground(kBackgroundColorId));
  }

  // Set style for each label in this cell depending on its current selection
  // state.
  for (views::Label* label : tracked_labels_) {
    label->SetAutoColorReadabilityEnabled(false);

    // If the current suggestion is selected or the label is disabled,
    // override the style. Otherwise, use the color that corresponds to the
    // actual style of the label.
    int style = label->GetEnabled()
                    ? (GetSelected() ? views::style::STYLE_SELECTED
                                     : label->GetTextStyle())
                    : views::style::STYLE_DISABLED;
    label->SetEnabledColorId(
        views::style::GetColorId(label->GetTextContext(), style));
  }

  SchedulePaint();
}

BEGIN_METADATA(PopupCellView, views::View)
ADD_PROPERTY_METADATA(bool, Selected)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
ADD_PROPERTY_METADATA(base::RepeatingClosure, OnEnteredCallback)
ADD_PROPERTY_METADATA(base::RepeatingClosure, OnExitedCallback)
ADD_PROPERTY_METADATA(base::RepeatingClosure, OnAcceptedCallback)
ADD_PROPERTY_METADATA(base::RepeatingClosure, OnSelectedCallback)
ADD_PROPERTY_METADATA(base::RepeatingClosure, OnUnselectedCallback)
END_METADATA

}  // namespace autofill
