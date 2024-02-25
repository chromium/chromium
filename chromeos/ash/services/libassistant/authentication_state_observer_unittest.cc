// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/libassistant_service.h"
#include "chromeos/ash/services/libassistant/public/mojom/authentication_state_observer.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

class AuthenticationStateObserverMock
    : public mojom::AuthenticationStateObserver {
 public:
  AuthenticationStateObserverMock() = default;
  AuthenticationStateObserverMock(const AuthenticationStateObserverMock&) =
      delete;
  AuthenticationStateObserverMock& operator=(
      const AuthenticationStateObserverMock&) = delete;
  ~AuthenticationStateObserverMock() override = default;

  // mojom::AuthenticationStateObserver implementation:
  MOCK_METHOD(void, OnAuthenticationError, ());

  mojo::PendingRemote<mojom::AuthenticationStateObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::AuthenticationStateObserver> receiver_{this};
};

}  // namespace

class AuthenticationStateObserverTest : public ::testing::Test {
 public:
  AuthenticationStateObserverTest() = default;
  AuthenticationStateObserverTest(const AuthenticationStateObserverTest&) =
      delete;
  AuthenticationStateObserverTest& operator=(
      const AuthenticationStateObserverTest&) = delete;
  ~AuthenticationStateObserverTest() override = default;

  void SetUp() override {
    service_tester_.service().AddAuthenticationStateObserver(
        observer_mock_.BindNewPipeAndPassRemote());

    service_tester_.Start();

    service_tester_.service()
        .conversation_controller()
        .OnAssistantClientRunning(&service_tester_.assistant_client());
  }

  AuthenticationStateObserverMock& observer_mock() { return observer_mock_; }

  void FlushMojomPipes() { service_tester_.FlushForTesting(); }

  void OnCommunicationError() {
    ::assistant::api::OnDeviceStateEventRequest request;
    auto* communication_error =
        request.mutable_event()->mutable_on_communication_error();
    communication_error->set_error_code(
        ::assistant::api::events::DeviceStateEvent::OnCommunicationError::
            AUTH_TOKEN_FAIL);

    service_tester_.service().conversation_controller().OnGrpcMessageForTesting(
        request);
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<AuthenticationStateObserverMock> observer_mock_;
  LibassistantServiceTester service_tester_;
};

TEST_F(AuthenticationStateObserverTest, ShouldReportAuthenticationErrors) {
  EXPECT_CALL(observer_mock(), OnAuthenticationError());
  OnCommunicationError();

  FlushMojomPipes();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_mock()))
      << "Failure to receive Auth error.";
}

}  // namespace ash::libassistant
