// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
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
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/type_conversion.h"

namespace autofill {

// static
std::unique_ptr<PopupRowView> PopupRowView::Create(PopupViewViews& popup_view,
                                                   int line_number) {
  base::WeakPtr<AutofillPopupController> controller = popup_view.controller();
  DCHECK(controller);

  Suggestion::FrontendId frontend_id =
      controller->GetSuggestionAt(line_number).frontend_id;
  std::unique_ptr<PopupRowStrategy> strategy;
  switch (frontend_id.as_popup_item_id()) {
    // These frontend ids should never be displayed in a `PopupRowView`.
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
      if (IsFooterFrontendId(frontend_id.as_popup_item_id())) {
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
      controller_(controller),
      strategy_(std::move(strategy)) {
  DCHECK(strategy_);
  const int kHorizontalPadding =
      base::FeatureList::IsEnabled(
          features::kAutofillShowAutocompleteDeleteButton)
          ? ChromeLayoutProvider::Get()->GetDistanceMetric(
                DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)
          : 0;
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_inside_border_insets(gfx::Insets::VH(0, kHorizontalPadding));

  auto add_exit_enter_callbacks = [&](CellType type, PopupCellView& cell) {
    cell.SetOnExitedCallback(base::BindRepeating(
        &SelectionDelegate::SetSelectedCell,
        base::Unretained(&selection_delegate), absl::nullopt));
    cell.SetOnEnteredCallback(base::BindRepeating(
        &SelectionDelegate::SetSelectedCell,
        base::Unretained(&selection_delegate),
        PopupViewViews::CellIndex{strategy_->GetLineNumber(), type}));
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

  auto view_from_type =
      [this](absl::optional<CellType> type) -> PopupCellView* {
    if (!type) {
      return nullptr;
    }
    switch (*type) {
      case CellType::kContent:
        return content_view_.get();
      case CellType::kControl:
        return control_view_.get();
    }
  };

  if (PopupCellView* old_view = view_from_type(selected_cell_)) {
    old_view->SetSelected(false);
  }
  selected_cell_ = cell;

  if (PopupCellView* new_view = view_from_type(selected_cell_)) {
    new_view->SetSelected(true);
    GetA11ySelectionDelegate().NotifyAXSelection(*new_view);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
  } else {
    // Set the selected cell to none in case an invalid choice was made (e.g.
    // selecting a control cell when none exists).
    selected_cell_ = absl::nullopt;
  }
}

bool PopupRowView::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windows_key_code) {
    case ui::VKEY_RETURN:
      if (*GetSelectedCell() == CellType::kControl &&
          GetControlView()->GetOnAcceptedCallback()) {
        GetControlView()->GetOnAcceptedCallback().Run();
        return true;
      }
      // TODO(crbug.com/1411172): Handle all return key presses here once the
      // reaction delay for accepting suggestions is the same between keyboard
      // and mouse/gesture events.
      return false;
    case ui::VKEY_LEFT:
      // `base::i18n::IsRTL` is used here instead of the controller's method
      // because the controller's `IsRTL` depends on the language of the focused
      // field and not the overall UI language. However, the layout of the popup
      // is determined by the overall UI language.
      if (base::i18n::IsRTL()) {
        SelectNextCell();
      } else {
        SelectPreviousCell();
      }
      return true;
    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL()) {
        SelectPreviousCell();
      } else {
        SelectNextCell();
      }
      return true;
    default:
      return false;
  }
}

void PopupRowView::SelectNextCell() {
  DCHECK(GetSelectedCell());
  if (*GetSelectedCell() == CellType::kContent && GetControlView()) {
    SetSelectedCell(CellType::kControl);
  }
}

void PopupRowView::SelectPreviousCell() {
  DCHECK(GetSelectedCell());
  if (*GetSelectedCell() == CellType::kControl) {
    SetSelectedCell(CellType::kContent);
  }
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
