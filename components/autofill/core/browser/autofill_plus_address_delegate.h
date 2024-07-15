// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/autofill_client.h"
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
  // Callback to return the list of plus address suggestions.
  using GetSuggestionsCallback =
      base::OnceCallback<void(std::vector<Suggestion>)>;
  // Describes interactions with Autofill suggestions for plus addresses.
  // The values are persisted to metrics, do not change them.
  enum class SuggestionEvent {
    kExistingPlusAddressSuggested = 0,
    kCreateNewPlusAddressSuggested = 1,
    kExistingPlusAddressChosen = 2,
    kCreateNewPlusAddressChosen = 3,
    kMaxValue = kCreateNewPlusAddressChosen,
  };

  virtual ~AutofillPlusAddressDelegate() = default;

  // Checks whether `potential_plus_address` is a known plus address.
  virtual bool IsPlusAddress(
      const std::string& potential_plus_address) const = 0;

  // Returns the suggestions to show for the given origin and
  // `focused_field_value`. If `trigger_source` indicates that this is a manual
  // fallback (e.g. the suggestions were triggered from the context menu on
  // Desktop), then `focused_field_value` is ignored. Otherwise, only
  // suggestions whose prefix matches `focused_field_value` are shown.
  virtual void GetSuggestions(
      const url::Origin& last_committed_primary_main_frame_origin,
      bool is_off_the_record,
      AutofillClient::PasswordFormType focused_form_type,
      std::u16string_view focused_field_value,
      AutofillSuggestionTriggerSource trigger_source,
      GetSuggestionsCallback callback) = 0;

  // Returns the "Manage plus addresses..." suggestion which redirects the user
  // to the plus address management page.
  virtual Suggestion GetManagePlusAddressSuggestion() const = 0;

  // Returns whether plus address suggestions should be mixed with single field
  // form fill suggestions instead of override them.
  // TODO(crbug.com/324557560): Remove once feature flag is not needed.
  virtual bool ShouldMixWithSingleFieldFormFillSuggestions() const = 0;

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
  virtual void OnPlusAddressSuggestionShown(
      AutofillManager& manager,
      FormGlobalId form,
      FieldGlobalId field,
      SuggestionContext suggestion_context,
      AutofillClient::PasswordFormType form_type,
      SuggestionType suggestion_type) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
