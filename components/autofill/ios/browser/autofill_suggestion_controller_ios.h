// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_SUGGESTION_CONTROLLER_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_SUGGESTION_CONTROLLER_IOS_H_

#import <optional>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/filling/filling_product.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#import "components/autofill/core/common/aliases.h"
#import "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillSuggestionDelegate;

// Generates a new unique session id for suggestion UI.
AutofillClient::SuggestionUiSessionId GenerateSuggestionUiSessionId();

// Controls the lifecycle, session tokens, and delegate interactions for
// Autofill suggestions on iOS.
class AutofillSuggestionControllerIOS {
 public:
  using UiSessionId = AutofillClient::SuggestionUiSessionId;

  AutofillSuggestionControllerIOS();
  AutofillSuggestionControllerIOS(const AutofillSuggestionControllerIOS&) =
      delete;
  AutofillSuggestionControllerIOS& operator=(
      const AutofillSuggestionControllerIOS&) = delete;
  ~AutofillSuggestionControllerIOS();

  // Shows `suggestions` for `form_id` and `field_id` with `trigger_source`,
  // `delegate`, and `session_id`, returning `session_id`. Moves the Undo
  // suggestion, if any, to the front. Empty `suggestions` hide the UI instead.
  // Showing for another field or `delegate` closes the previous session on the
  // previous delegate first.
  UiSessionId Show(
      std::vector<Suggestion> suggestions,
      AutofillSuggestionTriggerSource trigger_source,
      base::WeakPtr<AutofillSuggestionDelegate> delegate,
      FormGlobalId form_id,
      FieldGlobalId field_id,
      UiSessionId session_id = GenerateSuggestionUiSessionId());

  // Updates currently showing suggestions for `main_filling_product`. Dismisses
  // with `kStaleData` if `main_filling_product` does not match the showing
  // product, or with `kNoSuggestions` if `suggestions` is empty.
  void UpdateSuggestions(std::vector<Suggestion> suggestions,
                         FillingProduct main_filling_product,
                         AutofillSuggestionTriggerSource trigger_source);

  // Hides the suggestion UI, cleans up session state, and notifies delegate.
  void Hide(SuggestionHidingReason reason);

  // Returns the form identifier associated with the current suggestion session.
  FormGlobalId form_id() const { return form_id_; }

  // Returns the field identifier associated with the current suggestion
  // session.
  FieldGlobalId field_id() const { return field_id_; }

  // Returns true if suggestions are currently showing.
  bool is_showing() const { return is_showing_; }

  // Returns true if the currently showing suggestions belong to any field (or
  // its owning form, for cross-frame forms) in `frame_token`.
  bool IsShowingForFrame(LocalFrameToken frame_token) const;

  // Returns the currently showing suggestions.
  const std::vector<Suggestion>& suggestions() const { return suggestions_; }

  // Returns the main filling product of the current suggestions.
  FillingProduct GetMainFillingProduct() const;

  // Returns the session id if suggestions are showing, or `std::nullopt`
  // otherwise.
  std::optional<UiSessionId> GetUiSessionId() const;

  // Returns the active suggestion delegate.
  base::WeakPtr<AutofillSuggestionDelegate> GetDelegate() const;

 private:
  // Stores and orders `suggestions`, updates the filling product, and notifies
  // `delegate_` that suggestions are shown.
  void SetSuggestionsAndNotifyDelegate(
      std::vector<Suggestion> suggestions,
      AutofillSuggestionTriggerSource trigger_source);

  // Updates the filling product based on `delegate_` or `suggestions_`.
  void UpdateFillingProduct();

  // Moves the Undo suggestion, if any, to the front of `suggestions_`.
  void OrderSuggestions();

  // Currently showing suggestions.
  std::vector<Suggestion> suggestions_;

  // Current session id.
  std::optional<UiSessionId> ui_session_id_;

  // Weak reference to the suggestion delegate.
  base::WeakPtr<AutofillSuggestionDelegate> delegate_;

  // Form and field associated with the current suggestion session.
  FormGlobalId form_id_;
  FieldGlobalId field_id_;

  // Trigger source for current suggestions.
  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;

  // Main filling product for the suggestions.
  FillingProduct suggestions_filling_product_ = FillingProduct::kNone;

  // True if suggestions UI is currently showing.
  bool is_showing_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_SUGGESTION_CONTROLLER_IOS_H_
