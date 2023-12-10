// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"

#include <vector>

namespace autofill {

MockAutofillCrowdsourcingManager::MockAutofillCrowdsourcingManager(
    AutofillClient* client)
    : AutofillCrowdsourcingManager(client,
                                   /*api_key=*/"",
                                   /*log_manager=*/nullptr) {}

MockAutofillCrowdsourcingManager::~MockAutofillCrowdsourcingManager() = default;

}  // namespace autofill
