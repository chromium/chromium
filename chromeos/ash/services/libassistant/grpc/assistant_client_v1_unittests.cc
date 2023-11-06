// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_v1.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

class AssistantManagerMock : public chromeos::assistant::FakeAssistantManager {
 public:
  AssistantManagerMock() = default;
  AssistantManagerMock(const AssistantManagerMock&) = delete;
  AssistantManagerMock& operator=(const AssistantManagerMock&) = delete;
  ~AssistantManagerMock() override = default;

  // chromeos::assistant::FakeAssistantManager implementation:
  MOCK_METHOD(void, EnableListening, (bool value));
  MOCK_METHOD(void, SetAuthTokens, (const AssistantClient::AuthTokens&));
};

class MockServicesStatusObserver : public ServicesStatusObserver {
 public:
  MockServicesStatusObserver() = default;
  MockServicesStatusObserver(const MockServicesStatusObserver&) = delete;
  MockServicesStatusObserver& operator=(const MockServicesStatusObserver&) =
      delete;
  ~MockServicesStatusObserver() override = default;

  // ServicesStatusObserver:
  MOCK_METHOD(void, OnServicesStatusChanged, (ServicesStatus status));
};

}  // namespace

class AssistantClientV1Test : public testing::Test {
 public:
  AssistantClientV1Test() = default;
  AssistantClientV1Test(const AssistantClientV1Test&) = delete;
  AssistantClientV1Test& operator=(const AssistantClientV1Test&) = delete;
  ~AssistantClientV1Test() override = default;

  void SetUp() override {
    auto assistant_manager = std::make_unique<AssistantManagerMock>();
    assistant_client_ =
        std::make_unique<AssistantClientV1>(std::move(assistant_manager));
  }

  // Start Libassistant services.
  void StartServices(ServicesStatusObserver* observer) {
    assistant_client_->StartServices(observer);
  }

  AssistantClientV1& v1_client() { return *assistant_client_; }

  AssistantManagerMock& assistant_manager_mock() {
    return *reinterpret_cast<AssistantManagerMock*>(
        assistant_client_->assistant_manager());
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  std::unique_ptr<AssistantClientV1> assistant_client_ = nullptr;
};

TEST_F(AssistantClientV1Test, ShouldNotifyServicesStarted) {
  MockServicesStatusObserver services_status_observer;
  StartServices(&services_status_observer);
}

TEST_F(AssistantClientV1Test, ShouldSetListeningEnabled) {
  MockServicesStatusObserver services_status_observer;
  StartServices(&services_status_observer);

  EXPECT_CALL(assistant_manager_mock(), EnableListening(true));

  v1_client().EnableListening(true);
}

TEST_F(AssistantClientV1Test, ShouldSetAuthenticationTokens) {
  MockServicesStatusObserver services_status_observer;
  StartServices(&services_status_observer);

  const AssistantClient::AuthTokens expected = {{"user", "token"}};
  EXPECT_CALL(assistant_manager_mock(), SetAuthTokens(expected));

  v1_client().SetAuthenticationInfo(expected);
}

}  // namespace ash::libassistant
