// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "base/test/task_environment.h"
#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"
#include "chromeos/components/bloom/public/cpp/bloom_screenshot_delegate.h"
#include "chromeos/components/bloom/public/cpp/bloom_ui_delegate.h"
#include "chromeos/components/bloom/server/bloom_server_proxy.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace chromeos {
namespace bloom {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

const char kEmail[] = "test@gmail.com";

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0)

class ScreenshotDelegateMock : public BloomScreenshotDelegate {
 public:
  ScreenshotDelegateMock() {
    ON_CALL(*this, TakeScreenshot).WillByDefault([this](Callback callback) {
      // Store the callback passed to TakeScreenshot() so we can invoke it
      // later from our tests.
      this->callback_ = std::move(callback);
    });
  }
  ~ScreenshotDelegateMock() override = default;

  MOCK_METHOD(void, TakeScreenshot, (Callback callback));

  // Sends the given screenshot to the callback passed to TakeScreenshot()
  void SendScreenshot(
      const base::Optional<gfx::Image>& screenshot = gfx::Image()) {
    EXPECT_TRUE(callback_) << "TakeScreenshot() was never called.";

    if (callback_)
      std::move(callback_).Run(screenshot);
  }

  // Signal that taking the screenshot has failed.
  void SendScreenshotFailed() { SendScreenshot(base::nullopt); }

 private:
  Callback callback_;
};

class BloomUiDelegateMock : public BloomUiDelegate {
 public:
  MOCK_METHOD(void, OnInteractionStarted, ());
  MOCK_METHOD(void, OnShowUI, ());
  MOCK_METHOD(void, OnShowResult, (const std::string& html));
  MOCK_METHOD(void,
              OnInteractionFinished,
              (BloomInteractionResolution resolution));
};

// Helper class that will track the state of the Bloom interaction,
// So we can easily test if interactions are started/stopped correctly.
class BloomInteractionTracker : public BloomUiDelegate {
 public:
  void OnInteractionStarted() override {
    EXPECT_FALSE(has_interaction_);
    has_interaction_ = true;
  }

  void OnShowUI() override { EXPECT_TRUE(has_interaction_); }

  void OnShowResult(const std::string& html) override {
    EXPECT_TRUE(has_interaction_);
  }

  void OnInteractionFinished(BloomInteractionResolution resolution) override {
    EXPECT_TRUE(has_interaction_);
    has_interaction_ = false;
    last_resolution_ = resolution;
  }

  bool HasInteraction() const { return has_interaction_; }
  BloomInteractionResolution GetLastInteractionResolution() const {
    return last_resolution_;
  }

 private:
  bool has_interaction_ = false;
  BloomInteractionResolution last_resolution_ =
      BloomInteractionResolution::kNormal;
};

class BloomServerProxyMock : public BloomServerProxy {
 public:
  BloomServerProxyMock() {
    ON_CALL(*this, AnalyzeProblem)
        .WillByDefault([this](const std::string& access_token,
                              const gfx::Image& screenshot, Callback callback) {
          // Store the callback passed to AnalyzeProblem() so we can invoke it
          // later from our tests.
          this->callback_ = std::move(callback);
        });
  }
  MOCK_METHOD(void,
              AnalyzeProblem,
              (const std::string& access_token,
               const gfx::Image& screenshot,
               Callback callback));

  void SendResponse(base::Optional<std::string> html = std::string("<html/>")) {
    EXPECT_TRUE(callback_) << "AnalyzeProblem() was never called.";

    if (callback_)
      std::move(callback_).Run(html);
  }

  void SendResponseFailure() { SendResponse(base::nullopt); }

 private:
  Callback callback_;
};

}  // namespace

void PrintTo(const BloomInteractionResolution& value, std::ostream* output) {
#define CASE(name)                         \
  ({                                       \
    case BloomInteractionResolution::name: \
      *output << #name;                    \
      break;                               \
  })

  switch (value) {
    CASE(kNormal);
    CASE(kNoScreenshot);
    CASE(kNoAccessToken);
    CASE(kServerError);
  }
}

class BloomControllerImplTest : public testing::Test {
 public:
  void SetUp() override {
    controller_.SetScreenshotDelegate(
        std::make_unique<NiceMock<ScreenshotDelegateMock>>());
    controller_.SetUiDelegate(
        std::make_unique<NiceMock<BloomUiDelegateMock>>());

    identity_test_env_.MakePrimaryAccountAvailable(kEmail);
  }

 protected:
  void StartInteractionAndSendAccessTokenAndScreenshot(
      std::string access_token = "<access-token>",
      gfx::Image screenshot = gfx::Image()) {
    controller().StartInteraction();
    IssueAccessToken(access_token);
    screenshot_delegate_mock().SendScreenshot(screenshot);
  }

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

  ScreenshotDelegateMock& screenshot_delegate_mock() {
    return *static_cast<ScreenshotDelegateMock*>(
        controller_.screenshot_delegate());
  }

  BloomUiDelegateMock* AddUiDelegateMock() {
    auto delegate = std::make_unique<NiceMock<BloomUiDelegateMock>>();
    auto* raw_ptr = delegate.get();
    controller().SetUiDelegate(std::move(delegate));
    return raw_ptr;
  }

  BloomInteractionTracker* AddInteractionTracker() {
    auto delegate = std::make_unique<BloomInteractionTracker>();
    auto* raw_ptr = delegate.get();
    controller().SetUiDelegate(std::move(delegate));
    return raw_ptr;
  }

  BloomServerProxyMock& bloom_server() {
    return *static_cast<BloomServerProxyMock*>(controller_.server_proxy());
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

  BloomControllerImpl controller_{
      identity_test_env_.identity_manager(),
      std::make_unique<NiceMock<BloomServerProxyMock>>()};
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
  EXPECT_CALL(screenshot_delegate_mock(), TakeScreenshot);

  controller().StartInteraction();
}

TEST_F(BloomControllerImplTest, ShouldAbortWhenFetchingAccessTokenFails) {
  auto* interaction_tracker = AddInteractionTracker();

  controller().StartInteraction();
  screenshot_delegate_mock().SendScreenshot();

  FailAccessToken();

  EXPECT_FALSE(interaction_tracker->HasInteraction());
  EXPECT_EQ(BloomInteractionResolution::kNoAccessToken,
            interaction_tracker->GetLastInteractionResolution());
}

TEST_F(BloomControllerImplTest, ShouldAbortWhenFetchingScreenshotFails) {
  auto* interaction_tracker = AddInteractionTracker();

  controller().StartInteraction();
  IssueAccessToken();

  screenshot_delegate_mock().SendScreenshotFailed();

  EXPECT_FALSE(interaction_tracker->HasInteraction());
  EXPECT_EQ(BloomInteractionResolution::kNoScreenshot,
            interaction_tracker->GetLastInteractionResolution());
}

TEST_F(BloomControllerImplTest,
       ShouldPassAccessTokenAndScreenshotToBloomServer) {
  const std::string& access_token = "<the-access-token>";
  const gfx::Image screenshot = gfx::test::CreateImage(10, 20);

  EXPECT_CALL(bloom_server(), AnalyzeProblem(access_token, screenshot, _));

  StartInteractionAndSendAccessTokenAndScreenshot(access_token, screenshot);
}

TEST_F(BloomControllerImplTest, ShouldAbortWhenBloomServerFails) {
  auto* interaction_tracker = AddInteractionTracker();
  StartInteractionAndSendAccessTokenAndScreenshot();

  bloom_server().SendResponseFailure();

  EXPECT_FALSE(interaction_tracker->HasInteraction());
  EXPECT_EQ(BloomInteractionResolution::kServerError,
            interaction_tracker->GetLastInteractionResolution());
}

////////////////////////////////////////////////////////////////////////////////
///
/// Below are the tests to ensure the BloomUiDelegate is invoked.
///
////////////////////////////////////////////////////////////////////////////////

using BloomUiDelegateTest = BloomControllerImplTest;

TEST_F(BloomUiDelegateTest,
       ShouldCallOnInteractionStartedWhenBloomInteractionStarts) {
  auto* delegate = AddUiDelegateMock();
  EXPECT_CALL(*delegate, OnInteractionStarted);

  controller().StartInteraction();
}

TEST_F(BloomUiDelegateTest,
       ShouldCallOnShowUIWhenAccessTokenAndScreenshotAreReady) {
  auto* delegate = AddUiDelegateMock();

  EXPECT_CALL(*delegate, OnShowUI);

  controller().StartInteraction();
  IssueAccessToken();
  screenshot_delegate_mock().SendScreenshot();
}

TEST_F(BloomUiDelegateTest, ShouldNotCallOnShowUIBeforeAccessTokenArrives) {
  auto* delegate = AddUiDelegateMock();

  EXPECT_NO_CALLS(*delegate, OnShowUI);

  controller().StartInteraction();
  screenshot_delegate_mock().SendScreenshot();
}

TEST_F(BloomUiDelegateTest, ShouldNotCallOnShowUIBeforeScreenshotArrives) {
  auto* delegate = AddUiDelegateMock();

  EXPECT_NO_CALLS(*delegate, OnShowUI);

  controller().StartInteraction();
  IssueAccessToken();
}

TEST_F(BloomUiDelegateTest, ShouldForwardServerResponse) {
  auto* delegate = AddUiDelegateMock();
  StartInteractionAndSendAccessTokenAndScreenshot();

  EXPECT_CALL(*delegate, OnShowResult("<html>response</html>"));

  bloom_server().SendResponse("<html>response</html>");
}

}  // namespace bloom
}  // namespace chromeos
