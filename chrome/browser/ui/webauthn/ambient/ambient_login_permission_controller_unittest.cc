// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ambient_signin {

namespace {

std::vector<PasskeyOrPasswordCredential> CreateSinglePasskey() {
  return {{u"user@example.com", u"Google Password Manager"}};
}

}  // namespace

class AmbientLoginPermissionControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents(), mock_tab_interface_.get());
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    permission_request_manager()
        ->set_enabled_app_level_notification_permission_for_testing(true);

    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permission_request_manager());

    NavigateAndCommit(GURL("https://example.com"));
    AmbientLoginPermissionController::CreateForPage(
        web_contents()->GetPrimaryPage());
  }

  void TearDown() override {
    prompt_factory_.reset();
    DeleteContents();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  AmbientLoginPermissionController* controller() {
    return AmbientLoginPermissionController::GetForPage(
        web_contents()->GetPrimaryPage());
  }

  permissions::PermissionRequestManager* permission_request_manager() {
    return permissions::PermissionRequestManager::FromWebContents(
        web_contents());
  }

 protected:
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
};

TEST_F(AmbientLoginPermissionControllerTest, InitialStateIsIdle) {
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientLoginPermissionControllerTest, RequestPermission) {
  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(size_t)> credential_selected_callback;
  EXPECT_CALL(credential_selected_callback, Run).Times(0);

  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), credential_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(run_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  permission_request_manager()->Dismiss(std::monostate());
  run_loop.Run();

  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientLoginPermissionControllerTest,
       RequestPermissionFromCrossOriginSubframe) {
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://subframe.example.org"), subframe);
  EXPECT_EQ(AmbientLoginPermissionController::GetForPage(subframe->GetPage()),
            controller());

  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(size_t)> credential_selected_callback;
  EXPECT_CALL(credential_selected_callback, Run).Times(0);

  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://subframe.example.org"), GURL("https://example.com"),
      CreateSinglePasskey(), credential_selected_callback.Get());
  EXPECT_EQ(request->requesting_origin(), GURL("https://subframe.example.org"));
  EXPECT_EQ(request->embedding_origin(), GURL("https://example.com"));

  controller()->SetFinishedNotificationForTesting(run_loop.QuitClosure());

  controller()->RequestPermission(subframe, std::move(request));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  permission_request_manager()->Dismiss(std::monostate());
  run_loop.Run();

  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientLoginPermissionControllerTest,
       SecondRequestFailsWhileFirstIsOutstanding) {
  base::RunLoop first_finished_loop;
  base::MockRepeatingCallback<void(size_t)> first_selected_callback;
  EXPECT_CALL(first_selected_callback, Run).Times(0);

  auto request1 = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), first_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(
      first_finished_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request1));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  base::MockRepeatingCallback<void(size_t)> second_selected_callback;
  EXPECT_CALL(second_selected_callback, Run).Times(0);
  base::MockRepeatingCallback<void(const permissions::PermissionPromptDecision&,
                                   const permissions::PermissionRequestData&)>
      second_decided_callback;
  EXPECT_CALL(second_decided_callback,
              Run(testing::Field(
                      &permissions::PermissionPromptDecision::overall_decision,
                      PermissionDecision::kNone),
                  testing::_));

  auto request2 = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), second_selected_callback.Get(),
      second_decided_callback.Get());

  // The second request should fail immediately (cancelled/dismissed) and not
  // disrupt the first outstanding request.
  controller()->RequestPermission(main_rfh(), std::move(request2));
  testing::Mock::VerifyAndClearExpectations(&second_decided_callback);
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);
  EXPECT_TRUE(permission_request_manager()->IsRequestInProgress());

  permission_request_manager()->Dismiss(std::monostate());
  first_finished_loop.Run();

  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientLoginPermissionControllerTest, RequestPermissionWithCredentials) {
  base::RunLoop run_loop;
  std::vector<PasskeyOrPasswordCredential> credentials = {
      {u"alice@example.com", u"Google Password Manager",
       CredentialType::kPasskey},
      {u"bob@example.com", u"Google Password Manager",
       CredentialType::kPassword}};
  base::MockRepeatingCallback<void(size_t)> credential_selected_callback;
  EXPECT_CALL(credential_selected_callback, Run).Times(0);

  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      std::move(credentials), credential_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(run_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  permission_request_manager()->Dismiss(std::monostate());
  run_loop.Run();

  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientLoginPermissionControllerTest,
       ResetWhenRequestPendingDoesNotCrash) {
  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(size_t)> credential_selected_callback;
  EXPECT_CALL(credential_selected_callback, Run).Times(0);

  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), credential_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(run_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request));
  ASSERT_FALSE(permission_request_manager()->IsRequestInProgress());
  ASSERT_TRUE(permission_request_manager()->has_pending_requests());

  controller()->Reset();
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);

  run_loop.Run();
  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
}

TEST_F(AmbientLoginPermissionControllerTest, ResetWhenTabHiddenDoesNotCrash) {
  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(size_t)> credential_selected_callback;
  EXPECT_CALL(credential_selected_callback, Run).Times(0);

  std::unique_ptr<AmbientLoginPermissionRequest> request =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          CreateSinglePasskey(), credential_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(run_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));
  ASSERT_NE(permission_request_manager()->GetCurrentPrompt(), nullptr);

  // Hiding the tab destroys the prompt view (`GetCurrentPrompt() == nullptr`)
  // while keeping the request in progress (`IsRequestInProgress() == true`).
  web_contents()->WasHidden();
  ASSERT_TRUE(permission_request_manager()->IsRequestInProgress());
  ASSERT_EQ(permission_request_manager()->GetCurrentPrompt(), nullptr);

  controller()->Reset();
  run_loop.Run();

  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
}

TEST_F(AmbientLoginPermissionControllerTest,
       ResetDoesNotDismissUnrelatedActivePrompt) {
  base::RunLoop ambient_finished_loop;
  base::MockRepeatingCallback<void(size_t)> ambient_selected_callback;
  EXPECT_CALL(ambient_selected_callback, Run).Times(0);

  auto ambient_request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), ambient_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(
      ambient_finished_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(ambient_request));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  permissions::MockPermissionRequest::MockPermissionRequestState camera_state;
  auto camera_request = std::make_unique<permissions::MockPermissionRequest>(
      GURL("https://example.com"), permissions::RequestType::kCameraStream,
      camera_state.GetWeakPtr());
  permission_request_manager()->AddRequest(main_rfh(),
                                           std::move(camera_request));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return permission_request_manager()->IsRequestInProgress() &&
           !permission_request_manager()->Requests().empty() &&
           permission_request_manager()->Requests().front()->request_type() ==
               permissions::RequestType::kCameraStream;
  }));

  controller()->Reset();
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
  EXPECT_FALSE(camera_state.cancelled);
  ASSERT_TRUE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(permission_request_manager()->Requests().front()->request_type(),
            permissions::RequestType::kCameraStream);

  permission_request_manager()->Dismiss(std::monostate());
  ambient_finished_loop.Run();
  EXPECT_TRUE(camera_state.cancelled);
  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
}

// `AmbientLoginPermissionController` is scoped to `content::Page`, while
// `PermissionRequestManager` is scoped to `WebContents`. Verify that once a
// controller's request has finished, it does not interfere with subsequent
// requests added to the same `PermissionRequestManager` (e.g. when the first
// controller's page is kept alive in BFCache while another page issues a
// request).
TEST_F(AmbientLoginPermissionControllerTest,
       FinishedControllerDoesNotDismissSubsequentControllerPrompt) {
  base::RunLoop first_finished_loop;
  base::MockRepeatingCallback<void(size_t)> first_selected_callback;
  EXPECT_CALL(first_selected_callback, Run(0u));

  std::unique_ptr<AmbientLoginPermissionRequest> request1 =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          CreateSinglePasskey(), first_selected_callback.Get());
  controller()->SetFinishedNotificationForTesting(
      first_finished_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request1));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  permission_request_manager()->Accept(std::monostate());
  first_finished_loop.Run();
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);

  // Directly add `request2` to `PermissionRequestManager` (as another page's
  // `AmbientLoginPermissionController` in the same `WebContents` would) while
  // `controller()` is idle.
  base::MockRepeatingCallback<void(size_t)> second_selected_callback;
  EXPECT_CALL(second_selected_callback, Run).Times(0);

  std::unique_ptr<AmbientLoginPermissionRequest> request2 =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://other.example.org"), GURL("https://other.example.org"),
          CreateSinglePasskey(), second_selected_callback.Get());
  permission_request_manager()->AddRequest(main_rfh(), std::move(request2));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));

  EXPECT_TRUE(permission_request_manager()->IsRequestInProgress());

  permission_request_manager()->Dismiss(std::monostate());
}

TEST_F(AmbientLoginPermissionControllerTest,
       ResetWhilePendingFollowedByNewRequestFromSameOriginShowsSecondRequest) {
  base::MockRepeatingCallback<void(size_t index)> request1_selected_callback;
  base::MockRepeatingCallback<void(size_t index)> request2_selected_callback;
  EXPECT_CALL(request1_selected_callback, Run).Times(0);
  EXPECT_CALL(request2_selected_callback, Run(0u));

  std::unique_ptr<AmbientLoginPermissionRequest> request1 =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          CreateSinglePasskey(), request1_selected_callback.Get());
  std::unique_ptr<AmbientLoginPermissionRequest> request2 =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          CreateSinglePasskey(), request2_selected_callback.Get());
  AmbientLoginPermissionRequest* request2_ptr = request2.get();

  // Queue `request1` (still in `pending_permission_requests_` before
  // `DequeueRequestIfNeeded()` runs), then immediately `Reset()` and issue
  // `request2` from the same origin.
  controller()->RequestPermission(main_rfh(), std::move(request1));
  ASSERT_FALSE(permission_request_manager()->IsRequestInProgress());
  ASSERT_TRUE(permission_request_manager()->has_pending_requests());

  controller()->Reset();
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);

  controller()->RequestPermission(main_rfh(), std::move(request2));
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return permission_request_manager()->IsRequestInProgress() &&
           !permission_request_manager()->Requests().empty() &&
           permission_request_manager()->Requests().front().get() ==
               request2_ptr;
  }));

  permission_request_manager()->Accept(std::monostate());

  // The stale `request1` should be dropped before `ShowPrompt()` runs when
  // dequeued, so `show_count()` remains 1 (only `request2` was shown).
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !permission_request_manager()->IsRequestInProgress() &&
           !permission_request_manager()->has_pending_requests();
  }));
  EXPECT_EQ(prompt_factory_->show_count(), 1);
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientLoginPermissionControllerTest,
       MultipleQueuedRequestsResetAreDroppedBeforeShowPrompt) {
  permission_request_manager()->set_auto_response_for_test(
      permissions::PermissionRequestManager::DISMISS);

  base::MockRepeatingCallback<void(size_t)> request1_selected_callback;
  base::MockRepeatingCallback<void(size_t)> request2_selected_callback;
  EXPECT_CALL(request1_selected_callback, Run).Times(0);
  EXPECT_CALL(request2_selected_callback, Run).Times(0);

  std::unique_ptr<AmbientLoginPermissionRequest> request1 =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          CreateSinglePasskey(), request1_selected_callback.Get());
  std::unique_ptr<AmbientLoginPermissionRequest> request2 =
      std::make_unique<AmbientLoginPermissionRequest>(
          GURL("https://example.com"), GURL("https://example.com"),
          CreateSinglePasskey(), request2_selected_callback.Get());

  // Queue `request1`, call `Reset()`, queue `request2`, and call `Reset()`
  // again before `DequeueRequestIfNeeded()` runs. `Reset()` cancels and
  // removes each queued request immediately.
  controller()->RequestPermission(main_rfh(), std::move(request1));
  controller()->Reset();
  EXPECT_FALSE(permission_request_manager()->has_pending_requests());
  controller()->RequestPermission(main_rfh(), std::move(request2));
  controller()->Reset();
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
  EXPECT_FALSE(permission_request_manager()->has_pending_requests());
  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(prompt_factory_->show_count(), 0);
}

}  // namespace ambient_signin
