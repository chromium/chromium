// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/navigate_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::testing::Property;
using ::testing::Return;

class NavigateActionTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &browser_context_, nullptr);
    ON_CALL(mock_action_delegate_, GetWebContents)
        .WillByDefault(Return(web_contents_.get()));
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_navigate() = proto_;
    NavigateAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  NavigateProto proto_;
};

TEST_F(NavigateActionTest, FailsForEmptyAction) {
  EXPECT_CALL(mock_action_delegate_, ExpectNavigation).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, UNSUPPORTED))));

  Run();
}

TEST_F(NavigateActionTest, LoadsSpecifiedUrl) {
  EXPECT_CALL(mock_action_delegate_, ExpectNavigation);
  EXPECT_CALL(mock_action_delegate_, LoadURL(GURL("https://navigate.com")));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  proto_.set_url("https://navigate.com");
  Run();
}

TEST_F(NavigateActionTest, NavigatesBackwardIfPossible) {
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://initial.com"));
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://navigate.com"));

  EXPECT_CALL(mock_action_delegate_, ExpectNavigation);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  EXPECT_EQ(web_contents_->GetLastCommittedURL(), GURL("https://navigate.com"));
  EXPECT_TRUE(web_contents_->GetController().CanGoBack());
  proto_.set_go_backward(true);
  Run();
  content::WebContentsTester::For(web_contents_.get())
      ->CommitPendingNavigation();
  EXPECT_EQ(web_contents_->GetLastCommittedURL(), GURL("https://initial.com"));
}

TEST_F(NavigateActionTest, FailsIfNavigatingBackwardIsNotPossible) {
  EXPECT_CALL(mock_action_delegate_, ExpectNavigation).Times(0);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  proto_.set_go_backward(true);
  Run();
}

TEST_F(NavigateActionTest, NavigatesForwardIfPossible) {
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://initial.com"));
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://navigate.com"));
  EXPECT_TRUE(web_contents_->GetController().CanGoBack());
  web_contents_->GetController().GoBack();
  content::WebContentsTester::For(web_contents_.get())
      ->CommitPendingNavigation();

  EXPECT_CALL(mock_action_delegate_, ExpectNavigation);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  EXPECT_EQ(web_contents_->GetLastCommittedURL(), GURL("https://initial.com"));
  EXPECT_TRUE(web_contents_->GetController().CanGoForward());
  proto_.set_go_forward(true);
  Run();
  content::WebContentsTester::For(web_contents_.get())
      ->CommitPendingNavigation();
  EXPECT_EQ(web_contents_->GetLastCommittedURL(), GURL("https://navigate.com"));
}

TEST_F(NavigateActionTest, FailsIfNavigatingForwardIsNotPossible) {
  EXPECT_CALL(mock_action_delegate_, ExpectNavigation).Times(0);
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PRECONDITION_FAILED))));

  proto_.set_go_forward(true);
  Run();
}

}  // namespace
}  // namespace autofill_assistant
