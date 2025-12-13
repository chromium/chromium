// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PLUS_ADDRESSES_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PLUS_ADDRESSES_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {

class AutofillField;
class PlusAddressSuggestionGenerator;
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

  // Checks whether `value` matches the the plus address string format.
  virtual bool MatchesPlusAddressFormat(const std::u16string& value) const = 0;

  // Returns whether plus address filling is supported for the given `origin`.
  // This is true iff:
  // - the `PlusAddressService` is enabled and
  // - `origin` is not a blocked origin.
  virtual bool IsPlusAddressFillingEnabled(const url::Origin& origin) const = 0;

  // Returns whether plus address suggestions can be shown on `field`.
  // Returns true if field's filling_product is `kAddress` or when the `field`
  // had server prediction equal to either USERNAME or SINGLE_USERNAME, and an
  // EMAIL_ADDRESS as a heuristic prediction.
  virtual bool IsFieldEligibleForPlusAddress(
      const AutofillField& field) const = 0;

  // Returns a list of plus addresses for the `origin` and all affiliated
  // domains.
  virtual void GetAffiliatedPlusAddresses(
      const url::Origin& origin,
      base::OnceCallback<void(std::vector<std::string>)> callback) = 0;

  // Returns the suggestions to show for the given list of `plus_addresses`.
  // Note that this method does not do any filtering and always returns
  // suggestions for all plus addresses in `plus_addresses`.
  // Note that this method shouldn't be called from sources other than
  // `PlusAddressSuggestionGenerator`.
  virtual std::vector<Suggestion> GetSuggestionsFromPlusAddresses(
      const std::vector<std::string>& plus_addresses) = 0;

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
  virtual void OnPlusAddressSuggestionShown(
      AutofillManager& manager,
      FormGlobalId form,
      FieldGlobalId field,
      SuggestionContext suggestion_context,
      PasswordFormClassification::Type form_type,
      SuggestionType suggestion_type) = 0;

  // Called when a plus address was filled into a web input field.
  virtual void DidFillPlusAddress() = 0;

  // Returns the number of the plus addresses created by the user for the
  // current profile.
  virtual size_t GetPlusAddressesCount() = 0;

  // Returns survey specific data for plus address HaTS surveys. Subsequent
  // calls can return different data.
  virtual std::map<std::string, std::string> GetPlusAddressHatsData() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PLUS_ADDRESSES_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
