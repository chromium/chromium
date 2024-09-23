// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_BUTTON_ACTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_BUTTON_ACTION_H_

#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

// Used by `FillingProduct::kPredictionImprovements` to offers users more
// suggestions interaction options.
enum class PredictionImprovementsButtonActions {
  // Records that the user has given a good feedback about the feature.
  kThumbsUpClicked,
  // Records that the user has given a bad feedback about the feature.
  kThumbsDownClicked,
  // Navigates the user to a page where they can read about the feature.
  kLearnMoreClicked
};

// A `Suggestion` may have additional UI items that a user can interact with.
// Examples include refresh buttons or feedback buttons.
// `SuggestionButtonAction` is a type that allows the
// `AutofillSuggestionController` and the `AutofillSuggestionDelegate` to
// differentiate between different actions if a single `Suggestion` has more
// than one such action.
//
// Example:
// If a `Suggestion` wants to show downvote and upvote buttons, the handling
// logic (e.g. inside `AutofillExternalDelegate`) needs to be able to
// differentiate between the buttons that were clicked. To do that, one would do
// the following:
// - Define an action type, e.g.
//   enum class kMySuggestionButtonAction { kUpvote, kDownvote };
// - Add the type as a variant to `SuggestionButtonAction`.
using SuggestionButtonAction =
    absl::variant<absl::monostate, PredictionImprovementsButtonActions>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_BUTTON_ACTION_H_
