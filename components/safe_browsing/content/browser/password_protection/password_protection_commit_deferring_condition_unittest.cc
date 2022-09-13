// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "components/safe_browsing/content/browser/password_protection/mock_password_protection_service.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

using content::CommitDeferringCondition;
using content::MockNavigationHandle;
using content::NavigationHandle;
using content::NavigationSimulator;
using content::RenderViewHostTestHarness;
using testing::NiceMock;

class PasswordProtectionCommitDeferringConditionTest
    : public RenderViewHostTestHarness {
 public:
  PasswordProtectionCommitDeferringConditionTest() = default;
  PasswordProtectionCommitDeferringConditionTest(
      const PasswordProtectionCommitDeferringConditionTest&) = delete;
  PasswordProtectionCommitDeferringConditionTest& operator=(
      const PasswordProtectionCommitDeferringConditionTest&) = delete;
  ~PasswordProtectionCommitDeferringConditionTest() override = default;

  // RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    std::vector<password_manager::MatchingReusedCredential> credentials = {
        {"http://example.test"}, {"http://2.example.com"}};

    request_ = new PasswordProtectionRequestContent(
        RenderViewHostTestHarness::web_contents(), GURL(), GURL(), GURL(),
        RenderViewHostTestHarness::web_contents()->GetContentsMimeType(),
        "username", PasswordType::PASSWORD_TYPE_UNKNOWN, credentials,
        LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
        /* password_field_exists*/ true, &service_,
        /*request_timeout_in_ms=*/0);

    condition_ = std::make_unique<PasswordProtectionCommitDeferringCondition>(
        mock_navigation_, *request_.get());
  }

  void TearDown() override {
    condition_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  scoped_refptr<PasswordProtectionRequestContent> request_;
  std::unique_ptr<PasswordProtectionCommitDeferringCondition> condition_;

  MockNavigationHandle mock_navigation_;
  NiceMock<safe_browsing::MockPasswordProtectionService> service_;
};

// If created, and while not resumed, a condition must return kDefer to defer
// the navigation until safe browsing determines it should be resumed.
TEST_F(PasswordProtectionCommitDeferringConditionTest, DeferWhenCreated) {
  EXPECT_EQ(content::CommitDeferringCondition::Result::kDefer,
            condition_->WillCommitNavigation(base::DoNothing()));
}

// If the password protection request is resolved before the navigation reaches
// commit, the condition shouldn't defer the navigation.
TEST_F(PasswordProtectionCommitDeferringConditionTest,
       ResumedBeforeCommitProceeds) {
  condition_->ResumeNavigation();

  bool was_invoked = false;
  EXPECT_EQ(CommitDeferringCondition::Result::kProceed,
            condition_->WillCommitNavigation(
                base::BindLambdaForTesting([&]() { was_invoked = true; })));
  EXPECT_FALSE(was_invoked);
}

// Calling ResumeNavigation on a deferred condition should invoke the callback.
TEST_F(PasswordProtectionCommitDeferringConditionTest,
       InvokeCallbackFromResume) {
  bool was_invoked = false;
  ASSERT_EQ(CommitDeferringCondition::Result::kDefer,
            condition_->WillCommitNavigation(
                base::BindLambdaForTesting([&]() { was_invoked = true; })));
  ASSERT_FALSE(was_invoked);

  condition_->ResumeNavigation();
  EXPECT_TRUE(was_invoked);
}

}  // namespace safe_browsing
