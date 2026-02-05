// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_SYNC_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_SYNC_TEST_UTILS_H_

#include <string_view>

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"

namespace autofill {

// Creates a test `LoyaltyCard`.
LoyaltyCard TestLoyaltyCard(std::string_view id = "1");

// Creates a test `AutofillValuableSpecifics`.
sync_pb::AutofillValuableSpecifics TestLoyaltyCardSpecifics(
    std::string_view id = "1",
    std::string_view program_logo = "http://foobar.com/logo.png",
    std::string_view number = "80974934820245");

// Creates a test `ValuableMetadata`.
ValuableMetadata TestValuableMetadata(std::string_view id = "1");

// Creates a test `AutofillValuableMetadataSpecifics`.
sync_pb::AutofillValuableMetadataSpecifics TestValuableMetadataSpecifics(
    std::string_view id = "1",
    sync_pb::AutofillValuableMetadataSpecifics::PassType pass_type =
        sync_pb::AutofillValuableMetadataSpecifics::LOYALTY_CARD);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_SYNC_TEST_UTILS_H_
