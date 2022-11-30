// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_EXTERNAL_ACTION_UTIL_H_

#include "components/autofill_assistant/browser/public/external_action.pb.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {

// Creates `ProfileProto` from an `AutofillProfile`. Maps any `ServerFieldType`
// set on the `autofill_profile`.
external::ProfileProto CreateProfileProto(
    const autofill::AutofillProfile& autofill_profile);

// Creates `CreditCardProto` from a `CreditCard`. Maps any `ServerFieldType`
// set on the `credit_Card`. Also maps `record_type`, `instrument_id` and, if
// set, `network` and `server_id`.
external::CreditCardProto CreateCreditCardProto(
    const autofill::CreditCard& credit_card);

// Complete = contains name, street address, country code, zip code, email and
// phone number.
bool IsCompleteAddressProfile(const autofill::AutofillProfile* profile,
                              const std::string& app_locale);

}  // namespace autofill_assistant

#endif
