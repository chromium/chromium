// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/autofill_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#else
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "components/feature_engagement/test/mock_tracker.h"  // nogncheck
#endif

namespace autofill {
namespace {

using test::CreateFormDataForRenderFrameHost;
using test::CreateTestFormField;
using ::testing::_;
using ::testing::A;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using user_education::test::MockFeaturePromoController;

#if BUILDFLAG(IS_ANDROID)
class MockAutofillSaveCardBottomSheetBridge
    : public AutofillSaveCardBottomSheetBridge {
 public:
  MockAutofillSaveCardBottomSheetBridge()
      : AutofillSaveCardBottomSheetBridge(
            base::android::ScopedJavaGlobalRef<jobject>(nullptr)) {}

  MOCK_METHOD(void,
              RequestShowContent,
              (const AutofillSaveCardUiInfo&,
               std::unique_ptr<AutofillSaveCardDelegateAndroid>),
              (override));
};
#else
class MockSaveCardBubbleController : public SaveCardBubbleControllerImpl {
 public:
  explicit MockSaveCardBubbleController(content::WebContents* web_contents)
      : SaveCardBubbleControllerImpl(web_contents) {}
  ~MockSaveCardBubbleController() override = default;

  MOCK_METHOD(
      void,
      ShowConfirmationBubbleView,
      (bool,
       std::optional<
           payments::PaymentsAutofillClient::OnConfirmationClosedCallback>),
      (override));
  MOCK_METHOD(void, HideSaveCardBubble, (), (override));
};
#endif

class MockAutofillFieldPromoController : public AutofillFieldPromoController {
 public:
  ~MockAutofillFieldPromoController() override = default;
  MOCK_METHOD(void, Show, (const gfx::RectF&), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, IsMaybeShowing, (), (const override));
  MOCK_METHOD(const base::Feature&, GetFeaturePromo, (), (const override));
};

class TestChromeAutofillClient : public ChromeAutofillClient {
 public:
  explicit TestChromeAutofillClient(content::WebContents* web_contents)
      : ChromeAutofillClient(web_contents) {}
  ~TestChromeAutofillClient() override = default;

#if BUILDFLAG(IS_ANDROID)
  MockFastCheckoutClient* GetFastCheckoutClient() override {
    return &fast_checkout_client_;
  }

  // Inject a new MockAutofillSaveCardBottomSheetBridge.
  // Returns a pointer to the mock.
  MockAutofillSaveCardBottomSheetBridge*
  InjectMockAutofillSaveCardBottomSheetBridge() {
    auto mock = std::make_unique<MockAutofillSaveCardBottomSheetBridge>();
    auto* pointer = mock.get();
    GetPaymentsAutofillClient()->SetAutofillSaveCardBottomSheetBridgeForTesting(
        std::move(mock));
    return pointer;
  }

  MockFastCheckoutClient fast_checkout_client_;
#endif
};

class ChromeAutofillClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeAutofillClientTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PreparePersonalDataManager();
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));

    auto autofill_field_promo_controller_manual_fallback =
        std::make_unique<MockAutofillFieldPromoController>();
    autofill_field_promo_controller_manual_fallback_ =
        autofill_field_promo_controller_manual_fallback.get();
    ON_CALL(*autofill_field_promo_controller_manual_fallback_, GetFeaturePromo)
        .WillByDefault(
            ReturnRef(feature_engagement::kIPHAutofillManualFallbackFeature));
    client()->SetAutofillFieldPromoControllerManualFallbackForTesting(
        std::move(autofill_field_promo_controller_manual_fallback));
#if !BUILDFLAG(IS_ANDROID)
    ChromeSecurityStateTabHelper::CreateForWebContents(web_contents());

    auto save_card_bubble_controller =
        std::make_unique<MockSaveCardBubbleController>(web_contents());
    web_contents()->SetUserData(save_card_bubble_controller->UserDataKey(),
                                std::move(save_card_bubble_controller));
#endif
  }

  void TearDown() override {
    // Avoid that the raw pointer becomes dangling.
    personal_data_manager_ = nullptr;
    autofill_field_promo_controller_manual_fallback_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestChromeAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  ContentAutofillDriver* driver(content::RenderFrameHost* rfh) {
    return ContentAutofillDriver::GetForRenderFrameHost(rfh);
  }

  TestPersonalDataManager* personal_data_manager() {
    return personal_data_manager_;
  }

  MockAutofillFieldPromoController* autofill_field_promo_controller() {
    return autofill_field_promo_controller_manual_fallback_;
  }

#if !BUILDFLAG(IS_ANDROID)
  MockSaveCardBubbleController& save_card_bubble_controller() {
    return static_cast<MockSaveCardBubbleController&>(
        *SaveCardBubbleControllerImpl::FromWebContents(web_contents()));
  }
#endif

 private:
  void PreparePersonalDataManager() {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                profile(), base::BindOnce([](content::BrowserContext*) {
                  return std::make_unique<TestPersonalDataManager>();
                }));

    personal_data_manager_->test_address_data_manager()
        .SetAutofillProfileEnabled(true);
    personal_data_manager_->test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);
    personal_data_manager_->test_payments_data_manager()
        .SetAutofillWalletImportEnabled(false);

    // Enable MSBB by default. If MSBB has been explicitly turned off, Fast
    // Checkout is not supported.
    profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  raw_ptr<TestPersonalDataManager> personal_data_manager_ = nullptr;
  raw_ptr<MockAutofillFieldPromoController>
      autofill_field_promo_controller_manual_fallback_;
  TestAutofillClientInjector<TestChromeAutofillClient>
      test_autofill_client_injector_;
  base::OnceCallback<void()> setup_flags_;
};

// Tests that `ClassifyAsPasswordForm()` correctly recognizes a login form on a
// single frame.
TEST_F(ChromeAutofillClientTest, ClassifiesLoginFormOnMainFrame) {
  constexpr char kUrl[] = "https://www.foo.com/login.html";

  NavigateAndCommit(GURL(kUrl));
  ContentAutofillDriver* autofill_driver = driver(main_rfh());
  ASSERT_TRUE(autofill_driver);

  FormData form = CreateFormDataForRenderFrameHost(
      *main_rfh(), {CreateTestFormField("Username", "username", "",
                                        FormControlType::kInputText),
                    CreateTestFormField("Password", "password", "",
                                        FormControlType::kInputPassword)});

  {
    TestAutofillManagerWaiter waiter(autofill_driver->GetAutofillManager(),
                                     {AutofillManagerEvent::kFormsSeen});
    autofill_driver->renderer_events().FormsSeen(/*updated_forms=*/{form},
                                                 /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait(/*num_awaiting_calls=*/1));
  }

  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = form.fields()[0].global_id()};
  EXPECT_EQ(client()->ClassifyAsPasswordForm(
                autofill_driver->GetAutofillManager(), form.global_id(),
                form.fields()[0].global_id()),
            expected);
}

// Tests that `ClassifyAsPasswordForm()` correctly recognizes a login form on
// a child frame.
TEST_F(ChromeAutofillClientTest, ClassifiesLoginFormOnChildFrame) {
  constexpr char kUrl1[] = "https://www.foo.com/login.html";
  constexpr char kUrl2[] = "https://www.foo.com/otp.html";

  NavigateAndCommit(GURL(kUrl1));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild(std::string("child"));
  child_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kUrl2), child_rfh);
  ContentAutofillClient* autofill_client =
      ContentAutofillClient::FromWebContents(web_contents());
  ASSERT_TRUE(autofill_client);
  ContentAutofillDriver* main_driver = driver(main_rfh());
  ContentAutofillDriver* child_driver = driver(child_rfh);
  ASSERT_TRUE(main_driver);
  ASSERT_TRUE(child_driver);

  FormData main_form = CreateFormDataForRenderFrameHost(
      *main_rfh(), {CreateTestFormField("Search", "search", "",
                                        FormControlType::kInputText)});
  FormData child_form = CreateFormDataForRenderFrameHost(
      *child_rfh, {CreateTestFormField("Username", "username", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Password", "password", "",
                                       FormControlType::kInputPassword)});

  // Ensure that the child frame is picked up as a child frame of `main_form`.
  {
    autofill::FrameTokenWithPredecessor child_frame_information;
    child_frame_information.token = child_form.host_frame();
    main_form.set_child_frames({child_frame_information});
  }

  {
    autofill::TestAutofillManagerWaiter waiter(
        main_driver->GetAutofillManager(),
        {autofill::AutofillManagerEvent::kFormsSeen});
    main_driver->renderer_events().FormsSeen(/*updated_forms=*/{main_form},
                                             /*removed_forms=*/{});
    child_driver->renderer_events().FormsSeen(/*updated_forms=*/{child_form},
                                              /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait(/*num_awaiting_calls=*/2));
  }

  // The form fields in the main frame do not form a valid password form.
  EXPECT_EQ(client()->ClassifyAsPasswordForm(main_driver->GetAutofillManager(),
                                             main_form.global_id(),
                                             main_form.fields()[0].global_id()),
            PasswordFormClassification());
  // The form fields in the child frame form a login form.
  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = child_form.fields()[0].global_id()};
  EXPECT_EQ(client()->ClassifyAsPasswordForm(
                main_driver->GetAutofillManager(), main_form.global_id(),
                child_form.fields()[0].global_id()),
            expected);
}

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_BelowMaxFlowTime) {
  base::TimeDelta below_max_flow_time = base::Minutes(10);

  FormInteractionsFlowId first_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  task_environment()->FastForwardBy(below_max_flow_time);

  EXPECT_EQ(first_interaction_flow_id,
            client()->GetCurrentFormInteractionsFlowId());
}

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_AboveMaxFlowTime) {
  base::TimeDelta above_max_flow_time = base::Minutes(21);

  FormInteractionsFlowId first_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  task_environment()->FastForwardBy(above_max_flow_time);

  EXPECT_NE(first_interaction_flow_id,
            client()->GetCurrentFormInteractionsFlowId());
}

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_AdvancedTwice) {
  base::TimeDelta above_half_max_flow_time = base::Minutes(15);

  FormInteractionsFlowId first_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  task_environment()->FastForwardBy(above_half_max_flow_time);

  FormInteractionsFlowId second_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  task_environment()->FastForwardBy(above_half_max_flow_time);

  EXPECT_EQ(first_interaction_flow_id, second_interaction_flow_id);
  EXPECT_NE(first_interaction_flow_id,
            client()->GetCurrentFormInteractionsFlowId());
}

// Ensure that, by default, the plus address service is not available.
// The positive case (feature enabled) will be tested in plus_addresses browser
// tests; this test is intended to ensure the default state does not behave
// unexpectedly.
TEST_F(ChromeAutofillClientTest,
       PlusAddressDefaultFeatureStateMeansNullPlusAddressService) {
  PlusAddressServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
  EXPECT_EQ(client()->GetPlusAddressDelegate(), nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
// Test that the hats service is called with the expected params for different
// surveys. Note that Surveys are only launched on Desktop.
TEST_F(ChromeAutofillClientTest, TriggerUserPerceptionOfAutofillAddressSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  SurveyBitsData expected_bits = {{"granular filling available", false}};
  const SurveyStringData field_filling_stats_data;
  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerAutofillAddressUserPerception, _, _,
                  expected_bits, Ref(field_filling_stats_data), _, _, _, _, _));

  client()->TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kAddress,
                                                  field_filling_stats_data);
}

TEST_F(ChromeAutofillClientTest,
       TriggerUserPerceptionOfAutofillCreditCardSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  const SurveyStringData field_filling_stats_data;
  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerAutofillCreditCardUserPerception, _, _, _,
                  Ref(field_filling_stats_data), _, _, _, _, _));

  client()->TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kCreditCard,
                                                  field_filling_stats_data);
}

TEST_F(ChromeAutofillClientTest,
       CreditCardUploadCompleted_ShowConfirmationBubbleView_CardSaved) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(
                  true, A<std::optional<payments::PaymentsAutofillClient::
                                            OnConfirmationClosedCallback>>()));
  client()->GetPaymentsAutofillClient()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*on_confirmation_closed_callback=*/std::nullopt);
}

TEST_F(ChromeAutofillClientTest,
       CreditCardUploadCompleted_ShowConfirmationBubbleView_CardNotSaved) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(
                  false, A<std::optional<payments::PaymentsAutofillClient::
                                             OnConfirmationClosedCallback>>()));
  client()->GetPaymentsAutofillClient()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      /*on_confirmation_closed_callback=*/std::nullopt);
}

// Test that on getting client-side timeout, save card dialog is dismissed and
// confirmation dialog is not shown.
TEST_F(ChromeAutofillClientTest,
       CreditCardUploadCompleted_NoConfirmationBubbleView_OnRequestTimeout) {
  EXPECT_CALL(save_card_bubble_controller(), HideSaveCardBubble());
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(
                  false, A<std::optional<payments::PaymentsAutofillClient::
                                             OnConfirmationClosedCallback>>()))
      .Times(0);
  client()->GetPaymentsAutofillClient()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout,
      /*on_confirmation_closed_callback=*/std::nullopt);
}

TEST_F(ChromeAutofillClientTest, EditAddressDialogFooter) {
  EditAddressProfileDialogControllerImpl::CreateForWebContents(web_contents());
  auto* controller =
      EditAddressProfileDialogControllerImpl::FromWebContents(web_contents());
  controller->SetViewFactoryForTest(base::BindRepeating(
      [](content::WebContents*, EditAddressProfileDialogController*) {
        return static_cast<AutofillBubbleBase*>(nullptr);
      }));

  // Non-account profile
  client()->ShowEditAddressProfileDialog(test::GetFullProfile(),
                                         base::DoNothing());
  EXPECT_EQ(controller->GetFooterMessage(), u"");

  // Account profile
  AutofillProfile profile2 = test::GetFullProfile();
  test_api(profile2).set_record_type(AutofillProfile::RecordType::kAccount);
  client()->ShowEditAddressProfileDialog(profile2, base::DoNothing());
  std::optional<AccountInfo> account = GetPrimaryAccountInfoFromBrowserContext(
      web_contents()->GetBrowserContext());
  EXPECT_EQ(controller->GetFooterMessage(),
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE,
                base::ASCIIToUTF16(account->email)));
}

TEST_F(ChromeAutofillClientTest, AutofillManualFallbackIPH_IsShown) {
  EXPECT_CALL(*autofill_field_promo_controller(), Show);
  client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kManualFallback);
}

TEST_F(ChromeAutofillClientTest,
       AutofillManualFallbackIPH_HideOnShowAutofillSuggestions) {
  auto delegate = std::make_unique<MockAutofillSuggestionDelegate>();

  EXPECT_CALL(*autofill_field_promo_controller(), Hide);
  client()->ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                    delegate->GetWeakPtr());

  // Showing the Autofill Popup is an asynchronous task.
  task_environment()->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(autofill_field_promo_controller());
}

TEST_F(ChromeAutofillClientTest, AutofillManualFallbackIPH_NotifyFeatureUsed) {
  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return std::make_unique<feature_engagement::test::MockTracker>();
      }));

  EXPECT_CALL(
      *static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(profile())),
      NotifyUsedEvent);
  client()->NotifyAutofillManualFallbackUsed();
}
#endif
}  // namespace
}  // namespace autofill
