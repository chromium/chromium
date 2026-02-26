// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_loading_view.h"

#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

PopupLoadingView::PopupLoadingView(int expected_number_of_suggestions) {
  SetUseDefaultFillLayout(true);

  const gfx::Size expected_size =
      CalculateSizeOfSuggestions(expected_number_of_suggestions);

  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetPreferredSize(expected_size)
          .AddChild(views::Builder<views::Throbber>().CustomConfigure(
              base::BindOnce([](views::Throbber* throbber) {
                throbber->Start();
                throbber->GetViewAccessibility().AnnouncePolitely(
                    l10n_util::GetStringUTF16(
                        IDS_AUTOFILL_BNPL_PROGRESS_DIALOG_LOADING_MESSAGE));
              })))
          .Build());

  GetViewAccessibility().SetRole(ax::mojom::Role::kProgressIndicator);
}

gfx::Size PopupLoadingView::CalculateSizeOfSuggestions(
    int expected_number_of_suggestions) const {
  Suggestion dummy_suggestion(SuggestionType::kBnplEntry);
  dummy_suggestion.main_text = Suggestion::Text(u"Dummy Issuer");
  dummy_suggestion.labels = {{Suggestion::Text(u"Pay in 4 installments")}};

  auto dummy_view = CreatePopupRowContentView(dummy_suggestion,
                                              /*show_new_badge=*/std::nullopt,
                                              FillingProduct::kCreditCard,
                                              /*filter_match=*/std::nullopt);

  gfx::Size single_suggestion_size = dummy_view->GetPreferredSize();

  return gfx::Size(
      single_suggestion_size.width(),
      single_suggestion_size.height() * expected_number_of_suggestions);
}

BEGIN_METADATA(PopupLoadingView)
END_METADATA

}  // namespace autofill
