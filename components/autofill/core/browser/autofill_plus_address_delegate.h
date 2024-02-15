// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {

using PlusAddressCallback = base::OnceCallback<void(const std::string&)>;
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
  // `focused_field_value`.
  virtual std::vector<Suggestion> GetSuggestions(
      const url::Origin& last_committed_primary_main_frame_origin,
      bool is_off_the_record,
      std::u16string_view focused_field_value) = 0;

  // Logs Autofill suggestion events related to plus addresses.
  virtual void RecordAutofillSuggestionEvent(
      SuggestionEvent suggestion_event) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PLUS_ADDRESS_DELEGATE_H_
