// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "base/test/task_environment.h"
#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"
#include "chromeos/components/bloom/screenshot_grabber.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace bloom {

namespace {

using ::testing::AnyNumber;
using ::testing::NiceMock;

const char kEmail[] = "test@gmail.com";

class ScreenshotGrabberMock : public ScreenshotGrabber {
 public:
  ScreenshotGrabberMock() {
    ON_CALL(*this, TakeScreenshot).WillByDefault([this](Callback callback) {
      // Store the callback passed to TakeScreenshot() so we can invoke it
      // later from our tests.
      this->callback_ = std::move(callback);
    });
  }
  ~ScreenshotGrabberMock() override = default;

  MOCK_METHOD(void, TakeScreenshot, (Callback callback));

  // Sends the given screenshot to the callback passed to TakeScreenshot()
  void SendScreenshot(
      const base::Optional<Screenshot>& screenshot = Screenshot()) {
    EXPECT_TRUE(callback_) << "TakeScreenshot() was never called.";

    if (callback_)
      std::move(callback_).Run(screenshot);
  }

  // Signal that taking the screenshot has failed.
  void SendScreenshotFailed() { SendScreenshot(base::nullopt); }

 private:
  Callback callback_;
};

class AssistantInteractionControllerMock
    : public ash::AssistantInteractionController {
 public:
  MOCK_METHOD(const ash::AssistantInteractionModel*, GetModel, (), (const));
  MOCK_METHOD(base::TimeDelta, GetTimeDeltaSinceLastInteraction, (), (const));
  MOCK_METHOD(bool, HasHadInteraction, (), (const));
  MOCK_METHOD(void,
              StartTextInteraction,
              (const std::string& query,
               bool allow_tts,
               chromeos::assistant::AssistantQuerySource source));
  MOCK_METHOD(void, StartBloomInteraction, ());
};

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0)

}  // namespace

class BloomControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail);
  }

 protected:
  // Returns the |ScopeSet| that was passed to the access token request.
  // Fails the test and returns an empty |ScopeSet| if there were no access
  // token requests.
  signin::ScopeSet GetRequestedAccessTokenScopes() {
    std::vector<signin::IdentityTestEnvironment::PendingRequest> requests =
        identity_test_env_.GetPendingAccessTokenRequests();
    EXPECT_EQ(requests.size(), 1u);

    if (requests.size())
      return requests.begin()->scopes;
    else
      return signin::ScopeSet();
  }

  BloomControllerImpl& controller() { return controller_; }

  ScreenshotGrabberMock& screenshot_grabber_mock() {
    return *static_cast<ScreenshotGrabberMock*>(
        controller_.screenshot_grabber());
  }

  AssistantInteractionControllerMock& assistant_interaction_controller_mock() {
    return assistant_interaction_controller_mock_;
  }

  void IssueAccessToken(std::string token = std::string("<access-token>")) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        token, /*expiration=*/base::Time::Max());
  }

  void FailAccessToken(GoogleServiceAuthError::State error =
                           GoogleServiceAuthError::CONNECTION_FAILED) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        GoogleServiceAuthError(error));
  }

  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;

  AssistantInteractionControllerMock assistant_interaction_controller_mock_;

  BloomControllerImpl controller_{
      identity_test_env_.identity_manager(),
      &assistant_interaction_controller_mock_,
      std::make_unique<NiceMock<ScreenshotGrabberMock>>()};
};

TEST_F(BloomControllerImplTest, ShouldBeReturnedWhenCallingGet) {
  EXPECT_EQ(&controller(), BloomController::Get());
}

TEST_F(BloomControllerImplTest, ShouldFetchAccessToken) {
  controller().StartInteraction();

  ASSERT_TRUE(identity_test_env_.IsAccessTokenRequestPending());
  signin::ScopeSet expected_scopes;
  expected_scopes.insert(assistant::kBloomScope);
  signin::ScopeSet actual_scopes = GetRequestedAccessTokenScopes();
  EXPECT_EQ(expected_scopes, actual_scopes);
}

TEST_F(BloomControllerImplTest, ShouldTakeScreenshot) {
  EXPECT_CALL(screenshot_grabber_mock(), TakeScreenshot);

  controller().StartInteraction();
}

TEST_F(BloomControllerImplTest,
       ShouldStartAssistantInteractionWhenAccessTokenAndScreenshotAreReady) {
  EXPECT_CALL(assistant_interaction_controller_mock(), StartBloomInteraction);

  controller().StartInteraction();
  IssueAccessToken();
  screenshot_grabber_mock().SendScreenshot();
}

TEST_F(BloomControllerImplTest,
       ShouldNotStartAssistantInteractionBeforeAccessTokenArrives) {
  EXPECT_NO_CALLS(assistant_interaction_controller_mock(),
                  StartBloomInteraction);

  controller().StartInteraction();
  screenshot_grabber_mock().SendScreenshot();
}

TEST_F(BloomControllerImplTest,
       ShouldNotStartAssistantInteractionBeforeScreenshotArrives) {
  EXPECT_NO_CALLS(assistant_interaction_controller_mock(),
                  StartBloomInteraction);

  controller().StartInteraction();
  IssueAccessToken();
}

TEST_F(BloomControllerImplTest, ShouldAbortWhenFetchingAccessTokenFails) {
  EXPECT_NO_CALLS(assistant_interaction_controller_mock(),
                  StartBloomInteraction);

  controller().StartInteraction();
  screenshot_grabber_mock().SendScreenshot();

  FailAccessToken();

  EXPECT_FALSE(controller().HasInteraction());
  EXPECT_EQ(BloomInteractionResolution::kNoAccessToken,
            controller().GetLastInteractionResolution());
}

TEST_F(BloomControllerImplTest, ShouldAbortWhenFetchingScreenshotFails) {
  EXPECT_NO_CALLS(assistant_interaction_controller_mock(),
                  StartBloomInteraction);

  controller().StartInteraction();
  IssueAccessToken();

  screenshot_grabber_mock().SendScreenshotFailed();

  EXPECT_FALSE(controller().HasInteraction());
  EXPECT_EQ(BloomInteractionResolution::kNoScreenshot,
            controller().GetLastInteractionResolution());
}

}  // namespace bloom
}  // namespace chromeos
