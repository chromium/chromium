// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::SaveArg;

namespace {

const std::vector<content::IdentityRequestDialogDisclosureField>
    kDefaultPermissions = {
        content::IdentityRequestDialogDisclosureField::kName,
        content::IdentityRequestDialogDisclosureField::kEmail,
        content::IdentityRequestDialogDisclosureField::kPicture};

}  // namespace

constexpr char kAccountId[] = "account_id1";
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

  MOCK_METHOD(void, OnPageActionClicked, (), (override));

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
              (const GURL& url,
               blink::mojom::RpMode rp_mode,
               content::IdentityRequestDialogController::ShownModalAsyncCallback
                   on_shown_async),
              (override));

  MOCK_METHOD(void, CloseModalDialog, (), (override));

  MOCK_METHOD(content::WebContents*, GetRpWebContents, (), (override));

  MOCK_METHOD(void, SetCanShowUi, (bool), (override));
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
    manager->Accept(/*prompt_options=*/std::monostate());
    task_environment()->RunUntilIdle();
  }

  void Deny(permissions::PermissionRequestManager* manager) {
    manager->Deny(/*prompt_options=*/std::monostate());
    task_environment()->RunUntilIdle();
  }

  void Dismiss(permissions::PermissionRequestManager* manager) {
    manager->Dismiss(/*prompt_options=*/std::monostate());
    task_environment()->RunUntilIdle();
  }

  std::vector<IdentityRequestAccountPtr> CreateAccount() {
    return {base::MakeRefCounted<Account>(
        kAccountId, "", "", "", "", "", GURL(), "", "",
        /*potentially_approved_origin_hashes=*/std::vector<std::string>(),
        /*login_hints=*/std::vector<std::string>(),
        /*domain_hints=*/std::vector<std::string>(),
        /*labels=*/std::vector<std::string>(),
        /*idp_claimed_login_state=*/
        content::IdentityRequestAccount::LoginState::kSignIn,
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
        {idp_data}, accounts_, /*filtered_accounts=*/{}, rp_mode,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(), std::move(dismiss_callback),
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
        .WillByDefault(
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
                              web_contents()->GetLastCommittedURL().GetHost()),
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
            });
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
              metadata->set_any_metadata(
                  optimization_guide::AnyWrapProto(fedcm_metadata));
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

TEST_F(IdentityDialogControllerTest, ActorTaskSuppressesUi) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* view_ptr = mock_view.get();

  // 1. Simulate an active actor task. This should call SetCanShowUi(false).
  EXPECT_CALL(*view_ptr, SetCanShowUi(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  controller->SetActingTaskIdForTesting(actor::TaskId::FromUnsafeValue(1));

  // 2. If there is an active task, ShowLoadingDialog should NOT be called.
  EXPECT_CALL(*view_ptr, ShowLoadingDialog).Times(0);

  EXPECT_TRUE(controller->ShowLoadingDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne, u""), kIdpEtldPlusOne,
      blink::mojom::RpContext::kSignIn, blink::mojom::RpMode::kActive,
      base::DoNothing()));
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
                                *accounts[0]->idp_claimed_login_state);
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
                                *accounts[0]->idp_claimed_login_state);
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

TEST_F(IdentityDialogControllerTest, SegmentationPlatformLoudUi) {
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

  base::MockCallback<IdentityDialogController::GetPassiveDialogVolumeCallback>
      callback;
  content::IdentityRequestDialogController::PassiveDialogVolume value;
  EXPECT_CALL(callback, Run).WillOnce(SaveArg<0>(&value));

  controller->GetPassiveDialogVolume(callback.Get());

  run_loop.Run();
  EXPECT_EQ(
      content::IdentityRequestDialogController::PassiveDialogVolume::kDefault,
      value);
}

TEST_F(IdentityDialogControllerTest, SegmentationPlatformAmbientUi) {
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

  base::MockCallback<IdentityDialogController::GetPassiveDialogVolumeCallback>
      callback;
  content::IdentityRequestDialogController::PassiveDialogVolume value;
  EXPECT_CALL(callback, Run).WillOnce(SaveArg<0>(&value));

  controller->GetPassiveDialogVolume(callback.Get());

  run_loop.Run();
  EXPECT_EQ(
      content::IdentityRequestDialogController::PassiveDialogVolume::kAmbient,
      value);
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

    auto mock_view = std::make_unique<MockAccountSelectionView>();
    EXPECT_CALL(*mock_view, Show).WillOnce(testing::Return(true));
    controller->SetAccountSelectionViewForTesting(std::move(mock_view));

    EXPECT_CALL(*segmentation_platform_service_,
                CollectTrainingData(_, _, _, _, _))
        .Times(1);

    controller->GetPassiveDialogVolume(base::DoNothing());
    run_loop.Run();
    ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);

    // User selects an account.
    controller->OnAccountSelected(GURL(kIdpEtldPlusOne), accounts_[0]->id,
                                  *accounts_[0]->idp_claimed_login_state);
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

    auto mock_view = std::make_unique<MockAccountSelectionView>();
    EXPECT_CALL(*mock_view, Show).WillOnce(testing::Return(true));
    controller->SetAccountSelectionViewForTesting(std::move(mock_view));

    EXPECT_CALL(*segmentation_platform_service_,
                CollectTrainingData(_, _, _, _, _))
        .Times(1);

    controller->GetPassiveDialogVolume(base::DoNothing());
    run_loop.Run();
    ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);

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

    auto mock_view = std::make_unique<MockAccountSelectionView>();
    EXPECT_CALL(*mock_view, Show).WillOnce(testing::Return(true));
    controller->SetAccountSelectionViewForTesting(std::move(mock_view));

    EXPECT_CALL(*segmentation_platform_service_,
                CollectTrainingData(_, _, _, _, _))
        .Times(1);

    controller->GetPassiveDialogVolume(base::DoNothing());
    run_loop.Run();
    ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);

    // Dialog gets dismissed for other reasons.
    controller->OnDismiss(IdentityDialogController::DismissReason::kOther);
  }
  CheckForSampleAndReset(IdentityDialogController::UserAction::kIgnored);
}

TEST_F(IdentityDialogControllerTest,
       SegmentationPlatformTrainingDataNotCollectedWhenUiNotShown) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(
      segmentation_platform::features::kSegmentationPlatformFedCmUser);

  base::RunLoop run_loop;
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(
          web_contents(),
          CreateMockSegmentationPlatformService("FedCmUserLoud", run_loop),
          CreateMockOptimizationGuideDecider());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  EXPECT_CALL(*mock_view, Show).WillOnce(testing::Return(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  EXPECT_CALL(*segmentation_platform_service_,
              CollectTrainingData(_, _, _, _, _))
      .Times(0);

  controller->GetPassiveDialogVolume(base::DoNothing());
  run_loop.Run();
  ShowAccountsDialog(controller.get(), blink::mojom::RpMode::kPassive);

  controller->OnDismiss(IdentityDialogController::DismissReason::kCloseButton);
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

  controller->GetPassiveDialogVolume(base::DoNothing());

  // Reset the controller before running
  // `segmentation_platform_service_callback_`. This should not crash.
  controller.reset();
  std::move(segmentation_platform_service_callback_).Run();
  run_loop.Run();
}

TEST_F(IdentityDialogControllerTest,
       AccountSelectionInvokedWhenShouldShowFedCmUiIsFalse) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  // Set up ActorLoginRequest to make ShouldShowFedCmUi returns false.
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  content::webid::FederatedEmbedderLoginRequest::Set(
      web_contents(), idp_origin, kAccountId, base::DoNothing());

  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  accounts[0]->idp_claimed_login_state =
      content::IdentityRequestAccount::LoginState::kSignIn;
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);
  idp_data->idp_metadata.config_url = idp_url;

  base::MockCallback<AccountSelectionCallback> on_selected;
  EXPECT_CALL(on_selected, Run(idp_url, kAccountId, true)).Times(1);

  // ShowAccountsDialog should return true and trigger the callback immediately
  // without showing UI.
  EXPECT_TRUE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne,
                                /*iframe_for_display=*/u""),
      {idp_data}, accounts, /*filtered_accounts=*/{},
      blink::mojom::RpMode::kActive, on_selected.Get(),
      /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/base::DoNothing(),
      /*accounts_displayed_callback=*/base::DoNothing()));
}

TEST_F(IdentityDialogControllerTest,
       TokenReceivedCallbackInvokedWhenAccountNotSignedInOrMissing) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  // Case 1: Account is missing.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountNotLoggedIn))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    // Create an account with different ID.
    std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
    accounts[0]->id = "other_account";
    IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, accounts, /*filtered_accounts=*/{},
        blink::mojom::RpMode::kActive,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(),
        /*dismiss_callback=*/base::DoNothing(),
        /*accounts_displayed_callback=*/base::DoNothing()));
  }

  // Case 2: Account exists but is not signed in.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountIsSignUp))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
    // Ensure account matches.
    accounts[0]->id = account_id;
    // Ensure not signed in.
    accounts[0]->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignUp;
    accounts[0]->browser_trusted_login_state =
        content::IdentityRequestAccount::LoginState::kSignUp;

    IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, accounts, /*filtered_accounts=*/{},
        blink::mojom::RpMode::kActive,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(),
        /*dismiss_callback=*/base::DoNothing(),
        /*accounts_displayed_callback=*/base::DoNothing()));
  }

  // Case 3: Account is filtered out.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountNotAvailable))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    // Create an account with matching ID but in filtered_accounts list.
    std::vector<IdentityRequestAccountPtr> filtered_accounts = CreateAccount();
    filtered_accounts[0]->id = account_id;
    filtered_accounts[0]->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;
    filtered_accounts[0]->browser_trusted_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;

    IdentityProviderDataPtr idp_data =
        CreateIdentityProviderData(filtered_accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowAccountsDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        {idp_data}, /*accounts=*/{}, filtered_accounts,
        blink::mojom::RpMode::kActive,
        /*on_selected=*/base::DoNothing(),
        /*on_add_account=*/base::DoNothing(),
        /*dismiss_callback=*/base::DoNothing(),
        /*accounts_displayed_callback=*/base::DoNothing()));
  }

  // Case 4: ShowFailureDialog with filtered_accounts.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        result_callback;
    EXPECT_CALL(result_callback,
                Run(content::webid::FederatedLoginResult::kAccountNotAvailable))
        .Times(1);

    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, result_callback.Get());

    // Create an account with matching ID but in filtered_accounts list.
    std::vector<IdentityRequestAccountPtr> filtered_accounts = CreateAccount();
    filtered_accounts[0]->id = account_id;
    filtered_accounts[0]->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;
    filtered_accounts[0]->browser_trusted_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;

    IdentityProviderDataPtr idp_data =
        CreateIdentityProviderData(filtered_accounts);
    idp_data->idp_metadata.config_url = idp_url;

    EXPECT_FALSE(controller->ShowFailureDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne,
                                  /*iframe_for_display=*/u""),
        kIdpEtldPlusOne, blink::mojom::RpContext::kSignIn,
        blink::mojom::RpMode::kActive, idp_data->idp_metadata,
        filtered_accounts,
        /*dismiss_callback=*/base::DoNothing(),
        /*login_callback=*/base::DoNothing()));
  }
}

TEST_F(IdentityDialogControllerTest, EmbedderNotifiedOfContinuation) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();

  // 1. Set task ID. Should call SetCanShowUi(false).
  EXPECT_CALL(*mock_view_ptr, SetCanShowUi(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  controller->SetActingTaskIdForTesting(actor::TaskId::FromUnsafeValue(1));

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  accounts_ = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);

  // 2. ShowFailureDialog notifies kAccountNotLoggedIn.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        callback;
    EXPECT_CALL(callback,
                Run(content::webid::FederatedLoginResult::kAccountNotLoggedIn))
        .Times(1);
    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, callback.Get());

    EXPECT_CALL(*mock_view_ptr, ShowFailureDialog)
        .WillOnce(testing::Return(true));
    controller->ShowFailureDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne, u""), kIdpEtldPlusOne,
        blink::mojom::RpContext::kSignIn, blink::mojom::RpMode::kActive,
        content::IdentityProviderMetadata(), {}, base::DoNothing(),
        base::DoNothing());
  }

  // 3. ShowErrorDialog notifies kIdpReturnedError.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        callback;
    EXPECT_CALL(callback,
                Run(content::webid::FederatedLoginResult::kIdpReturnedError))
        .Times(1);
    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, callback.Get());

    // ShowErrorDialog not triggered in active mode when there is an embedder
    // login request.
    EXPECT_CALL(*mock_view_ptr, ShowErrorDialog).Times(0);
    controller->ShowErrorDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne, u""), kIdpEtldPlusOne,
        blink::mojom::RpContext::kSignIn, blink::mojom::RpMode::kActive,
        content::IdentityProviderMetadata(), std::nullopt, base::DoNothing(),
        base::DoNothing());
  }

  // 4. ShowLoadingDialog does NOT notify on success.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        callback;
    EXPECT_CALL(callback, Run).Times(0);
    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, callback.Get());

    // ShowLoadingDialog not triggered in active mode when there is an embedder
    // login request.
    EXPECT_CALL(*mock_view_ptr, ShowLoadingDialog).Times(0);
    controller->ShowLoadingDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne, u""), kIdpEtldPlusOne,
        blink::mojom::RpContext::kSignIn, blink::mojom::RpMode::kActive,
        base::DoNothing());
  }

  // 5. ShowVerifyingDialog does NOT notify on success.
  {
    base::MockCallback<
        base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
        callback;
    EXPECT_CALL(callback, Run).Times(0);
    content::webid::FederatedEmbedderLoginRequest::Set(
        web_contents(), idp_origin, account_id, callback.Get());

    // ShowVerifyingDialog not triggered in active mode when there is an
    // embedder login request.
    EXPECT_CALL(*mock_view_ptr, ShowVerifyingDialog).Times(0);
    controller->ShowVerifyingDialog(
        content::RelyingPartyData(kTopFrameEtldPlusOne, u""), idp_data,
        accounts_[0], content::IdentityRequestAccount::SignInMode::kExplicit,
        blink::mojom::RpMode::kActive, base::DoNothing());
  }
}

TEST_F(IdentityDialogControllerTest, ActorLoginContinuation) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  controller->SetAccountSelectionViewForTesting(
      std::make_unique<MockAccountSelectionView>());

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  // Test that showing modal dialog results in kContinuation.
  base::MockCallback<
      base::RepeatingCallback<void(content::webid::FederatedLoginResult)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(content::webid::FederatedLoginResult::kContinuation))
      .Times(1);

  content::webid::FederatedEmbedderLoginRequest::Set(
      web_contents(), idp_origin, account_id, result_callback.Get());

  controller->ShowModalDialog(GURL("https://idp.example/login"),
                              blink::mojom::RpMode::kActive, base::DoNothing(),
                              base::DoNothing());
}

class IdentityDialogControllerTestWithOptimizationDisabled
    : public IdentityDialogControllerTest {
 public:
  IdentityDialogControllerTestWithOptimizationDisabled() {
    list.InitWithFeatures(
        /*enabled_features=*/{segmentation_platform::features::
                                  kSegmentationPlatformFedCmUser},
        /*disabled_features=*/{
            optimization_guide::features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList list;
};

// Tests that there is no crash if kSegmentationPlatformFedCmUser is enabled but
// kOptimizationHints is disabled. See crbug.com/435613236.
TEST_F(IdentityDialogControllerTestWithOptimizationDisabled, NoCrash) {
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

TEST_F(IdentityDialogControllerTest,
       PassiveAccountSelectionWithEmbedderLoginRequest) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));

  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  content::webid::FederatedEmbedderLoginRequest::Set(
      web_contents(), idp_origin, kAccountId, base::DoNothing());

  accounts_ = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);
  idp_data->idp_metadata.config_url = idp_url;

  // Should auto select and not show UI.
  EXPECT_CALL(*mock_view_ptr, Show).Times(0);

  base::MockCallback<AccountSelectionCallback> on_selected;
  EXPECT_CALL(on_selected, Run(idp_url, kAccountId, true)).Times(1);

  EXPECT_TRUE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne,
                                /*iframe_for_display=*/u""),
      {idp_data}, accounts_, /*filtered_accounts=*/{},
      blink::mojom::RpMode::kPassive, on_selected.Get(),
      /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/base::DoNothing(),
      /*accounts_displayed_callback=*/base::DoNothing()));
}

TEST_F(IdentityDialogControllerTest, ActiveModeGuardedByActorTask) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();

  // 1. Set task ID. Should call SetCanShowUi(false).
  EXPECT_CALL(*mock_view_ptr, SetCanShowUi(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  controller->SetActingTaskIdForTesting(actor::TaskId::FromUnsafeValue(1));

  accounts_ = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);

  // 2. ShowAccountsDialog should NOT show UI and return false in active mode.
  EXPECT_CALL(*mock_view_ptr, Show).Times(0);
  base::MockCallback<AccountSelectionCallback> on_selected;
  EXPECT_CALL(on_selected, Run).Times(0);

  EXPECT_FALSE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne, u""), {idp_data},
      accounts_, {}, blink::mojom::RpMode::kActive, on_selected.Get(),
      base::DoNothing(), base::DoNothing(), base::DoNothing()));

  // 3. ShowVerifyingDialog should NOT show UI and return true in active mode.
  EXPECT_CALL(*mock_view_ptr, ShowVerifyingDialog).Times(0);
  EXPECT_TRUE(controller->ShowVerifyingDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne, u""), idp_data,
      accounts_[0], content::IdentityRequestAccount::SignInMode::kExplicit,
      blink::mojom::RpMode::kActive, base::DoNothing()));
}

TEST_F(IdentityDialogControllerTest, ShowModalDialogNotGuardedByActorTask) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();

  // 1. Set task ID. Should call SetCanShowUi(false).
  EXPECT_CALL(*mock_view_ptr, SetCanShowUi(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  controller->SetActingTaskIdForTesting(actor::TaskId::FromUnsafeValue(1));

  // 2. Should STILL show modal UI.
  EXPECT_CALL(*mock_view_ptr, ShowModalDialog)
      .WillOnce(testing::Return(nullptr));

  controller->ShowModalDialog(GURL("https://idp.example/login"),
                              blink::mojom::RpMode::kActive, base::DoNothing(),
                              base::DoNothing());
}

TEST_F(IdentityDialogControllerTest, PassiveModeNotGuardedByActorTask) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();

  // 1. Set task ID. Should call SetCanShowUi(false).
  EXPECT_CALL(*mock_view_ptr, SetCanShowUi(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  controller->SetActingTaskIdForTesting(actor::TaskId::FromUnsafeValue(1));

  accounts_ = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);

  // 2. ShowAccountsDialog should STILL show UI in passive mode.
  EXPECT_CALL(*mock_view_ptr, Show).WillOnce(testing::Return(true));
  base::MockCallback<AccountSelectionCallback> on_selected;
  EXPECT_CALL(on_selected, Run).Times(0);

  EXPECT_TRUE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne, u""), {idp_data},
      accounts_, {}, blink::mojom::RpMode::kPassive, on_selected.Get(),
      base::DoNothing(), base::DoNothing(), base::DoNothing()));

  // 3. ShowVerifyingDialog should STILL show UI in passive mode.
  EXPECT_CALL(*mock_view_ptr, ShowVerifyingDialog)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(controller->ShowVerifyingDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne, u""), idp_data,
      accounts_[0], content::IdentityRequestAccount::SignInMode::kExplicit,
      blink::mojom::RpMode::kPassive, base::DoNothing()));
}

TEST_F(IdentityDialogControllerTest, ActiveModeDismissedWhenActorStopsActing) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents());
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* mock_view_ptr = mock_view.get();

  // 1. Simulate an active actor task.
  EXPECT_CALL(*mock_view_ptr, SetCanShowUi(false));
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));
  controller->SetActingTaskIdForTesting(actor::TaskId::FromUnsafeValue(1));

  accounts_ = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts_);

  base::MockCallback<DismissCallback> dismiss_callback;
  // 2. Call ShowAccountsDialog in active mode while task is active.
  // It should return false and not show any UI.
  EXPECT_CALL(*mock_view_ptr, Show).Times(0);
  EXPECT_FALSE(controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne, u""), {idp_data},
      accounts_, {}, blink::mojom::RpMode::kActive, base::DoNothing(),
      base::DoNothing(), dismiss_callback.Get(), base::DoNothing()));

  // 3. Simulate the actor task finishing.
  // This should trigger the dismiss callback because we are in active mode
  // and we previously suppressed the UI.
  EXPECT_CALL(*mock_view_ptr, SetCanShowUi(true));
  EXPECT_CALL(dismiss_callback,
              Run(IdentityDialogController::DismissReason::kOther))
      .Times(1);
  controller->SetActingTaskIdForTesting(actor::TaskId());
}
