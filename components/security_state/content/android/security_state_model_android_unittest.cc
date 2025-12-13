// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "components/security_state/content/android/security_state_client.h"
#include "components/security_state/content/android/security_state_model_delegate.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// The actual JNI call is just a wrapper around this function.
namespace security_state::internal {
security_state::SecurityLevel GetSecurityLevelForWebContentsInternal(
    content::WebContents* web_contents,
    SecurityStateModelDelegate* delegate);
security_state::MaliciousContentStatus
GetMaliciousContentStatusForWebContentsInternal(
    content::WebContents* web_contents,
    SecurityStateModelDelegate* delegate);
SecurityStateModelDelegate* CreateSecurityStateModelDelegate();
}  // namespace security_state::internal

using security_state::MaliciousContentStatus;
using security_state::SecurityLevel;
using security_state::SecurityStateClient;
using security_state::SetSecurityStateClient;
using security_state::internal::CreateSecurityStateModelDelegate;
using security_state::internal::GetMaliciousContentStatusForWebContentsInternal;
using security_state::internal::GetSecurityLevelForWebContentsInternal;
using testing::AtMost;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

// Mock implementation of SecurityStateModelDelegate.
class MockSecurityStateModelDelegate : public SecurityStateModelDelegate {
 public:
  MOCK_METHOD(MaliciousContentStatus,
              GetMaliciousContentStatus,
              (content::WebContents * web_contents),
              (const override));
  MOCK_METHOD(SecurityLevel,
              GetSecurityLevel,
              (content::WebContents * web_contents),
              (const override));
};

// Mock implementation of SecurityStateClient.
class MockSecurityStateClient : public SecurityStateClient {
 public:
  MOCK_METHOD(std::unique_ptr<SecurityStateModelDelegate>,
              MaybeCreateSecurityStateModelDelegate,
              (),
              (override));
};

// Base Test Fixture for MaliciousContentStatusBridge tests.
class SecurityStateBridgeTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  std::unique_ptr<StrictMock<MockSecurityStateClient>> mock_client_;
  std::unique_ptr<StrictMock<MockSecurityStateModelDelegate>>
      mock_delegate_ptr_;

  content::WebContents* CreateWebContents() {
    return web_contents_factory_.CreateWebContents(&context_);
  }

 public:
  void SetUp() override {
    mock_client_ = std::make_unique<StrictMock<MockSecurityStateClient>>();
    SetSecurityStateClient(mock_client_.get());
    mock_delegate_ptr_ =
        std::make_unique<StrictMock<MockSecurityStateModelDelegate>>();
  }
};

TEST_F(SecurityStateBridgeTest,
       GetMaliciousContentStatusReturnsNoneIfWebContentsIsNull) {
  // Subsequent calls use the already initialized delegate.
  EXPECT_CALL(*mock_delegate_ptr_, GetMaliciousContentStatus(Eq(nullptr)))
      .Times(0);

  EXPECT_EQ(MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_NONE,
            GetMaliciousContentStatusForWebContentsInternal(
                nullptr, mock_delegate_ptr_.get()));
}

TEST_F(SecurityStateBridgeTest,
       GetMaliciousContentStatusReturnsNoneIfDelegateIsNull) {
  content::WebContents* web_contents = CreateWebContents();

  EXPECT_EQ(
      MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_NONE,
      GetMaliciousContentStatusForWebContentsInternal(web_contents, nullptr));
}

// Parameterized Test Fixture inheriting from the SecurityStateBridgeTest
// fixture.
class GetSecurityLevelParamTest
    : public SecurityStateBridgeTest,
      public ::testing::WithParamInterface<SecurityLevel> {};

// The parameterized test body.
TEST_P(GetSecurityLevelParamTest, ReturnsCorrectStatus) {
  SecurityLevel expected_level = GetParam();
  content::WebContents* web_contents = CreateWebContents();

  EXPECT_CALL(*mock_delegate_ptr_, GetSecurityLevel(Eq(web_contents)))
      .WillOnce(Return(expected_level));

  EXPECT_EQ(expected_level, GetSecurityLevelForWebContentsInternal(
                                web_contents, mock_delegate_ptr_.get()));
}

INSTANTIATE_TEST_SUITE_P(SecurityLevelTest,
                         GetSecurityLevelParamTest,
                         ::testing::Values(SecurityLevel::NONE,
                                           SecurityLevel::SECURE,
                                           SecurityLevel::DANGEROUS,
                                           SecurityLevel::WARNING));

// Parameterized Test Fixture inheriting from the SecurityStateBridgeTest
// fixture.
class GetMaliciousContentStatusParamTest
    : public SecurityStateBridgeTest,
      public ::testing::WithParamInterface<MaliciousContentStatus> {};

// The parameterized test body.
TEST_P(GetMaliciousContentStatusParamTest, ReturnsCorrectStatus) {
  MaliciousContentStatus expected_status = GetParam();
  content::WebContents* web_contents = CreateWebContents();

  EXPECT_CALL(*mock_delegate_ptr_, GetMaliciousContentStatus(Eq(web_contents)))
      .WillOnce(Return(expected_status));

  EXPECT_EQ(expected_status, GetMaliciousContentStatusForWebContentsInternal(
                                 web_contents, mock_delegate_ptr_.get()));
}

INSTANTIATE_TEST_SUITE_P(
    MaliciousContentStatusTests,
    GetMaliciousContentStatusParamTest,
    ::testing::Values(
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_NONE,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_MALWARE,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE,
        MaliciousContentStatus::
            MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
        MaliciousContentStatus::
            MALICIOUS_CONTENT_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE,
        MaliciousContentStatus::
            MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_BILLING,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN,
        MaliciousContentStatus::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK));

using CreateSecurityStateModelDelegateTest = SecurityStateBridgeTest;

TEST_F(CreateSecurityStateModelDelegateTest, ReturnsNullWhenClientIsNull) {
  SetSecurityStateClient(nullptr);
  EXPECT_THAT(CreateSecurityStateModelDelegate(), IsNull());
}

TEST_F(CreateSecurityStateModelDelegateTest,
       ReturnsNullWhenMaybeCreateReturnsNull) {
  // The mock_client_ is set up in SecurityStateBridgeTest::SetUp.
  // Expect MaybeCreateSecurityStateModelDelegate to be called and return a
  // nullptr unique_ptr.
  EXPECT_CALL(*mock_client_, MaybeCreateSecurityStateModelDelegate())
      .WillOnce(Return(ByMove(nullptr)));

  EXPECT_THAT(CreateSecurityStateModelDelegate(), IsNull());
}

TEST_F(CreateSecurityStateModelDelegateTest, ReturnsValidDelegateWhenCreated) {
  auto mock_delegate =
      std::make_unique<StrictMock<MockSecurityStateModelDelegate>>();
  // Keep a raw pointer to the mock_delegate before ownership is moved.
  SecurityStateModelDelegate* expected_raw_ptr = mock_delegate.get();

  EXPECT_CALL(*mock_client_, MaybeCreateSecurityStateModelDelegate())
      .WillOnce(Return(ByMove(std::move(mock_delegate))));

  EXPECT_EQ(CreateSecurityStateModelDelegate(), expected_raw_ptr);
}

}  // namespace
