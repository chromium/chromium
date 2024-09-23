// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_MOCK_AUTOFILL_CROWDSOURCING_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_MOCK_AUTOFILL_CROWDSOURCING_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

// Note that this is not a pure mock - it overrides the full
// `AutofillCrowdsourcingManager` and only mocks `StartQueryRequest` and
// `StartUploadRequest`.
class MockAutofillCrowdsourcingManager : public AutofillCrowdsourcingManager {
 public:
  explicit MockAutofillCrowdsourcingManager(AutofillClient* client);
  ~MockAutofillCrowdsourcingManager() override;

  MockAutofillCrowdsourcingManager(const MockAutofillCrowdsourcingManager&) =
      delete;
  MockAutofillCrowdsourcingManager& operator=(
      const MockAutofillCrowdsourcingManager&) = delete;

  MOCK_METHOD(bool,
              StartQueryRequest,
              ((const std::vector<raw_ptr<FormStructure, VectorExperimental>>&),
               std::optional<net::IsolationInfo>,
               base::OnceCallback<void(std::optional<QueryResponse>)>),
              (override));

  MOCK_METHOD(bool,
              StartUploadRequest,
              (std::vector<AutofillUploadContents>,
               mojom::SubmissionSource,
               bool),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_MOCK_AUTOFILL_CROWDSOURCING_MANAGER_H_
