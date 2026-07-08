// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_H_

#include "components/personal_context/core/personal_context_enablement_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace personal_context {

class MockPersonalContextEnablementService
    : public PersonalContextEnablementService {
 public:
  MockPersonalContextEnablementService();
  ~MockPersonalContextEnablementService() override;

  MOCK_METHOD(void, AddObserver, (Observer* observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer* observer), (override));
  MOCK_METHOD(PersonalContextEnablementState,
              GetEnablementState,
              (),
              (override));
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_H_
