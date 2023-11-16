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

namespace {

// Max width for the username and masked password.
constexpr int kAutofillPopupUsernameMaxWidth = 272;
constexpr int kAutofillPopupPasswordMaxWidth = 108;


}  // namespace

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

/************************ PopupPasswordSuggestionStrategy *******************/

PopupPasswordSuggestionStrategy::PopupPasswordSuggestionStrategy(
    base::WeakPtr<AutofillPopupController> controller,
    int line_number)
    : PopupRowBaseStrategy(std::move(controller), line_number) {}

PopupPasswordSuggestionStrategy::~PopupPasswordSuggestionStrategy() = default;

std::unique_ptr<PopupRowContentView>
PopupPasswordSuggestionStrategy::CreateContent() {
  if (!GetController()) {
    return nullptr;
  }

  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  auto view = std::make_unique<PopupRowContentView>();

  // Add the actual views.
  std::unique_ptr<views::Label> main_text_label =
      popup_cell_utils::CreateMainTextLabel(
          kSuggestion.main_text, views::style::TextStyle::STYLE_PRIMARY);
  main_text_label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);

  popup_cell_utils::AddSuggestionContentToView(
      kSuggestion, std::move(main_text_label),
      popup_cell_utils::CreateMinorTextLabel(kSuggestion.minor_text),
      CreateDescriptionLabel(), CreateAndTrackSubtextViews(*view), *view);

  return view;
}

std::unique_ptr<views::Label>
PopupPasswordSuggestionStrategy::CreateDescriptionLabel() const {
  const Suggestion& kSuggestion =
      GetController()->GetSuggestionAt(GetLineNumber());
  if (kSuggestion.labels.empty()) {
    return nullptr;
  }

  DCHECK_EQ(kSuggestion.labels.size(), 1u);
  DCHECK_EQ(kSuggestion.labels[0].size(), 1u);

  auto label = std::make_unique<views::Label>(
      kSuggestion.labels[0][0].value, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::ELIDE_HEAD);
  label->SetMaximumWidthSingleLine(kAutofillPopupUsernameMaxWidth);
  return label;
}

std::vector<std::unique_ptr<views::View>>
PopupPasswordSuggestionStrategy::CreateAndTrackSubtextViews(
    PopupRowContentView& content_view) const {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      GetController()->GetSuggestionAt(GetLineNumber()).additional_label,
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  label->SetElideBehavior(gfx::TRUNCATE);
  label->SetMaximumWidthSingleLine(kAutofillPopupPasswordMaxWidth);
  content_view.TrackLabel(label.get());
  std::vector<std::unique_ptr<views::View>> result;
  result.push_back(std::move(label));
  return result;
}

}  // namespace autofill
