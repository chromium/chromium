// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/test_support/fake_browser_actuator_task_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(BrowserActuatorServiceImplTest, IsInitialized) {
  BrowserActuatorServiceImpl service;
  EXPECT_TRUE(service.IsInitialized());
}

TEST(BrowserActuatorServiceImplTest, CreateTaskClientReturnsNullByDefault) {
  BrowserActuatorServiceImpl service;
  EXPECT_EQ(service.CreateTaskClient(), nullptr);
}

TEST(BrowserActuatorServiceImplTest, SetTaskClientFactoryCreatesTaskClient) {
  BrowserActuatorServiceImpl service;
  service.SetTaskClientFactory(
      base::BindRepeating([]() -> std::unique_ptr<BrowserActuatorTaskClient> {
        return std::make_unique<FakeBrowserActuatorTaskClient>();
      }));

  std::unique_ptr<BrowserActuatorTaskClient> client1 =
      service.CreateTaskClient();
  EXPECT_NE(client1, nullptr);

  std::unique_ptr<BrowserActuatorTaskClient> client2 =
      service.CreateTaskClient();
  EXPECT_NE(client2, nullptr);
  EXPECT_NE(client1.get(), client2.get());

  service.SetTaskClientFactory(
      BrowserActuatorServiceImpl::TaskClientFactoryCallback());
  EXPECT_EQ(service.CreateTaskClient(), nullptr);
}

TEST(BrowserActuatorServiceImplTest, ShutdownResetsTaskClientFactory) {
  BrowserActuatorServiceImpl service;
  service.SetTaskClientFactory(
      base::BindRepeating([]() -> std::unique_ptr<BrowserActuatorTaskClient> {
        return std::make_unique<FakeBrowserActuatorTaskClient>();
      }));
  EXPECT_NE(service.CreateTaskClient(), nullptr);

  service.Shutdown();
  EXPECT_EQ(service.CreateTaskClient(), nullptr);
}

}  // namespace
}  // namespace browser_actuator
