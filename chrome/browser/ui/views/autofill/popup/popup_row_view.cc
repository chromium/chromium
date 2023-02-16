// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

  return std::make_unique<PopupRowView>(popup_view, std::move(strategy));
}

PopupRowView::PopupRowView(PopupViewViews& popup_view,
                           std::unique_ptr<PopupRowStrategy> strategy)
    : popup_view_(popup_view), strategy_(std::move(strategy)) {
  // TODO(crbug.com/1411172): Use a BoxLayout once controls are supported.
  SetUseDefaultFillLayout(true);
  content_view_ = AddChildView(strategy_->CreateContent());
}

PopupRowView::~PopupRowView() = default;

void PopupRowView::SetSelected(bool selected) {
  if (selected == selected_) {
    return;
  }

  selected_ = selected;
  GetContentView().SetSelected(selected_);
  if (selected_) {
    GetPopupView().NotifyAXSelection(GetContentView());
  }
}

void PopupRowView::MaybeShowIphPromo() {
  std::string feature_name = GetPopupView()
                                 .controller()
                                 ->GetSuggestionAt(strategy_->GetLineNumber())
                                 .feature_for_iph;
  if (feature_name.empty()) {
    return;
  }

  if (feature_name == "IPH_AutofillVirtualCardSuggestion") {
    SetProperty(views::kElementIdentifierKey,
                kAutofillCreditCardSuggestionEntryElementId);
    Browser* browser = GetPopupView().browser();
    DCHECK(browser);
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  }
}

BEGIN_METADATA(PopupRowView, views::View)
ADD_PROPERTY_METADATA(bool, Selected)
END_METADATA

}  // namespace autofill
