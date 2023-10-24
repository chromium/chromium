// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_BULK_LEAK_CHECK_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_BULK_LEAK_CHECK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

enum class LeakDetectionInitiator;
// Mocked BulkLeakCheckService used by unit tests.
class MockBulkLeakCheckService : public BulkLeakCheckServiceInterface {
 public:
  MockBulkLeakCheckService();
  ~MockBulkLeakCheckService() override;
  MOCK_METHOD(void,
              CheckUsernamePasswordPairs,
              (LeakDetectionInitiator, std::vector<LeakCheckCredential>),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(size_t, GetPendingChecksCount, (), (const, override));
  MOCK_METHOD(State, GetState, (), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_BULK_LEAK_CHECK_SERVICE_H_
