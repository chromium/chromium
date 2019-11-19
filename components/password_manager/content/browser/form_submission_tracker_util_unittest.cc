// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "components/password_manager/core/browser/form_submission_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class FormSubmissionObserverMock : public FormSubmissionObserver {
 public:
  MOCK_METHOD1(DidNavigateMainFrame, void(bool form_may_be_submitted));
};

class FormSubmissionTrackerUtilTest
    : public content::RenderViewHostTestHarness {
 public:
  FormSubmissionTrackerUtilTest() = default;
  ~FormSubmissionTrackerUtilTest() override = default;

  FormSubmissionObserverMock& observer() { return observer_; }

 private:
  FormSubmissionObserverMock observer_;

  DISALLOW_COPY_AND_ASSIGN(FormSubmissionTrackerUtilTest);
};

TEST_F(FormSubmissionTrackerUtilTest, NotRendererInitiated) {
  EXPECT_CALL(observer(),
              DidNavigateMainFrame(false /* form_may_be_submitted */));
  NotifyDidNavigateMainFrame(
      false /* is_renderer_initiated */, ui::PAGE_TRANSITION_RELOAD,
      true /* was_initiated_by_link_click */, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, LinkTransition) {
  EXPECT_CALL(observer(),
              DidNavigateMainFrame(false /* form_may_be_submitted */));
  NotifyDidNavigateMainFrame(
      true /* is_renderer_initiated */, ui::PAGE_TRANSITION_LINK,
      true /* was_initiated_by_link_click */, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, FormSubmission) {
  EXPECT_CALL(observer(),
              DidNavigateMainFrame(true /* form_may_be_submitted */));
  NotifyDidNavigateMainFrame(
      true /* is_renderer_initiated */, ui::PAGE_TRANSITION_FORM_SUBMIT,
      true /* was_initiated_by_link_click */, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, PageRedirectAfterJavaScriptSubmission) {
  EXPECT_CALL(observer(),
              DidNavigateMainFrame(true /* form_may_be_submitted */));
  NotifyDidNavigateMainFrame(
      true /* is_renderer_initiated */, ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      false /* was_initiated_by_link_click */, &observer());
}

}  // namespace
}  // namespace password_manager
