// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

// Returns the margin on the left and right of the row.
int GetHorizontalMargin() {
  return base::FeatureList::IsEnabled(
             features::kAutofillShowAutocompleteDeleteButton)
             ? ChromeLayoutProvider::Get()->GetDistanceMetric(
                   DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)
             : 0;
}

}  // namespace

// static
std::unique_ptr<PopupRowView> PopupRowView::Create(PopupViewViews& popup_view,
                                                   int line_number) {
  base::WeakPtr<AutofillPopupController> controller = popup_view.controller();
  CHECK(controller);

  PopupItemId popup_item_id =
      controller->GetSuggestionAt(line_number).popup_item_id;
  std::unique_ptr<PopupRowStrategy> strategy;
  switch (popup_item_id) {
    // These `popup_item_id` should never be displayed in a `PopupRowView`.
    case PopupItemId::kSeparator:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
      NOTREACHED_NORETURN();
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
    case PopupItemId::kAccountStoragePasswordEntry:
      strategy = std::make_unique<PopupPasswordSuggestionStrategy>(controller,
                                                                   line_number);
      break;
    default:
      if (IsFooterPopupItemId(popup_item_id)) {
        strategy =
            std::make_unique<PopupFooterStrategy>(controller, line_number);
      } else {
        strategy =
            std::make_unique<PopupSuggestionStrategy>(controller, line_number);
      }
      break;
  }

  return std::make_unique<PopupRowView>(
      /*a11y_selection_delegate=*/popup_view, /*selection_delegate=*/popup_view,
      controller, std::move(strategy));
}

PopupRowView::PopupRowView(
    AccessibilitySelectionDelegate& a11y_selection_delegate,
    SelectionDelegate& selection_delegate,
    base::WeakPtr<AutofillPopupController> controller,
    std::unique_ptr<PopupRowStrategy> strategy)
    : a11y_selection_delegate_(a11y_selection_delegate),
      selection_delegate_(selection_delegate),
      controller_(controller),
      strategy_(std::move(strategy)) {
  CHECK(strategy_);

  SetProperty(views::kMarginsKey, gfx::Insets::VH(0, GetHorizontalMargin()));
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>());

  auto add_exit_enter_callbacks = [&](CellType type, PopupCellView& cell) {
    cell.SetOnExitedCallback(
        base::BindRepeating(&SelectionDelegate::SetSelectedCell,
                            base::Unretained(&selection_delegate),
                            absl::nullopt, PopupCellSelectionSource::kMouse));
    cell.SetOnEnteredCallback(base::BindRepeating(
        &SelectionDelegate::SetSelectedCell,
        base::Unretained(&selection_delegate),
        PopupViewViews::CellIndex{strategy_->GetLineNumber(), type},
        PopupCellSelectionSource::kMouse));
  };

  content_view_ = AddChildView(strategy_->CreateContent());
  add_exit_enter_callbacks(CellType::kContent, *content_view_);
  layout->SetFlexForView(content_view_.get(), 1);

  if (std::unique_ptr<PopupCellView> control_view =
          strategy_->CreateControl()) {
    control_view_ = AddChildView(std::move(control_view));
    add_exit_enter_callbacks(CellType::kControl, *control_view_);
    layout->SetFlexForView(control_view_.get(), 0);
  }
}

PopupRowView::~PopupRowView() = default;

void PopupRowView::SetSelectedCell(absl::optional<CellType> cell) {
  if (cell == selected_cell_) {
    return;
  }

  PopupCellView* old_view =
      selected_cell_ ? GetCellView(*selected_cell_) : nullptr;
  if (old_view) {
    old_view->SetSelected(false);
  }

  PopupCellView* new_view = cell ? GetCellView(*cell) : nullptr;
  if (new_view) {
    new_view->SetSelected(true);
    GetA11ySelectionDelegate().NotifyAXSelection(*new_view);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
    selected_cell_ = cell;
  } else {
    // Set the selected cell to none in case an invalid choice was made (e.g.
    // selecting a control cell when none exists) or the cell was reset
    // explicitly with `absl::nullopt`.
    selected_cell_ = absl::nullopt;
  }

  UpdateBackground();
}

void PopupRowView::SetCellPermanentlyHighlighted(CellType type,
                                                 bool highlighted) {
  if (PopupCellView* view = GetCellView(type)) {
    view->SetPermanentlyHighlighted(highlighted);
  }

  UpdateBackground();
}

gfx::RectF PopupRowView::GetCellBounds(CellType cell) const {
  const PopupCellView* view = GetCellView(cell);
  // The view is expected to be present.
  gfx::RectF bounds = gfx::RectF(view->GetBoundsInScreen());
  bounds.Outset(GetHorizontalMargin());
  return bounds;
}

bool PopupRowView::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Some cells may want to define their own behavior.
  CHECK(GetSelectedCell());
  if (*GetSelectedCell() == CellType::kControl &&
      control_view_->HandleKeyPressEvent(event)) {
    return true;
  }
  if (*GetSelectedCell() == CellType::kContent &&
      content_view_->HandleKeyPressEvent(event)) {
    return true;
  }
  return false;
}

const PopupCellView* PopupRowView::GetCellView(CellType type) const {
  switch (type) {
    case CellType::kContent:
      return content_view_.get();
    case CellType::kControl:
      return control_view_.get();
  }
}

PopupCellView* PopupRowView::GetCellView(CellType type) {
  return const_cast<PopupCellView*>(std::as_const(*this).GetCellView(type));
}

void PopupRowView::UpdateBackground() {
  PopupCellView* control_cell = GetCellView(CellType::kControl);
  if (!control_cell) {
    return;
  }

  ui::ColorId kBackgroundColorId = control_cell->IsHighlighted()
                                       ? ui::kColorDropdownBackgroundSelected
                                       : ui::kColorDropdownBackground;
  SetBackground(views::CreateThemedRoundedRectBackground(
      kBackgroundColorId, ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                              views::Emphasis::kMedium)));
}

BEGIN_METADATA(PopupRowView, views::View)
ADD_PROPERTY_METADATA(absl::optional<PopupRowView::CellType>, SelectedCell)
END_METADATA

}  // namespace autofill

DEFINE_ENUM_CONVERTERS(autofill::PopupRowView::CellType,
                       {autofill::PopupRowView::CellType::kContent,
                        u"kContent"},
                       {autofill::PopupRowView::CellType::kControl,
                        u"kControl"})
