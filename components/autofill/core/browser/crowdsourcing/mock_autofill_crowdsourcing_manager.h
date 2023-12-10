// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_MOCK_AUTOFILL_CROWDSOURCING_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_MOCK_AUTOFILL_CROWDSOURCING_MANAGER_H_

#include <vector>

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
              (const std::vector<FormStructure*>&,
               net::IsolationInfo,
               base::WeakPtr<Observer>),
              (override));

  MOCK_METHOD(bool,
              StartUploadRequest,
              (const FormStructure&,
               bool,
               const ServerFieldTypeSet&,
               const std::string&,
               bool,
               PrefService*,
               base::WeakPtr<Observer>),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_MOCK_AUTOFILL_CROWDSOURCING_MANAGER_H_
