// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace ambient_signin {

namespace {

scoped_refptr<AuthenticatorRequestDialogModel> CreatePasskeyModel(
    content::RenderFrameHost* rfh,
    base::RepeatingClosure callback = base::DoNothing()) {
  auto model = base::MakeRefCounted<AuthenticatorRequestDialogModel>(rfh);
  model->relying_party_id = "example.com";
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      std::move(callback));
  return model;
}

}  // namespace

class AmbientSigninControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  AmbientSigninControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    NavigateAndCommit(GURL("https://example.com"));

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    AmbientLoginPermissionController::CreateForPage(
        web_contents()->GetPrimaryPage());

    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permission_request_manager());

    AmbientSigninController::CreateForCurrentDocument(main_rfh());
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    ukm_recorder_.reset();
    DeleteContents();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  AmbientSigninController* controller() {
    return AmbientSigninController::GetForCurrentDocument(main_rfh());
  }

  AmbientLoginPermissionController* permission_controller() {
    return AmbientLoginPermissionController::GetForPage(
        web_contents()->GetPrimaryPage());
  }

  permissions::PermissionRequestManager* permission_request_manager() {
    return permissions::PermissionRequestManager::FromWebContents(
        web_contents());
  }

  AmbientLoginPermissionRequest* current_ambient_request() {
    if (permission_request_manager()->Requests().empty()) {
      return nullptr;
    }
    return static_cast<AmbientLoginPermissionRequest*>(
        permission_request_manager()->Requests().front().get());
  }

  void CompleteRequestAndWait(AuthenticatorRequestDialogModel* model) {
    base::RunLoop run_loop;
    permission_controller()->SetFinishedNotificationForTesting(
        run_loop.QuitClosure());
    model->OnRequestComplete();
    run_loop.Run();
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
};

TEST_F(AmbientSigninControllerTest, ShowSinglePasskey) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      passkey_callback.Get());

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  EXPECT_CALL(passkey_callback, Run());
  permission_request_manager()->Accept(std::monostate());
  run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, ShowSinglePassword) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure password_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"username",
      ::features::IsRoundedIconsEnabled() ? kPasswordIcon
                                          : kPasswordFieldOldIcon,
      password_callback.Get());

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  EXPECT_CALL(password_callback, Run()).WillOnce([&model]() {
    model->OnRequestComplete();
  });
  permission_request_manager()->Accept(std::monostate());
  run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, ShowMultipleCredentials) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  base::MockRepeatingClosure password_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"alice@example.com",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      passkey_callback.Get(), u"Alice Smith");
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Password(
          AuthenticatorRequestDialogModel::Mechanism::PasswordInfo(
              std::nullopt)),
      u"bob@example.com",
      ::features::IsRoundedIconsEnabled() ? kPasswordIcon
                                          : kPasswordFieldOldIcon,
      password_callback.Get());

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();

  AmbientLoginPermissionRequest* ambient_request = current_ambient_request();
  ASSERT_NE(ambient_request, nullptr);
  ASSERT_EQ(ambient_request->credentials().size(), 2u);
  EXPECT_EQ(ambient_request->credentials()[0].username, u"alice@example.com");
  EXPECT_EQ(ambient_request->credentials()[0].display_name, u"Alice Smith");
  EXPECT_EQ(ambient_request->credentials()[0].type, CredentialType::kPasskey);
  EXPECT_EQ(ambient_request->credentials()[1].username, u"bob@example.com");
  EXPECT_EQ(ambient_request->credentials()[1].type, CredentialType::kPassword);

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  ambient_request->SelectCredential(1, /*from_expanded_list=*/true);
  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kRequestAdded);

  EXPECT_CALL(password_callback, Run()).WillOnce([&model]() {
    model->OnRequestComplete();
  });
  permission_request_manager()->Accept(std::monostate());
  run_loop.Run();
}

TEST_F(AmbientSigninControllerTest, DismissPromptClosesUIWithoutSigningIn) {
  auto model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  model->relying_party_id = "example.com";
  base::MockRepeatingClosure passkey_callback;
  model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {4, 5, 6}, std::nullopt}),
      u"username",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      passkey_callback.Get());

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  EXPECT_CALL(passkey_callback, Run()).Times(0);
  permission_request_manager()->Dismiss(std::monostate());
  run_loop.Run();

  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientSigninControllerTest, OnRequestCompleteClosesUI) {
  auto model = CreatePasskeyModel(main_rfh());

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();

  CompleteRequestAndWait(model.get());

  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientSigninControllerTest,
       SubsequentRequestWithNewModelAfterRequestComplete) {
  scoped_refptr<AuthenticatorRequestDialogModel> model1 =
      CreatePasskeyModel(main_rfh());

  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();

  CompleteRequestAndWait(model1.get());

  // Even while `model1` is still alive, a new request with `model2` should
  // succeed and display the prompt.
  scoped_refptr<AuthenticatorRequestDialogModel> model2 =
      CreatePasskeyModel(main_rfh());

  controller()->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();

  CompleteRequestAndWait(model2.get());
}

TEST_F(AmbientSigninControllerTest, ControllerDestroyedBeforeModelClosesUI) {
  scoped_refptr<AuthenticatorRequestDialogModel> model =
      CreatePasskeyModel(main_rfh());

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();

  base::RunLoop run_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      run_loop.QuitClosure());
  AmbientSigninController::DeleteForCurrentDocument(main_rfh());
  run_loop.Run();

  EXPECT_FALSE(permission_request_manager()->IsRequestInProgress());
  EXPECT_EQ(permission_controller()->state(),
            AmbientLoginPermissionController::State::kIdle);
}

TEST_F(AmbientSigninControllerTest,
       CooldownAfterTwoDismissalsAndResetOnSelection) {
  // 1st request: bubble starts open, user dismisses it (dismissal count -> 1).
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model1.get());

  // 2nd request: bubble still starts open after 1 dismissal; user dismisses it
  // a second time (dismissal count -> 2).
  auto model2 = CreatePasskeyModel(main_rfh());
  controller()->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model2.get());

  // 3rd request: after 2 dismissals, the request is created with
  // `bubble_starts_open = false`. Selecting a credential (e.g. after manually
  // opening the bubble from the chip) resets the dismissal counter to zero.
  base::MockRepeatingClosure passkey_callback;
  auto model3 = CreatePasskeyModel(main_rfh(), passkey_callback.Get());
  controller()->Show(model3.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_FALSE(current_ambient_request()->ShouldBubbleStartOpen());

  base::RunLoop accept_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      accept_loop.QuitClosure());
  EXPECT_CALL(passkey_callback, Run()).WillOnce([&model3]() {
    model3->OnRequestComplete();
  });
  permission_request_manager()->Accept(std::monostate());
  accept_loop.Run();

  // 4th request: bubble starts open again, and even after 1 new dismissal the
  // 5th request still starts open because the counter was reset to 0.
  auto model4 = CreatePasskeyModel(main_rfh());
  controller()->Show(model4.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model4.get());

  auto model5 = CreatePasskeyModel(main_rfh());
  controller()->Show(model5.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model5.get());
}

TEST_F(AmbientSigninControllerTest,
       SelectingCredentialAfterDismissingInSameRequestResetsCounter) {
  // 1st request: user dismisses the bubble (dismissal count -> 1).
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model1.get());

  // 2nd request: user dismisses the bubble (dismissal count -> 2), then clicks
  // the remaining chip to reopen the bubble and selects a credential.
  base::MockRepeatingClosure passkey_callback;
  auto model2 = CreatePasskeyModel(main_rfh(), passkey_callback.Get());
  controller()->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();
  current_ambient_request()->OnBubbleDismissed();

  base::RunLoop accept_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      accept_loop.QuitClosure());
  EXPECT_CALL(passkey_callback, Run()).WillOnce([&model2]() {
    model2->OnRequestComplete();
  });
  permission_request_manager()->Accept(std::monostate());
  accept_loop.Run();

  // 3rd request: counter was reset to 0, so bubble starts open.
  auto model3 = CreatePasskeyModel(main_rfh());
  controller()->Show(model3.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model3.get());
}

TEST_F(AmbientSigninControllerTest, CooldownTrackedPerTopLevelSiteETLDPlus1) {
  // 1st dismissal on https://a.example.com.
  NavigateAndCommit(GURL("https://a.example.com"));
  AmbientSigninController::GetOrCreateForCurrentDocument(main_rfh());
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model1.get());

  // 2nd dismissal on https://b.example.com from a cross-origin iframe
  // https://iframe.other.test.
  NavigateAndCommit(GURL("https://b.example.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://iframe.other.test"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));
  auto* subframe_controller =
      AmbientSigninController::GetOrCreateForCurrentDocument(subframe);
  auto model2 = CreatePasskeyModel(subframe);
  subframe_controller->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model2.get());

  // Navigating to https://c.example.com has cooldown active (`example.com` has
  // 2 dismissals).
  NavigateAndCommit(GURL("https://c.example.com"));
  AmbientSigninController::GetOrCreateForCurrentDocument(main_rfh());
  auto model3 = CreatePasskeyModel(main_rfh());
  controller()->Show(model3.get());
  prompt_factory_->WaitForPermissionBubble();
  EXPECT_FALSE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model3.get());

  // Navigating to https://other.test is unaffected and starts with the bubble
  // open.
  NavigateAndCommit(GURL("https://other.test"));
  AmbientSigninController::GetOrCreateForCurrentDocument(main_rfh());
  auto model4 = CreatePasskeyModel(main_rfh());
  controller()->Show(model4.get());
  prompt_factory_->WaitForPermissionBubble();
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model4.get());
}

TEST_F(AmbientSigninControllerTest,
       BubbleShowsAgainAfterOneWeekWithoutDismissal) {
  // Dismiss the bubble twice (dismissal count -> 2).
  for (int i = 0; i < 2; ++i) {
    auto model = CreatePasskeyModel(main_rfh());
    controller()->Show(model.get());
    prompt_factory_->WaitForPermissionBubble();
    ASSERT_NE(current_ambient_request(), nullptr);
    EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
    current_ambient_request()->OnBubbleDismissed();
    CompleteRequestAndWait(model.get());
  }

  // After 6 days (less than 1 week), cooldown is still active.
  task_environment()->AdvanceClock(base::Days(6));
  auto model_before_week = CreatePasskeyModel(main_rfh());
  controller()->Show(model_before_week.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_FALSE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model_before_week.get());

  // After 1 more day (7 days total since last dismissal), the bubble starts
  // open again even though the dismissal count is 2.
  task_environment()->AdvanceClock(base::Days(1));
  auto model_after_week = CreatePasskeyModel(main_rfh());
  controller()->Show(model_after_week.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model_after_week.get());

  // Dismissing the bubble again immediately re-activates the cooldown.
  auto model_dismissed_again = CreatePasskeyModel(main_rfh());
  controller()->Show(model_dismissed_again.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_FALSE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model_dismissed_again.get());
}

TEST_F(AmbientSigninControllerTest, DismissalCountExpiresAfterThirtyDays) {
  // Dismiss the bubble twice (dismissal count -> 2).
  for (int i = 0; i < 2; ++i) {
    auto model = CreatePasskeyModel(main_rfh());
    controller()->Show(model.get());
    prompt_factory_->WaitForPermissionBubble();
    ASSERT_NE(current_ambient_request(), nullptr);
    EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
    current_ambient_request()->OnBubbleDismissed();
    CompleteRequestAndWait(model.get());
  }

  // After 31 days (> 30-day content setting expiration), the stored dismissal
  // count has expired completely, so it takes 2 new dismissals to re-activate
  // cooldown rather than 1.
  task_environment()->AdvanceClock(base::Days(31));

  // 1st dismissal after expiration (new dismissal count -> 1).
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model1.get());

  // 2nd request after expiration still starts with the bubble open because the
  // count was reset to 1 (not 3).
  auto model2 = CreatePasskeyModel(main_rfh());
  controller()->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_TRUE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model2.get());
}

TEST_F(AmbientSigninControllerTest, AmbientChipOnlySuppressesInitialBubble) {
  base::HistogramTester histograms;
  auto model = CreatePasskeyModel(main_rfh());
  model->ambient_chip_only = true;

  controller()->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  EXPECT_FALSE(current_ambient_request()->ShouldBubbleStartOpen());
  CompleteRequestAndWait(model.get());

  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.DisplayOutcome",
      webauthn::metrics::AmbientDisplayOutcome::kShown, 1);
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.InitialUiMode",
      webauthn::metrics::AmbientInitialUiMode::kChipOnlyRequestedBySite, 1);
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.ChipOnlyOutcome",
      webauthn::metrics::AmbientChipOnlyOutcome::kIgnored, 1);
  histograms.ExpectTotalCount("WebAuthentication.Ambient.BubbleCredentialCount",
                              0);
  histograms.ExpectTotalCount(
      "WebAuthentication.Ambient.BubbleResolution.InitiallyShown", 0);
  histograms.ExpectTotalCount(
      "WebAuthentication.Ambient.BubbleResolution.OpenedFromChip", 0);
}

TEST_F(AmbientSigninControllerTest, MetricsForNoCredentialsAndPendingRequest) {
  base::HistogramTester histograms;

  // Empty model records kNoCredentials.
  auto empty_model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  empty_model->relying_party_id = "example.com";
  controller()->Show(empty_model.get());
  empty_model->OnRequestComplete();
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.DisplayOutcome",
      webauthn::metrics::AmbientDisplayOutcome::kNoCredentials, 1);

  // Active request followed by another request while pending records
  // kAnotherRequestPending.
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  histograms.ExpectBucketCount("WebAuthentication.Ambient.DisplayOutcome",
                               webauthn::metrics::AmbientDisplayOutcome::kShown,
                               1);

  controller()->Show(model1.get());
  histograms.ExpectBucketCount(
      "WebAuthentication.Ambient.DisplayOutcome",
      webauthn::metrics::AmbientDisplayOutcome::kAnotherRequestPending, 1);
  CompleteRequestAndWait(model1.get());
}

TEST_F(AmbientSigninControllerTest,
       MetricsForBubbleResolutionsAndSelectionMethods) {
  base::HistogramTester histograms;

  // 1. Bubble initially shown, user ignores it (request completes without
  // interaction).
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();
  CompleteRequestAndWait(model1.get());
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.BubbleCredentialCount", 1, 1);
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.BubbleResolution.InitiallyShown",
      webauthn::metrics::AmbientBubbleResolution::kIgnored, 1);

  // 2. Bubble initially shown with 2 credentials, user unfolds and selects the
  // second (non-default) credential.
  auto model2 = CreatePasskeyModel(main_rfh());
  model2->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Credential(
          {device::AuthenticatorType::kEnclave, {7, 8, 9}, std::nullopt}),
      u"username2",
      ::features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                          : vector_icons::kPasskeyOldIcon,
      base::BindRepeating(
          [](AuthenticatorRequestDialogModel* m) { m->OnRequestComplete(); },
          base::Unretained(model2.get())));
  controller()->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->SelectCredential(/*index=*/1,
                                              /*from_expanded_list=*/true);

  base::RunLoop accept_loop;
  permission_controller()->SetFinishedNotificationForTesting(
      accept_loop.QuitClosure());
  permission_request_manager()->Accept(std::monostate());
  accept_loop.Run();

  histograms.ExpectBucketCount(
      "WebAuthentication.Ambient.BubbleCredentialCount", 2, 1);
  histograms.ExpectBucketCount(
      "WebAuthentication.Ambient.BubbleResolution.InitiallyShown",
      webauthn::metrics::AmbientBubbleResolution::kCredentialSelected, 1);
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.CredentialSelectionMethod",
      webauthn::metrics::AmbientCredentialSelectionMethod::
          kExpandedNonDefaultCredentialClicked,
      1);

  // 3. Chip-only mode where user clicks the chip to activate the bubble, then
  // dismisses the bubble.
  auto model3 = CreatePasskeyModel(main_rfh());
  model3->ambient_chip_only = true;
  controller()->Show(model3.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model3.get());

  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.ChipOnlyOutcome",
      webauthn::metrics::AmbientChipOnlyOutcome::kBubbleActivated, 1);
  histograms.ExpectUniqueSample(
      "WebAuthentication.Ambient.BubbleResolution.OpenedFromChip",
      webauthn::metrics::AmbientBubbleResolution::kDismissed, 1);

  // 4. Restarting a request via OnStartOver() does not emit duplicate
  // DisplayOutcome, InitialUiMode, BubbleCredentialCount, BubbleResolution, or
  // CredentialSelectionMethod samples.
  auto model4 = CreatePasskeyModel(main_rfh());
  controller()->Show(model4.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();

  {
    base::RunLoop loop;
    permission_controller()->SetFinishedNotificationForTesting(
        loop.QuitClosure());
    permission_request_manager()->Accept(std::monostate());
    loop.Run();
  }

  histograms.ExpectBucketCount("WebAuthentication.Ambient.DisplayOutcome",
                               webauthn::metrics::AmbientDisplayOutcome::kShown,
                               4);
  histograms.ExpectBucketCount("WebAuthentication.Ambient.InitialUiMode",
                               webauthn::metrics::AmbientInitialUiMode::kBubble,
                               3);
  histograms.ExpectBucketCount(
      "WebAuthentication.Ambient.BubbleCredentialCount", 1, 3);
  histograms.ExpectTotalCount(
      "WebAuthentication.Ambient.BubbleResolution.InitiallyShown", 3);
  histograms.ExpectTotalCount(
      "WebAuthentication.Ambient.CredentialSelectionMethod", 2);

  model4->OnStartOver();
  controller()->Show(model4.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();

  {
    base::RunLoop loop;
    permission_controller()->SetFinishedNotificationForTesting(
        loop.QuitClosure());
    permission_request_manager()->Accept(std::monostate());
    loop.Run();
  }
  model4->OnRequestComplete();

  histograms.ExpectBucketCount("WebAuthentication.Ambient.DisplayOutcome",
                               webauthn::metrics::AmbientDisplayOutcome::kShown,
                               4);
  histograms.ExpectBucketCount("WebAuthentication.Ambient.InitialUiMode",
                               webauthn::metrics::AmbientInitialUiMode::kBubble,
                               3);
  histograms.ExpectBucketCount(
      "WebAuthentication.Ambient.BubbleCredentialCount", 1, 3);
  histograms.ExpectTotalCount(
      "WebAuthentication.Ambient.BubbleResolution.InitiallyShown", 3);
  histograms.ExpectTotalCount(
      "WebAuthentication.Ambient.CredentialSelectionMethod", 2);
}

TEST_F(AmbientSigninControllerTest, UkmMetricsForOutcomesAndInteractions) {
  using UkmEntry = ukm::builders::WebAuthn_AmbientSignIn;

  // 1. Empty model -> DisplayOutcome::kNoCredentials.
  auto empty_model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(main_rfh());
  empty_model->relying_party_id = "example.com";
  controller()->Show(empty_model.get());
  empty_model->OnRequestComplete();

  EXPECT_THAT(
      ukm_recorder_->GetMetricsEntryValues(UkmEntry::kEntryName,
                                           UkmEntry::kDisplayOutcomeName),
      testing::ElementsAre(static_cast<int64_t>(
          webauthn::metrics::AmbientUkmDisplayOutcome::kNoCredentials)));

  // 2. Bubble shown, another request while pending -> kNotShownOther, then
  // default credential selected in unexpanded bubble ->
  // kSelectedDefaultInUnexpandedBubble.
  auto model1 = CreatePasskeyModel(main_rfh());
  controller()->Show(model1.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();

  // Attempt while pending records kNotShownOther.
  controller()->Show(model1.get());

  current_ambient_request()->SelectCredential(/*index=*/0,
                                              /*from_expanded_list=*/false);
  {
    base::RunLoop loop;
    permission_controller()->SetFinishedNotificationForTesting(
        loop.QuitClosure());
    permission_request_manager()->Accept(std::monostate());
    loop.Run();
  }
  model1->OnRequestComplete();

  EXPECT_THAT(
      ukm_recorder_->GetMetricsEntryValues(UkmEntry::kEntryName,
                                           UkmEntry::kDisplayOutcomeName),
      testing::ElementsAre(
          static_cast<int64_t>(
              webauthn::metrics::AmbientUkmDisplayOutcome::kNoCredentials),
          static_cast<int64_t>(
              webauthn::metrics::AmbientUkmDisplayOutcome::kShown),
          static_cast<int64_t>(
              webauthn::metrics::AmbientUkmDisplayOutcome::kNotShownOther)));
  EXPECT_THAT(ukm_recorder_->GetMetricsEntryValues(
                  UkmEntry::kEntryName, UkmEntry::kInitialUiModeName),
              testing::ElementsAre(static_cast<int64_t>(
                  webauthn::metrics::AmbientInitialUiMode::kBubble)));
  EXPECT_THAT(ukm_recorder_->GetMetricsEntryValues(
                  UkmEntry::kEntryName, UkmEntry::kBubbleOutcomeName),
              testing::ElementsAre(static_cast<int64_t>(
                  webauthn::metrics::AmbientUkmBubbleOutcome::
                      kSelectedDefaultInUnexpandedBubble)));

  // 3. Chip-only mode (opted in by site): ignored without user interaction ->
  // kIgnored.
  auto model2 = CreatePasskeyModel(main_rfh());
  model2->ambient_chip_only = true;
  controller()->Show(model2.get());
  prompt_factory_->WaitForPermissionBubble();
  CompleteRequestAndWait(model2.get());

  EXPECT_THAT(ukm_recorder_->GetMetricsEntryValues(
                  UkmEntry::kEntryName, UkmEntry::kInitialUiModeName),
              testing::ElementsAre(
                  static_cast<int64_t>(
                      webauthn::metrics::AmbientInitialUiMode::kBubble),
                  static_cast<int64_t>(webauthn::metrics::AmbientInitialUiMode::
                                           kChipOnlyRequestedBySite)));
  EXPECT_THAT(ukm_recorder_->GetMetricsEntryValues(
                  UkmEntry::kEntryName, UkmEntry::kChipOnlyOutcomeName),
              testing::ElementsAre(static_cast<int64_t>(
                  webauthn::metrics::AmbientChipOnlyOutcome::kIgnored)));

  // 4. Chip-only mode: user clicks chip to open bubble, then expands and
  // selects a credential -> kBubbleActivated and kSelectedInExpandedBubble.
  auto model3 = CreatePasskeyModel(main_rfh());
  model3->ambient_chip_only = true;
  controller()->Show(model3.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();
  current_ambient_request()->SelectCredential(/*index=*/0,
                                              /*from_expanded_list=*/true);
  {
    base::RunLoop loop;
    permission_controller()->SetFinishedNotificationForTesting(
        loop.QuitClosure());
    permission_request_manager()->Accept(std::monostate());
    loop.Run();
  }
  model3->OnRequestComplete();

  EXPECT_THAT(
      ukm_recorder_->GetMetricsEntryValues(UkmEntry::kEntryName,
                                           UkmEntry::kChipOnlyOutcomeName),
      testing::ElementsAre(
          static_cast<int64_t>(
              webauthn::metrics::AmbientChipOnlyOutcome::kIgnored),
          static_cast<int64_t>(
              webauthn::metrics::AmbientChipOnlyOutcome::kBubbleActivated)));
  EXPECT_THAT(
      ukm_recorder_->GetMetricsEntryValues(UkmEntry::kEntryName,
                                           UkmEntry::kBubbleOutcomeName),
      testing::ElementsAre(
          static_cast<int64_t>(webauthn::metrics::AmbientUkmBubbleOutcome::
                                   kSelectedDefaultInUnexpandedBubble),
          static_cast<int64_t>(webauthn::metrics::AmbientUkmBubbleOutcome::
                                   kSelectedInExpandedBubble)));
}

TEST_F(AmbientSigninControllerTest, UkmTriggeredFromSubframeEmitsMainFrameUrl) {
  using UkmEntry = ukm::builders::WebAuthn_AmbientSignIn;

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://subframe.example.org"), subframe);
  AmbientSigninController::CreateForCurrentDocument(subframe);
  AmbientSigninController* subframe_controller =
      AmbientSigninController::GetForCurrentDocument(subframe);
  ASSERT_NE(subframe_controller, nullptr);

  auto model = CreatePasskeyModel(subframe);
  subframe_controller->Show(model.get());
  prompt_factory_->WaitForPermissionBubble();
  ASSERT_NE(current_ambient_request(), nullptr);
  current_ambient_request()->OnBubbleShown();
  current_ambient_request()->OnBubbleDismissed();
  CompleteRequestAndWait(model.get());

  const auto entries = ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 3u);
  for (const ukm::mojom::UkmEntry* entry : entries) {
    ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL("https://example.com"));
  }
  EXPECT_THAT(ukm_recorder_->GetMetricsEntryValues(
                  UkmEntry::kEntryName, UkmEntry::kBubbleOutcomeName),
              testing::ElementsAre(static_cast<int64_t>(
                  webauthn::metrics::AmbientUkmBubbleOutcome::kDismissed)));
}

}  // namespace ambient_signin
