// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_TEST_UTIL_H_

#include <string>

#include "components/autofill/core/browser/data_model/credit_card.h"

namespace sync_pb {
class AutofillWalletSpecifics;
}

namespace autofill {

CreditCard CreateServerCreditCard(const std::string& server_id);

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForCard(
    const std::string& client_tag,
    const std::string& billing_address_id = "",
    const std::string& nickname = "");

sync_pb::AutofillWalletSpecifics
CreateAutofillWalletSpecificsForPaymentsCustomerData(
    const std::string& client_tag);

sync_pb::AutofillWalletSpecifics
CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
    const std::string& client_tag);

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForIban(
    const std::string& client_tag);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_TEST_UTIL_H_
