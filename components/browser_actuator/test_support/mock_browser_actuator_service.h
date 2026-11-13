// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_BROWSER_ACTUATOR_SERVICE_H_
#define COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_BROWSER_ACTUATOR_SERVICE_H_

#include <memory>

#include "components/browser_actuator/public/browser_actuator_service.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_actuator {

class MockBrowserActuatorService : public BrowserActuatorService {
 public:
  MockBrowserActuatorService();
  ~MockBrowserActuatorService() override;

  MOCK_METHOD(bool, IsInitialized, (), (const, override));
  MOCK_METHOD(std::unique_ptr<BrowserActuatorTaskClient>,
              CreateTaskClient,
              (),
              (override));
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_TEST_SUPPORT_MOCK_BROWSER_ACTUATOR_SERVICE_H_
