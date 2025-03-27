// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_VALUABLES_MOCK_VALUABLE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_VALUABLES_MOCK_VALUABLE_MANAGER_H_

#include "components/autofill/core/browser/integrators/valuables/valuable_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

// Mock version of ValuableManager.
class MockValuableManager : public ValuableManager {
 public:
  MockValuableManager();
  MockValuableManager(const MockValuableManager&) = delete;
  MockValuableManager& operator=(const MockValuableManager&) = delete;
  ~MockValuableManager() override;

  MOCK_METHOD(void,
              FetchValue,
              (ValuableId,
               (base::OnceCallback<void(const std::u16string& value)>)),
              (override));
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_VALUABLES_MOCK_VALUABLE_MANAGER_H_
