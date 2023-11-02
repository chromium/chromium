// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/libassistant_service.h"
#include "chromeos/ash/services/libassistant/public/mojom/authentication_state_observer.mojom.h"
#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

// Return the list of all libassistant error codes that are considered to be
// authentication errors. This list is created on demand as there is no clear
// enum that defines these, and we don't want to hard code this list in the
// test.
std::vector<int> GetAuthenticationErrorCodes() {
  const int kMinErrorCode = chromeos::assistant::GetLowestErrorCode();
  const int kMaxErrorCode = chromeos::assistant::GetHighestErrorCode();

  std::vector<int> result;
  for (int code = kMinErrorCode; code <= kMaxErrorCode; ++code) {
    if (chromeos::assistant::IsAuthError(code))
      result.push_back(code);
  }

  return result;
}

// Return a list of some libassistant error codes that are not considered to be
// authentication errors.  Note we do not return all such codes as there are
// simply too many and testing them all significantly slows down the tests.
std::vector<int> GetNonAuthenticationErrorCodes() {
  return {-99999, 0, 1};
}

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

  assistant_client::AssistantManagerDelegate& assistant_manager_delegate() {
    return *service_tester_.assistant_manager_internal()
                .assistant_manager_delegate();
  }

  void FlushMojomPipes() { service_tester_.FlushForTesting(); }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<AuthenticationStateObserverMock> observer_mock_;
  LibassistantServiceTester service_tester_;
};

TEST_F(AuthenticationStateObserverTest, ShouldReportAuthenticationErrors) {
  for (int code : GetAuthenticationErrorCodes()) {
    EXPECT_CALL(observer_mock(), OnAuthenticationError());
    assistant_manager_delegate().OnCommunicationError(code);

    FlushMojomPipes();
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_mock()))
        << "Failure for error code " << code;
  }
}

TEST_F(AuthenticationStateObserverTest, ShouldIgnoreNonAuthenticationErrors) {
  std::vector<int> non_authentication_errors = GetNonAuthenticationErrorCodes();

  // check to ensure these are not authentication errors.
  for (int code : non_authentication_errors)
    ASSERT_FALSE(chromeos::assistant::IsAuthError(code));

  // Run the actual unittest
  for (int code : GetAuthenticationErrorCodes()) {
    EXPECT_CALL(observer_mock(), OnAuthenticationError());
    assistant_manager_delegate().OnCommunicationError(code);

    FlushMojomPipes();
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer_mock()))
        << "Failure for error code " << code;
  }
}

}  // namespace ash::libassistant
