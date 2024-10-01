// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {

struct Suggestion;

// The interface for communication from //components/autofill to
// //components/plus_addresses.
//
// In general, plus addresses uses Autofill as a platform/API: Plus addresses is
// informed about certain renderer events (e.g. user focus on an appropriate
// textfield) and may choose to trigger Autofill to field the field. Therefore
// //components/plus_addresses should depend on //components/autofill. To still
// allow communication from //components/autofill to
// //components/plus_addresses, this interface exists and is injected via
// `AutofillClient`.
class AutofillPlusAddressDelegate {
 public:
  // Describes interactions with Autofill suggestions for plus addresses.
  // The values are persisted to metrics, do not change them.
  enum class SuggestionEvent {
    // Suggestion shown events.
    kExistingPlusAddressSuggested = 0,
    kCreateNewPlusAddressSuggested = 1,
    kCreateNewPlusAddressInlineSuggested = 4,
    kErrorDuringReserve = 8,

    // Suggestion accepted events.
    kExistingPlusAddressChosen = 2,
    kCreateNewPlusAddressChosen = 3,
    kCreateNewPlusAddressInlineChosen = 5,

    // Other events.
    // The other clicked the refresh button on an inline creation suggestion.
    kRefreshPlusAddressInlineClicked = 6,
    // A loading state for the suggested address was shown because none was
    // available synchronously.
    kCreateNewPlusAddressInlineReserveLoadingStateShown = 7,

    kMaxValue = kErrorDuringReserve,
  };

  virtual ~AutofillPlusAddressDelegate() = default;

  // Checks whether `potential_plus_address` is a known plus address.
  virtual bool IsPlusAddress(
      const std::string& potential_plus_address) const = 0;

  // Returns whether plus address filling is supported for the given `origin`.
  // This is true iff:
  // - the `PlusAddressService` is enabled and
  // - `origin` is not a blocked origin.
  virtual bool IsPlusAddressFillingEnabled(const url::Origin& origin) const = 0;

  // Returns whether plus address full form filling is supported.
  virtual bool IsPlusAddressFullFormFillingEnabled() const = 0;

  // Returns a list of plus addresses for the `origin` and all affiliated
  // domains.
  virtual void GetAffiliatedPlusAddresses(
      const url::Origin& origin,
      base::OnceCallback<void(std::vector<std::string>)> callback) = 0;

  // Returns the suggestions to show for the given list of
  // `plus_addresses`, `origin` and the `focused_field`. If
  // `trigger_source` indicates that this is a manual fallback (e.g. the
  // suggestions were triggered from the context menu on Desktop), then
  // `focused_field` is ignored. Otherwise, only suggestions whose prefix
  // matches `focused_field` are shown.
  virtual std::vector<Suggestion> GetSuggestionsFromPlusAddresses(
      const std::vector<std::string>& plus_addresses,
      const url::Origin& origin,
      bool is_off_the_record,
      const PasswordFormClassification& focused_form_classification,
      const FormFieldData& focused_field,
      AutofillSuggestionTriggerSource trigger_source) = 0;

  // Returns the "Manage plus addresses..." suggestion which redirects the user
  // to the plus address management page.
  virtual Suggestion GetManagePlusAddressSuggestion() const = 0;

  // Logs Autofill suggestion events related to plus addresses.
  virtual void RecordAutofillSuggestionEvent(
      SuggestionEvent suggestion_event) = 0;

  // An enum describing the context in which a plus address suggestion was
  // shown. These values are persisted to logs - do not modify or remove them.
  enum class SuggestionContext {
    // The plus address suggestion was shown alongside Autofill profile
    // suggestions because the user focused on a field classified as an email
    // field.
    kAutofillProfileOnEmailField = 0,
    // The plus address suggestion was not shown explicitly, but the user
    // performed Autofill profile filling on a domain for which they already had
    // a plus address.
    kAutofillProfileOnOtherField = 1,
    // The plus address suggestion was shown alongside Autocomplete suggestions.
    kAutocomplete = 2,
    // The plus address suggestion was shown because the user entered via manual
    // fallback.
    kManualFallback = 3,
    kMaxValue = kManualFallback
  };
  // Starts a session for logging a form submission UKM specific to plus
  // addresses. `suggestion_type` is the type of the first shown plus address
  // suggestion.
  // TODO(crbug.com/362445807): Investigate whether this can be moved into AED
  // as well and be combined with OnShowedInlineSuggestion.
  virtual void OnPlusAddressSuggestionShown(
      AutofillManager& manager,
      FormGlobalId form,
      FieldGlobalId field,
      SuggestionContext suggestion_context,
      PasswordFormClassification::Type form_type,
      SuggestionType suggestion_type) = 0;

  using UpdateSuggestionsCallback =
      base::OnceCallback<void(std::vector<Suggestion>,
                              AutofillSuggestionTriggerSource)>;

  // Calls `update_suggestions_callback` with updated suggestions. The updated
  // suggestions may either contain a "loading new proposed plus address"
  // suggestion, or the new proposed plus address if one is cached.
  virtual void OnClickedRefreshInlineSuggestion(
      const url::Origin& last_committed_primary_main_frame_origin,
      base::span<const Suggestion> current_suggestions,
      size_t current_suggestion_index,
      UpdateSuggestionsCallback update_suggestions_callback) = 0;

  // Checks whether any of the suggestions still require a suggested plus
  // address and, if so, trigger a network request for one. On completion of
  // that request, it runs `update_suggestions_callback`.
  virtual void OnShowedInlineSuggestion(
      const url::Origin& primary_main_frame_origin,
      base::span<const Suggestion> current_suggestions,
      UpdateSuggestionsCallback update_suggestions_callback) = 0;

  using HideSuggestionsCallback =
      base::OnceCallback<void(SuggestionHidingReason)>;
  using PlusAddressErrorDialogType = AutofillClient::PlusAddressErrorDialogType;
  using ShowErrorDialogCallback =
      base::OnceCallback<void(PlusAddressErrorDialogType, base::OnceClosure)>;
  // A callback to inform the user that there is an affiliated domain (first
  // parameter) with an existing plus address (second parameter).
  using ShowAffiliationErrorDialogCallback =
      base::OnceCallback<void(std::u16string, std::u16string)>;
  // Attempts to create the plus address in
  // `current_suggestions[current_suggestion_index]` for
  // `primary_main_frame_origin`.
  virtual void OnAcceptedInlineSuggestion(
      const url::Origin& primary_main_frame_origin,
      base::span<const Suggestion> current_suggestions,
      size_t current_suggestion_index,
      UpdateSuggestionsCallback update_suggestions_callback,
      HideSuggestionsCallback hide_suggestions_callback,
      PlusAddressCallback fill_field_callback,
      ShowAffiliationErrorDialogCallback show_affiliation_error_dialog,
      ShowErrorDialogCallback show_error_dialog,
      base::OnceClosure reshow_suggestions) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
