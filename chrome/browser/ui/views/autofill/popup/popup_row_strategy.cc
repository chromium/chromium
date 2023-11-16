// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/views/new_badge_label.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace autofill {

/**************************** PopupRowBaseStrategy ****************************/

PopupRowBaseStrategy::PopupRowBaseStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : controller_(controller), line_number_(line_number) {
  DCHECK(controller_);
}

PopupRowBaseStrategy::~PopupRowBaseStrategy() = default;

int PopupRowBaseStrategy::GetLineNumber() const {
  return line_number_;
}

/************************** PopupSuggestionStrategy ***************************/

PopupSuggestionStrategy::PopupSuggestionStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : PopupRowBaseStrategy(std::move(controller), line_number) {}

PopupSuggestionStrategy::~PopupSuggestionStrategy() = default;

std::unique_ptr<PopupRowContentView> PopupSuggestionStrategy::CreateContent() {
  if (!GetController()) {
    return nullptr;
  }

  auto view = std::make_unique<PopupRowContentView>();
  AddContentLabelsAndCallbacks(*view);
  return view;
}

void PopupSuggestionStrategy::AddContentLabelsAndCallbacks(
    PopupRowContentView& view) {
  // Add the actual views.
  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text,
          GetMainTextStyleForPopupItemId(kSuggestion.popup_item_id));
  popup_cell_utils::FormatLabel(*main_text_label, kSuggestion.main_text,
                                GetController());
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      /*description_label=*/nullptr,
      popup_cell_utils::CreateAndTrackSubtextViews(view, GetController(),
                                                   GetLineNumber()),
      view);
}

/************************ PopupComposeSuggestionStrategy ********************/

PopupComposeSuggestionStrategy::PopupComposeSuggestionStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    bool show_new_badge)
    : PopupRowBaseStrategy(std::move(controller), line_number),
      show_new_badge_(show_new_badge) {}

PopupComposeSuggestionStrategy::~PopupComposeSuggestionStrategy() = default;

std::unique_ptr<PopupRowContentView>
PopupComposeSuggestionStrategy::CreateContent() {
  if (!GetController()) {
    return nullptr;
  }

  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  auto view = std::make_unique<PopupRowContentView>();
  auto main_text_label = std::make_unique<user_education::NewBadgeLabel>(
      kSuggestion.main_text.value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_3_MEDIUM);
  main_text_label->SetDisplayNewBadge(show_new_badge_);
  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      /*minor_text_label=*/nullptr,
      /*description_label=*/nullptr, /*subtext_views=*/
      popup_cell_utils::CreateAndTrackSubtextViews(
          *view, GetController(), GetLineNumber(), views::style::STYLE_BODY_4),
      *view);

  return view;
}

}  // namespace autofill
