// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_TEST_UTIL_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/sync/protocol/sync.pb.h"

namespace autofill {

AutofillProfile CreateServerProfile(const std::string& server_id);

CreditCard CreateServerCreditCard(const std::string& server_id);

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForAddress(
    const std::string& specifics_id);

sync_pb::AutofillWalletSpecifics CreateAutofillWalletSpecificsForCard(
    const std::string& specifics_id,
    const std::string& billing_address_id = "");

sync_pb::AutofillWalletSpecifics
CreateAutofillWalletSpecificsForPaymentsCustomerData(
    const std::string& specifics_id);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_BRIDGE_TEST_UTIL_H_
