// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"

#include <vector>

namespace autofill {

MockAutofillCrowdsourcingManager::MockAutofillCrowdsourcingManager(
    AutofillClient* client,
    LogManager* log_manager)
    : AutofillCrowdsourcingManager(client,
                                   /*api_key=*/"") {
  ON_CALL(*this, StartQueryRequest)
      .WillByDefault(
          [](const auto&, const auto&,
             base::OnceCallback<void(std::optional<QueryResponse>)> callback) {
            std::move(callback).Run(std::nullopt);
            return false;
          });
}

MockAutofillCrowdsourcingManager::~MockAutofillCrowdsourcingManager() = default;

}  // namespace autofill
