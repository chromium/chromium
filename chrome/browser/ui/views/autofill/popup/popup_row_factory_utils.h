// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_FACTORY_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_FACTORY_UTILS_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/password_favicon_loader.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"

namespace autofill {

// Creates the content view for regular suggestions like address and credit
// cards. Views for suggestions of other types and special suggestions are
// created by corresponding `Create*PopupRowContentView()` methods.
std::unique_ptr<PopupRowContentView> CreatePopupRowContentView(
    const Suggestion& suggestion,
    std::optional<user_education::DisplayNewBadge> show_new_badge,
    FillingProduct main_filling_product,
    std::optional<AutofillPopupController::SuggestionFilterMatch> filter_match);

// Creates a row view depending on the suggestion type at `line_number`.
// If `filter_match` is provided, it is used for highlighting the suggestion
// label parts accordingly.
std::unique_ptr<PopupRowView> CreatePopupRowView(
    base::WeakPtr<AutofillPopupController> controller,
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    int line_number,
    std::optional<AutofillPopupController::SuggestionFilterMatch> filter_match =
        std::nullopt,
    PasswordFaviconLoader* favicon_loader = nullptr);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_ROW_FACTORY_UTILS_H_
