// Copyright 2018 The Chromium Authors
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
  MOCK_METHOD(void,
              DidNavigateMainFrame,
              (bool form_may_be_submitted),
              (override));
};

class FormSubmissionTrackerUtilTest
    : public content::RenderViewHostTestHarness {
 public:
  FormSubmissionTrackerUtilTest() = default;
  FormSubmissionTrackerUtilTest(const FormSubmissionTrackerUtilTest&) = delete;
  FormSubmissionTrackerUtilTest& operator=(
      const FormSubmissionTrackerUtilTest&) = delete;
  ~FormSubmissionTrackerUtilTest() override = default;

  FormSubmissionObserverMock& observer() { return observer_; }

 private:
  FormSubmissionObserverMock observer_;
};

TEST_F(FormSubmissionTrackerUtilTest, NotRendererInitiated) {
  EXPECT_CALL(observer(),
              DidNavigateMainFrame(/*form_may_be_submitted=*/false));
  NotifyDidNavigateMainFrame(
      /*is_renderer_initiated=*/false, ui::PAGE_TRANSITION_RELOAD,
      /*was_initiated_by_link_click=*/true, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, LinkTransition) {
  EXPECT_CALL(observer(),
              DidNavigateMainFrame(/*form_may_be_submitted=*/false));
  NotifyDidNavigateMainFrame(
      /*is_renderer_initiated=*/true, ui::PAGE_TRANSITION_LINK,
      /*was_initiated_by_link_click=*/true, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, FormSubmission) {
  EXPECT_CALL(observer(), DidNavigateMainFrame(/*form_may_be_submitted=*/true));
  NotifyDidNavigateMainFrame(
      /*is_renderer_initiated=*/true, ui::PAGE_TRANSITION_FORM_SUBMIT,
      /*was_initiated_by_link_click=*/true, &observer());
}

TEST_F(FormSubmissionTrackerUtilTest, PageRedirectAfterJavaScriptSubmission) {
  EXPECT_CALL(observer(), DidNavigateMainFrame(/*form_may_be_submitted=*/true));
  NotifyDidNavigateMainFrame(
      /*is_renderer_initiated=*/true, ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*was_initiated_by_link_click=*/false, &observer());
}

}  // namespace
}  // namespace password_manager
