// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
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
  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), base::DoNothing(), base::DoNothing());
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
  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://subframe.example.org"), GURL("https://example.com"),
      CreateSinglePasskey(), base::DoNothing(), base::DoNothing());
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
  auto request1 = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), base::DoNothing(), base::DoNothing());
  controller()->SetFinishedNotificationForTesting(
      first_finished_loop.QuitClosure());

  controller()->RequestPermission(main_rfh(), std::move(request1));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return permission_request_manager()->IsRequestInProgress(); }));
  EXPECT_EQ(controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  std::optional<PermissionDecision> second_decision;
  auto request2 = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      CreateSinglePasskey(), base::DoNothing(),
      base::BindRepeating(
          [](std::optional<PermissionDecision>* out,
             const permissions::PermissionPromptDecision& decision,
             const permissions::PermissionRequestData& request_data) {
            *out = decision.overall_decision;
          },
          &second_decision));

  controller()->RequestPermission(main_rfh(), std::move(request2));

  // The second request should fail immediately (cancelled/dismissed) and not
  // disrupt the first outstanding request.
  EXPECT_EQ(second_decision, PermissionDecision::kNone);
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
  std::optional<size_t> selected_index;

  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      GURL("https://example.com"), GURL("https://example.com"),
      std::move(credentials),
      base::BindOnce(
          [](std::optional<size_t>* out, size_t index) { *out = index; },
          &selected_index),
      base::DoNothing());
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

}  // namespace ambient_signin
