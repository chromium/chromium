// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_

#include <vector>
#include "base/callback.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/website_login_manager.h"

namespace autofill_assistant {
namespace user_data {

// Validate the completeness of a contact.
std::vector<std::string> GetContactValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options);

// Sorts the given autofill profiles based on completeness, and returns a
// vector of profile indices in sorted order. Full profiles will be ordered
// before empty ones, and for equally complete profiles, this falls back to
// sorting based on the profile names.
std::vector<int> SortContactsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Get the default selection for the current list of profiles. Returns -1 if no
// default selection is possible.
int GetDefaultContactProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

std::vector<std::string> GetShippingAddressValidationErrors(
    const autofill::AutofillProfile* profile,
    const CollectUserDataOptions& collect_user_data_options);

// Sorts the given autofill profiles based on completeness, and returns a
// vector of profile indices in sorted order. Full profiles will be ordered
// before empty ones, and for equally complete profiles, this falls back to
// sorting based on the profile names.
std::vector<int> SortShippingAddressesByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

// Get the default selection for the current list of profiles. Returns -1 if no
// default selection is possible.
int GetDefaultShippingAddressProfile(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles);

std::vector<std::string> GetPaymentInstrumentValidationErrors(
    const autofill::CreditCard* credit_card,
    const autofill::AutofillProfile* billing_address,
    const CollectUserDataOptions& collect_user_data_options);

// Sorts the given payment instruments by completeness, and returns a vector
// of payment instrument indices in sorted order. Full payment instruments will
// be ordered before empty ones, and for equally complete payment instruments,
// this falls back to sorting based on the full name on the credit card.
std::vector<int> SortPaymentInstrumentsByCompleteness(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

// Get the default selection for the current list of payment instruments.
// Returns -1 if no default selection is possible.
int GetDefaultPaymentInstrument(
    const CollectUserDataOptions& collect_user_data_options,
    const std::vector<std::unique_ptr<PaymentInstrument>>& payment_instruments);

std::unique_ptr<autofill::AutofillProfile> MakeUniqueFromProfile(
    const autofill::AutofillProfile& profile);

// Compare contact fields only. This comparison checks a subset of
// AutofillProfile::Compare. Falls back to comparing the GUIDs if nothing else
// is to be compared.
bool CompareContactDetails(
    const CollectUserDataOptions& collect_user_data_options,
    const autofill::AutofillProfile* a,
    const autofill::AutofillProfile* b);

// Get a formatted autofill value. The replacement is treated as strict,
// meaning a missing value will lead to a failed ClientStatus. If the value
// or the profile is empty, fails with |INVALID_ACTION|. If the requested
// profile does not exist, fails with |PRECONDITION FAILED|. If the value
// cannot be fully resolved, fails with |AUTOFILL_INFO_NOT_AVAILABLE|.
ClientStatus GetFormattedAutofillValue(const AutofillValue& autofill_value,
                                       const UserData* user_data,
                                       std::string* out_value);
ClientStatus GetFormattedAutofillValue(
    const AutofillValueRegexp& autofill_value_regexp,
    const UserData* user_data,
    std::string* out_value);

// Get a password manager value from the |UserData|. Returns the user name
// directly and resolves the password from the |WebsiteLoginManager|. If the
// login credentials do not exist, fails with |PRECONDITION_FAILED|. If the
// origin of the |target_element| does not match the origin of the login
// credentials, fails with |PASSWORD_ORIGIN_MISMATCH|.
void GetPasswordManagerValue(
    const PasswordManagerValue& password_manager_value,
    const ElementFinder::Result& target_element,
    const UserData* user_data,
    WebsiteLoginManager* website_login_manager,
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback);

// Retrieve a single string value stored in |UserData| under
// |client_memory_key|. If the value is not present or not a single string,
// fails with |PRECONDITION_FAILED|.
ClientStatus GetClientMemoryStringValue(const std::string& client_memory_key,
                                        const UserData* user_data,
                                        std::string* out_value);

// Take a |text_value| and resolve its content to a string. Reports the result
// through the |callback|.
void ResolveTextValue(
    const TextValue& text_value,
    const ElementFinder::Result& target_element,
    const ActionDelegate* action_delegate,
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback);

}  // namespace user_data
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_DATA_UTIL_H_
