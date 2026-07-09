// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_H_

#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace personal_context {

class MockPersonalContextEligibilityService
    : public PersonalContextEligibilityService {
 public:
  MockPersonalContextEligibilityService();
  ~MockPersonalContextEligibilityService() override;

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(PersonalContextEligibilityState,
              GetEligibilityState,
              (),
              (override));
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_H_
