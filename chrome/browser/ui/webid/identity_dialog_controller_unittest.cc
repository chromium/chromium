// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "content/public/browser/identity_request_account.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;

namespace {

const std::vector<content::IdentityRequestDialogDisclosureField>
    kDefaultPermissions = {
        content::IdentityRequestDialogDisclosureField::kName,
        content::IdentityRequestDialogDisclosureField::kEmail,
        content::IdentityRequestDialogDisclosureField::kPicture};

}  // namespace

constexpr char16_t kTopFrameEtldPlusOne[] = u"top-frame-example.com";
constexpr char kIdpEtldPlusOne[] = "idp-example.com";
constexpr float kPerPageLoadClickthroughRate = 0.1;
constexpr float kPerClientClickthroughRate = 0.2;
constexpr float kPerImpressionClickthroughRate = 0.3;
constexpr float kLikelyToSignin = 0.4;
constexpr float kLikelyInsufficientData = 0.5;

// Mock version of AccountSelectionView for injection during tests.
class MockAccountSelectionView : public AccountSelectionView {
 public:
  MockAccountSelectionView() : AccountSelectionView(/*delegate=*/nullptr) {}
  ~MockAccountSelectionView() override = default;

  MockAccountSelectionView(const MockAccountSelectionView&) = delete;
  MockAccountSelectionView& operator=(const MockAccountSelectionView&) = delete;

  MOCK_METHOD(
      bool,
      Show,
      (const content::RelyingPartyData& rp_data,
       const std::vector<IdentityProviderDataPtr>& identity_provider_data,
       const std::vector<IdentityRequestAccountPtr>& accounts,
       blink::mojom::RpMode rp_mode,
       const std::vector<IdentityRequestAccountPtr>& new_accounts),
      (override));

  MOCK_METHOD(bool,
              ShowFailureDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata),
              (override));

  MOCK_METHOD(bool,
              ShowErrorDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata,
               const std::optional<TokenError>& error),
              (override));

  MOCK_METHOD(bool,
              ShowLoadingDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode),
              (override));

  MOCK_METHOD(bool,
              ShowVerifyingDialog,
              (const content::RelyingPartyData&,
               const IdentityProviderDataPtr&,
               const IdentityRequestAccountPtr&,
               Account::SignInMode sign_in_mode,
               blink::mojom::RpMode),
              (override));

  MOCK_METHOD(std::string, GetTitle, (), (const, override));

  MOCK_METHOD(std::optional<std::string>, GetSubtitle, (), (const, override));

  MOCK_METHOD(void, ShowUrl, (LinkType type, const GURL& url), (override));

  MOCK_METHOD(content::WebContents*,
              ShowModalDialog,
              (const GURL& url, blink::mojom::RpMode rp_mode),
              (override));

  MOCK_METHOD(void, CloseModalDialog, (), (override));

  MOCK_METHOD(content::WebContents*, GetRpWebContents, (), (override));
};

class IdentityDialogControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  IdentityDialogControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~IdentityDialogControllerTest() override = default;
  IdentityDialogControllerTest(IdentityDialogControllerTest&) = delete;
  IdentityDialogControllerTest& operator=(IdentityDialogControllerTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  void WaitForBubbleToBeShown(permissions::PermissionRequestManager* manager) {
    manager->DocumentOnLoadCompletedInPrimaryMainFrame();
    task_environment()->RunUntilIdle();
  }

  void Accept(permissions::PermissionRequestManager* manager) {
    manager->Accept();
    task_environment()->RunUntilIdle();
  }

  void Deny(permissions::PermissionRequestManager* manager) {
    manager->Deny();
    task_environment()->RunUntilIdle();
  }

  void Dismiss(permissions::PermissionRequestManager* manager) {
    manager->Dismiss();
    task_environment()->RunUntilIdle();
  }

  std::vector<IdentityRequestAccountPtr> CreateAccount() {
    return {base::MakeRefCounted<Account>(
        "account_id1", "", "", "", "", "", GURL(), "", "",
        /*login_hints=*/std::vector<std::string>(),
        /*domain_hints=*/std::vector<std::string>(),
        /*labels=*/std::vector<std::string>(),
        /*login_state=*/content::IdentityRequestAccount::LoginState::kSignUp,
        /*browser_trusted_login_state=*/
        content::IdentityRequestAccount::LoginState::kSignUp)};
  }

  IdentityProviderDataPtr CreateIdentityProviderData(
      std::vector<IdentityRequestAccountPtr>& accounts) {
    IdentityProviderDataPtr idp_data =
        base::MakeRefCounted<content::IdentityProviderData>(
            kIdpEtldPlusOne, content::IdentityProviderMetadata(),
            content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
            blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
            kDefaultPermissions,
            /*has_login_status_mismatch=*/false);
    for (auto& account : accounts) {
      account->identity_provider = idp_data;
    }
    return idp_data;
  }

  void ShowAccountsDialog(
      IdentityDialogController* controller,
      blink::mojom::RpMode rp_mode,
      DismissCallback dismiss_callback = base::DoNothing()) {
    accounts_ = CreateAccount();
    IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);
    controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, accounts_, rp_mode,
        /*new_accounts=*/std::vector<IdentityRequestAccountPtr>(),
        /*on_selected=*/base::DoNothing(), /*on_add_account=*/base::DoNothing(),
        std::move(dismiss_callback),
        /*accounts_displayed_callback=*/base::DoNothing());
  }

  segmentation_platform::MockSegmentationPlatformService*
  CreateMockSegmentationPlatformService(const std::string& result_label,
                                        base::RunLoop& run_loop,
                                        bool delay_callback = false) {
    segmentation_platform_service_ = std::make_unique<
        segmentation_platform::MockSegmentationPlatformService>();
    ON_CALL(*segmentation_platform_service_,
            GetClassificationResult(_, _, _, _))
        .WillByDefault(testing::Invoke(
            [&run_loop, result_label, delay_callback, this](
                auto, auto,
                scoped_refptr<segmentation_platform::InputContext>
                    input_context,
                segmentation_platform::ClassificationResultCallback callback) {
              segmentation_platform::ClassificationResult result(
                  segmentation_platform::PredictionStatus::kSucceeded);
              result.request_id = segmentation_platform::TrainingRequestId(1);
              result.ordered_labels = {result_label};
              if (this->optimization_guide_decider_) {
                ASSERT_EQ(segmentation_platform::processing::ProcessedValue(
                              web_contents()->GetLastCommittedURL().host()),
                          input_context->GetMetadataArgument(
                              segmentation_platform::kFedCmHost));
                ASSERT_EQ(segmentation_platform::processing::ProcessedValue(
                              web_contents()->GetLastCommittedURL()),
                          input_context->GetMetadataArgument(
                              segmentation_platform::kFedCmUrl));
                ASSERT_EQ(segmentation_platform::processing::ProcessedValue(
                              kPerPageLoadClickthroughRate),
                          input_context->GetMetadataArgument(
                              segmentation_platform::
                                  kFedCmPerPageLoadClickthroughRate));
                ASSERT_EQ(segmentation_platform::processing::ProcessedValue(
                              kPerClientClickthroughRate),
                          input_context->GetMetadataArgument(
                              segmentation_platform::
                                  kFedCmPerClientClickthroughRate));
                ASSERT_EQ(segmentation_platform::processing::ProcessedValue(
                              kPerImpressionClickthroughRate),
                          input_context->GetMetadataArgument(
                              segmentation_platform::
                                  kFedCmPerImpressionClickthroughRate));
                ASSERT_EQ(segmentation_platform::processing::ProcessedValue(
                              kLikelyToSignin),
                          input_context->GetMetadataArgument(
                              segmentation_platform::kFedCmLikelyToSignin));
                ASSERT_EQ(
                    segmentation_platform::processing::ProcessedValue(
                        kLikelyInsufficientData),
                    input_context->GetMetadataArgument(
                        segmentation_platform::kFedCmLikelyInsufficientData));
              }
              if (delay_callback) {
                segmentation_platform_service_callback_ =
                    base::BindOnce(std::move(callback), result)
                        .Then(run_loop.QuitClosure());
                return;
              }
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), result)
                                 .Then(run_loop.QuitClosure()));
            }));
    return segmentation_platform_service_.get();
  }

  optimization_guide::MockOptimizationGuideDecider*
  CreateMockOptimizationGuideDecider() {
    optimization_guide_decider_ =
        std::make_unique<optimization_guide::MockOptimizationGuideDecider>();
    EXPECT_CALL(*optimization_guide_decider_,
                RegisterOptimizationTypes(testing::ElementsAre(
                    optimization_guide::proto::FEDCM_CLICKTHROUGH_RATE)))
        .Times(1);
    ON_CALL(*optimization_guide_decider_,
            CanApplyOptimization(
                _,
                optimization_guide::proto::OptimizationType::
                    FEDCM_CLICKTHROUGH_RATE,
                testing::An<optimization_guide::OptimizationMetadata*>()))
        .WillByDefault(
            [](const GURL& url,
               optimization_guide::proto::OptimizationType optimization_type,
               optimization_guide::OptimizationMetadata* metadata)
                -> optimization_guide::OptimizationGuideDecision {
              *metadata = {};
              webid::FedCmClickthroughRateMetadata fedcm_metadata;
              fedcm_metadata.set_per_page_load_clickthrough_rate(
                  kPerPageLoadClickthroughRate);
              fedcm_metadata.set_per_client_clickthrough_rate(
                  kPerClientClickthroughRate);
              fedcm_metadata.set_per_impression_clickthrough_rate(
                  kPerImpressionClickthroughRate);
              fedcm_metadata.set_likely_to_signin(kLikelyToSignin);
              fedcm_metadata.set_likely_insufficient_data(
                  kLikelyInsufficientData);
              metadata->SetAnyMetadataForTesting(fedcm_metadata);
              return optimization_guide::OptimizationGuideDecision::kTrue;
            });
    return optimization_guide_decider_.get();
  }

 protected:
  std::vector<IdentityRequestAccountPtr> accounts_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
  std::unique_ptr<optimization_guide::MockOptimizationGuideDecider>
      optimization_guide_decider_;
  base::OnceClosure segmentation_platform_service_callback_;
};

TEST_F(IdentityDialogControllerTest, Accept) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(true)).WillOnce(testing::Return());
  controller->RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Accept(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Deny) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller->RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Deny(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

TEST_F(IdentityDialogControllerTest, Dismiss) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());

  base::MockCallback<base::OnceCallback<void(bool accepted)>> callback;
  EXPECT_CALL(callback, Run(false)).WillOnce(testing::Return());
  controller->RequestIdPRegistrationPermision(
      url::Origin::Create(GURL("https://idp.example")), callback.Get());

  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto prompt_factory =
      std::make_unique<permissions::MockPermissionPromptFactory>(manager);

  WaitForBubbleToBeShown(manager);

  EXPECT_TRUE(prompt_factory->is_visible());

  Dismiss(manager);

  EXPECT_FALSE(prompt_factory->is_visible());
}

// Test that selecting an account in button mode, and then dismissing it should
// run the dismiss callback.
TEST_F(IdentityDialogControllerTest, OnAccountSelectedButtonCallsDismiss) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  // Dismiss callback should run once.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).WillOnce(testing::Return());

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kActive,
                     dismiss_callback.Get());

  // User selects an account, and then dismisses it. The expectation set for
  // dismiss callback should pass.
  controller->OnAccountSelected(GURL(kIdpEtldPlusOne), accounts[0]->id,
                                *accounts[0]->login_state);
  controller->OnDismiss(IdentityDialogController::DismissReason::kOther);
}

// Test that selecting an account in widget, and then dismissing it should not
// run the dismiss callback.
TEST_F(IdentityDialogControllerTest, OnAccountSelectedWidgetResetsDismiss) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  // Dismiss callback should not be run.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).Times(0);

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive,
                     dismiss_callback.Get());

  // User selects an account, and then dismisses it. The expectation set for
  // dismiss callback should pass.
  controller->OnAccountSelected(GURL(kIdpEtldPlusOne), accounts[0]->id,
                                *accounts[0]->login_state);
  controller->OnDismiss(IdentityDialogController::DismissReason::kOther);
}

// Crash test for crbug.com/358302105.
TEST_F(IdentityDialogControllerTest, NoTabDoesNotCrash) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kActive);
}

TEST_F(IdentityDialogControllerTest, SegmentationPlatformShowUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(
      segmentation_platform::features::kSegmentationPlatformFedCmUser);
  // Mock the segmentation platform service to return "FedCmUserLoud" as the UI
  // volume recommendation.
  base::RunLoop run_loop;
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(
          web_contents(),
          CreateMockSegmentationPlatformService("FedCmUserLoud", run_loop),
          CreateMockOptimizationGuideDecider());

  // Show should be called.
  std::unique_ptr<MockAccountSelectionView> account_selection_view =
      std::make_unique<MockAccountSelectionView>();
  EXPECT_CALL(*account_selection_view, Show(_, _, _, _, _)).Times(1);
  controller->SetAccountSelectionViewForTesting(
      std::move(account_selection_view));

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);
  run_loop.Run();
}

TEST_F(IdentityDialogControllerTest, SegmentationPlatformDontShowUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(
      segmentation_platform::features::kSegmentationPlatformFedCmUser);
  // Mock the segmentation platform service to return "FedCmUserQuiet" as the UI
  // volume recommendation.
  base::RunLoop run_loop;
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(
          web_contents(),
          CreateMockSegmentationPlatformService("FedCmUserQuiet", run_loop),
          CreateMockOptimizationGuideDecider());

  // Show should not be called.
  std::unique_ptr<MockAccountSelectionView> account_selection_view =
      std::make_unique<MockAccountSelectionView>();
  EXPECT_CALL(*account_selection_view, Show(_, _, _, _, _)).Times(0);
  controller->SetAccountSelectionViewForTesting(
      std::move(account_selection_view));

  // Dismiss callback should be run.
  base::MockCallback<DismissCallback> dismiss_callback;
  EXPECT_CALL(dismiss_callback, Run).WillOnce(testing::Return());

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive,
                     dismiss_callback.Get());
  run_loop.Run();
}

TEST_F(IdentityDialogControllerTest,
       SegmentationPlatformTrainingDataCollection) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(
      segmentation_platform::features::kSegmentationPlatformFedCmUser);

  auto CheckForSampleAndReset([&](IdentityDialogController::UserAction action) {
    histogram_tester_->ExpectUniqueSample(
        "Blink.FedCm.SegmentationPlatform.UserAction", static_cast<int>(action),
        1);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  });

  {
    // User proceeds with an account.
    base::RunLoop run_loop;
    std::unique_ptr<IdentityDialogController> controller =
        std::make_unique<IdentityDialogController>(
            web_contents(),
            CreateMockSegmentationPlatformService("FedCmUserLoud", run_loop),
            CreateMockOptimizationGuideDecider());
    controller->SetAccountSelectionViewForTesting(
        std::make_unique<MockAccountSelectionView>());
    EXPECT_CALL(*segmentation_platform_service_,
                CollectTrainingData(_, _, _, _, _))
        .Times(1);

    ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);
    run_loop.Run();

    // User selects an account.
    controller->OnAccountSelected(GURL(kIdpEtldPlusOne), accounts_[0]->id,
                                  *accounts_[0]->login_state);
  }
  CheckForSampleAndReset(IdentityDialogController::UserAction::kSuccess);

  {
    // User clicks on close button.
    base::RunLoop run_loop;
    std::unique_ptr<IdentityDialogController> controller =
        std::make_unique<IdentityDialogController>(
            web_contents(),
            CreateMockSegmentationPlatformService("FedCmUserLoud", run_loop),
            CreateMockOptimizationGuideDecider());
    controller->SetAccountSelectionViewForTesting(
        std::make_unique<MockAccountSelectionView>());
    EXPECT_CALL(*segmentation_platform_service_,
                CollectTrainingData(_, _, _, _, _))
        .Times(1);

    ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);
    run_loop.Run();

    // User closes the dialog.
    controller->OnDismiss(
        IdentityDialogController::DismissReason::kCloseButton);
  }
  CheckForSampleAndReset(IdentityDialogController::UserAction::kClosed);

  {
    // User ignores the UI.
    base::RunLoop run_loop;
    std::unique_ptr<IdentityDialogController> controller =
        std::make_unique<IdentityDialogController>(
            web_contents(),
            CreateMockSegmentationPlatformService("FedCmUserLoud", run_loop),
            CreateMockOptimizationGuideDecider());
    controller->SetAccountSelectionViewForTesting(
        std::make_unique<MockAccountSelectionView>());
    EXPECT_CALL(*segmentation_platform_service_,
                CollectTrainingData(_, _, _, _, _))
        .Times(1);

    ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);
    run_loop.Run();

    // Dialog gets dismissed for other reasons.
    controller->OnDismiss(IdentityDialogController::DismissReason::kOther);
  }
  CheckForSampleAndReset(IdentityDialogController::UserAction::kIgnored);
}

TEST_F(IdentityDialogControllerTest,
       ControllerDestroyedBeforeSegmentationPlatformCallback) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(
      segmentation_platform::features::kSegmentationPlatformFedCmUser);
  // Mock the segmentation platform service to store its callback in
  // `segmentation_platform_service_callback_`.
  base::RunLoop run_loop;
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(
          web_contents(),
          CreateMockSegmentationPlatformService("FedCmUserLoud", run_loop,
                                                /*delay_callback=*/true),
          CreateMockOptimizationGuideDecider());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);

  // Reset the controller before running
  // `segmentation_platform_service_callback_`. This should not crash.
  controller.reset();
  std::move(segmentation_platform_service_callback_).Run();
  run_loop.Run();
}
