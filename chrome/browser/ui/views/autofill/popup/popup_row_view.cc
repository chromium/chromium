// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

// static
std::unique_ptr<PopupRowView> PopupRowView::Create(PopupViewViews& popup_view,
                                                   int line_number) {
  base::WeakPtr<AutofillPopupController> controller = popup_view.controller();
  DCHECK(controller);

  int frontend_id = controller->GetSuggestionAt(line_number).frontend_id;
  std::unique_ptr<PopupRowStrategy> strategy;
  switch (frontend_id) {
    // These frontend ids should never be displayed in a `PopupRowView`.
    case PopupItemId::POPUP_ITEM_ID_SEPARATOR:
    case PopupItemId::POPUP_ITEM_ID_MIXED_FORM_MESSAGE:
    case PopupItemId::POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
      NOTREACHED();
      break;
    case PopupItemId::POPUP_ITEM_ID_USERNAME_ENTRY:
    case PopupItemId::POPUP_ITEM_ID_PASSWORD_ENTRY:
    case PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY:
    case PopupItemId::POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY:
      strategy = std::make_unique<PopupPasswordSuggestionStrategy>(controller,
                                                                   line_number);
      break;
    default:
      if (IsFooterFrontendId(frontend_id)) {
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
  // TODO(crbug.com/1411172): Use a BoxLayout once controls are supported.
  SetUseDefaultFillLayout(true);
  content_view_ = AddChildView(strategy_->CreateContent());
  content_view_->SetOnExitedCallback(base::BindRepeating(
      &SelectionDelegate::SetSelectedCell,
      base::Unretained(&selection_delegate), absl::nullopt));
  content_view_->SetOnEnteredCallback(base::BindRepeating(
      &SelectionDelegate::SetSelectedCell,
      base::Unretained(&selection_delegate),
      PopupViewViews::CellIndex{strategy_->GetLineNumber(),
                                PopupRowView::CellType::kContent}));
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
  } else {
    // Set the selected cell to none in case an invalid choice was made (e.g.
    // selecting a control cell when none exists).
    selected_cell_ = absl::nullopt;
  }
}

// TODO(crbug.com/1411172): Move to `PopupViewViews` class.
void PopupRowView::MaybeShowIphPromo() {
  if (!controller_) {
    return;
  }
  std::string feature_name =
      controller_->GetSuggestionAt(strategy_->GetLineNumber()).feature_for_iph;
  if (feature_name.empty()) {
    return;
  }

  if (feature_name == "IPH_AutofillVirtualCardSuggestion") {
    SetProperty(views::kElementIdentifierKey,
                kAutofillCreditCardSuggestionEntryElementId);
    Browser* browser = chrome::FindLastActive();
    if (!browser) {
      return;
    }
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
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
