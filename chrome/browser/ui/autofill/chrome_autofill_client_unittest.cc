// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/autofill/content/browser/autofill_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/fast_checkout/mock_fast_checkout_client.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/core/browser/fake_plus_address_service.h"
#include "components/plus_addresses/core/browser/plus_address_hats_utils.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"
#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#else
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#endif

namespace autofill {
namespace {

using ::autofill::test::CreateFormDataForRenderFrameHost;
using ::autofill::test::CreateTestFormField;
using ::testing::_;
using ::testing::A;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::Le;
using ::testing::Pair;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

#if !BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
class MockAutofillAccessibilityHelper : public AutofillAccessibilityHelper {
 public:
  MockAutofillAccessibilityHelper() = default;
  ~MockAutofillAccessibilityHelper() override = default;

  MOCK_METHOD(void,
              AnnounceTextForA11y,
              (const std::u16string& message),
              (override));
};
#endif

#if !BUILDFLAG(IS_ANDROID)
class MockAutofillFieldPromoController : public AutofillFieldPromoController {
 public:
  ~MockAutofillFieldPromoController() override = default;
  MOCK_METHOD(void, Show, (const gfx::RectF&), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, IsMaybeShowing, (), (const override));
  MOCK_METHOD(const base::Feature&, GetFeaturePromo, (), (const override));
};
#endif  // !BUILDFLAG(IS_ANDROID)

// This test class is needed to make the constructor public.
class TestChromeAutofillClient : public ChromeAutofillClient {
 public:
  explicit TestChromeAutofillClient(content::WebContents* web_contents)
      : ChromeAutofillClient(web_contents) {}
  ~TestChromeAutofillClient() override = default;
};

class ChromeAutofillClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeAutofillClientTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Enable MSBB by default. If MSBB has been explicitly turned off, Fast
    // Checkout is not supported.
    profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));

#if !BUILDFLAG(IS_ANDROID)
    ChromeSecurityStateTabHelper::CreateForWebContents(web_contents());

    auto save_card_bubble_controller =
        std::make_unique<MockSaveCardBubbleController>(web_contents());
    const auto* user_data_key = save_card_bubble_controller->UserDataKey();
    web_contents()->SetUserData(user_data_key,
                                std::move(save_card_bubble_controller));
#endif
  }

#if !BUILDFLAG(IS_ANDROID)
  void SetUpIphForTesting(const base::Feature& feature_promo) {
    auto autofill_field_promo_controller =
        std::make_unique<MockAutofillFieldPromoController>();
    autofill_field_promo_controller_ = autofill_field_promo_controller.get();
    ON_CALL(*autofill_field_promo_controller_, IsMaybeShowing)
        .WillByDefault(Return(false));
    ON_CALL(*autofill_field_promo_controller_, GetFeaturePromo)
        .WillByDefault(ReturnRef(feature_promo));
    client()->SetAutofillFieldPromoTesting(
        std::move(autofill_field_promo_controller));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  void TearDown() override {
    // Avoid that the raw pointer becomes dangling.
#if !BUILDFLAG(IS_ANDROID)
    autofill_field_promo_controller_ = nullptr;
#endif  // !BUILDFLAG(IS_ANDROID)
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestChromeAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  ContentAutofillDriver* driver(content::RenderFrameHost* rfh) {
    return ContentAutofillDriver::GetForRenderFrameHost(rfh);
  }

#if !BUILDFLAG(IS_ANDROID)
  MockAutofillFieldPromoController* autofill_field_promo_controller() {
    return autofill_field_promo_controller_;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  // Helper function to set up mock accessibility helper for Android tests.
  MockAutofillAccessibilityHelper* SetUpMockAccessibilityHelper() {
    mock_accessibility_helper_ =
        std::make_unique<MockAutofillAccessibilityHelper>();
    MockAutofillAccessibilityHelper* mock_ptr =
        mock_accessibility_helper_.get();
    AutofillAccessibilityHelper::SetInstanceForTesting(mock_ptr);
    return mock_ptr;
  }

  void TearDownMockAccessibilityHelper() {
    AutofillAccessibilityHelper::SetInstanceForTesting(nullptr);
    mock_accessibility_helper_.reset();
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  MockSaveCardBubbleController& save_card_bubble_controller() {
    return static_cast<MockSaveCardBubbleController&>(
        *SaveCardBubbleControllerImpl::FromWebContents(web_contents()));
  }
#endif

 private:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                autofill::PersonalDataManagerFactory::GetInstance(),
                base::BindRepeating(&CreateTestPersonalDataManager)},
            TestingProfile::TestingFactory{
                PlusAddressServiceFactory::GetInstance(),
                base::BindRepeating(&BuildFakePlusAddressService)}};
  }

  static std::unique_ptr<KeyedService> CreateTestPersonalDataManager(
      content::BrowserContext* context) {
    auto pdm = std::make_unique<TestPersonalDataManager>();
    pdm->test_address_data_manager().SetAutofillProfileEnabled(true);
    pdm->test_payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
    pdm->test_payments_data_manager().SetAutofillWalletImportEnabled(false);
    return pdm;
  }

  static std::unique_ptr<KeyedService> BuildFakePlusAddressService(
      content::BrowserContext* context) {
    return std::make_unique<plus_addresses::FakePlusAddressService>();
  }

  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  base::test::ScopedFeatureList scoped_feature_list_{
      plus_addresses::features::kPlusAddressesEnabled};
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockAutofillFieldPromoController> autofill_field_promo_controller_;
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MockAutofillAccessibilityHelper> mock_accessibility_helper_;
#endif
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
    ASSERT_TRUE(waiter.Wait(/*num_expected_relevant_events=*/1));
  }

  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = form.fields()[0].global_id(),
      .password_field = form.fields()[1].global_id()};
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
    ASSERT_TRUE(waiter.Wait(/*num_expected_relevant_events=*/2));
  }

  // The form fields in the main frame do not form a valid password form.
  EXPECT_EQ(client()->ClassifyAsPasswordForm(main_driver->GetAutofillManager(),
                                             main_form.global_id(),
                                             main_form.fields()[0].global_id()),
            PasswordFormClassification());
  // The form fields in the child frame form a login form.
  const auto expected = PasswordFormClassification{
      .type = PasswordFormClassification::Type::kLoginForm,
      .username_field = child_form.fields()[0].global_id(),
      .password_field = child_form.fields()[1].global_id()};
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      plus_addresses::features::kPlusAddressesEnabled);

  PlusAddressServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
  EXPECT_EQ(client()->GetPlusAddressDelegate(), nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
// Test the scenario when the plus address survey delay is not configured. The
// random delay of the survey should be between the 10s and 60s.
TEST_F(ChromeAutofillClientTest,
       TriggerPlusAddressUserPerceptionSurvey_DelayNotConfigured) {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPlusAddressAcceptedFirstTimeCreateSurvey};

  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate, _,
          AllOf(Ge(10000), Le(60000)), _,
          UnorderedElementsAre(
              Pair(plus_addresses::hats::kPlusAddressesCount, std::string("0")),
              Pair(plus_addresses::hats::kFirstPlusAddressCreationTime,
                   std::string("-1")),
              Pair(plus_addresses::hats::kLastPlusAddressFillingTime,
                   std::string("-1"))),
          HatsService::NavigationBehavior::ALLOW_ANY, _, _, _, _));

  client()->TriggerPlusAddressUserPerceptionSurvey(
      plus_addresses::hats::SurveyType::kAcceptedFirstTimeCreate);
}

// Test that the hats service is called with the expected params for different
// surveys.
TEST_F(ChromeAutofillClientTest, TriggerPlusAddressUserPerceptionSurvey) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::
                                 kPlusAddressAcceptedFirstTimeCreateSurvey,
                             {{plus_addresses::hats::kMinDelayMs, "10"},
                              {plus_addresses::hats::kMaxDelayMs, "60"}}}},
      /*disabled_features=*/{});

  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerPlusAddressAcceptedFirstTimeCreate, _,
          AllOf(Ge(10), Le(60)), _,
          UnorderedElementsAre(
              Pair(plus_addresses::hats::kPlusAddressesCount, std::string("0")),
              Pair(plus_addresses::hats::kFirstPlusAddressCreationTime,
                   std::string("-1")),
              Pair(plus_addresses::hats::kLastPlusAddressFillingTime,
                   std::string("-1"))),
          HatsService::NavigationBehavior::ALLOW_ANY, _, _, _, _));

  client()->TriggerPlusAddressUserPerceptionSurvey(
      plus_addresses::hats::SurveyType::kAcceptedFirstTimeCreate);
}

// Test that the hats service is called with the expected params for different
// surveys. Note that Surveys are only launched on Desktop.
TEST_F(ChromeAutofillClientTest, TriggerUserPerceptionOfAutofillAddressSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  const SurveyStringData field_filling_stats_data;
  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerAutofillAddressUserPerception, _, _, _,
                  Ref(field_filling_stats_data), _, _, _, _, _));

  client()->TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kAddress,
                                                  field_filling_stats_data);
}

// Test that the Autofill AI filling journey survey calls the hats service with
// the expected params.
TEST_F(ChromeAutofillClientTest,
       TriggerUserAutofillAiFillingJourneySurvey_Vehicle_SuggestionAccepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiFillingSurvey);

  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAiFilling, _, _,
          Eq(SurveyBitsData({{"User accepted suggestion", true}})),
          Eq(SurveyStringData({{"Entity type", "Vehicle"},
                               {"Saved entities", "Vehicle"},
                               {"Triggering field types", "NAME_FULL"}})),
          _, _, _, _, _));

  client()->TriggerAutofillAiFillingJourneySurvey(
      /*suggestion_accepted=*/true, EntityType(EntityTypeName::kVehicle),
      base::flat_set<EntityTypeName>({EntityTypeName::kVehicle}), {NAME_FULL});
}

// Test that some entities (such as passports) does not trigger AutofillAi
// filling surveys.
TEST_F(ChromeAutofillClientTest,
       TriggerUserAutofillAiFillingJourneySurvey_Passport_SurveyNotTriggered) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiFillingSurvey);

  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_hats_service, LaunchDelayedSurveyForWebContents).Times(0);

  client()->TriggerAutofillAiFillingJourneySurvey(
      /*suggestion_accepted=*/true, EntityType(EntityTypeName::kPassport),
      base::flat_set<EntityTypeName>({EntityTypeName::kPassport}),
      {PASSPORT_NUMBER});
}

TEST_F(
    ChromeAutofillClientTest,
    TriggerUserAutofillAiFillingJourneySurvey_FlightReservation_SuggestionDeclined) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiFillingSurvey);

  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(
      *mock_hats_service,
      LaunchDelayedSurveyForWebContents(
          kHatsSurveyTriggerAutofillAiFilling, _, _,
          Eq(SurveyBitsData({{"User accepted suggestion", false}})),
          Eq(SurveyStringData(
              {{"Entity type",
                std::string(EntityType(EntityTypeName::kFlightReservation)
                                .name_as_string())},
               {"Triggering field types", "FLIGHT_RESERVATION_FLIGHT_NUMBER"},
               {"Saved entities", "Passport,Flight Reservation"}})),
          _, _, _, _, _));

  client()->TriggerAutofillAiFillingJourneySurvey(
      /*suggestion_accepted=*/false,
      EntityType(EntityTypeName::kFlightReservation),
      base::flat_set<EntityTypeName>(
          {EntityTypeName::kPassport, EntityTypeName::kFlightReservation}),
      FieldTypeSet({FLIGHT_RESERVATION_FLIGHT_NUMBER}));
}

// Test that the Autofill AI save prompt survey calls the hats service with
// the expected params.
TEST_F(ChromeAutofillClientTest,
       TriggerUserAutofillAiSavePromptSurvey_Accepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kAutofillAiSavePromptSurvey,
                             {{"autofill_ai_walletable_entity_save_prompt_"
                               "survey_accepted_trigger_id",
                               "12345"}}}},
      /*disabled_features=*/{});
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerAutofillAiSavePrompt, _, _, _,
                  Eq(SurveyStringData({{"Entity type", "Vehicle"},
                                       {"Saved entities", "Vehicle"}})),
                  _, _, _, Eq("12345"), _));

  client()->TriggerAutofillAiSavePromptSurvey(
      /*prompt_accepted=*/true, EntityType(EntityTypeName::kVehicle),
      base::flat_set<EntityTypeName>({EntityTypeName::kVehicle}));
}

TEST_F(ChromeAutofillClientTest,
       TriggerUserAutofillAiSavePromptSurvey_Declined) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kAutofillAiSavePromptSurvey,
                             {{"autofill_ai_walletable_entity_save_prompt_"
                               "survey_declined_trigger_id",
                               "12345"}}}},
      /*disabled_features=*/{});

  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerAutofillAiSavePrompt, _, _, _,
                  Eq(SurveyStringData({{"Entity type", "Vehicle"},
                                       {"Saved entities", "Vehicle"}})),
                  _, _, _, Eq("12345"), _));

  client()->TriggerAutofillAiSavePromptSurvey(
      /*prompt_accepted=*/false, EntityType(EntityTypeName::kVehicle),
      base::flat_set<EntityTypeName>({EntityTypeName::kVehicle}));
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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeAutofillClientTest, AutofillFieldIPH_NotShownByPromoController) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiOptInFeature);

  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kAutofillAi));
}

TEST_F(ChromeAutofillClientTest, AutofillFieldIPH_IsShown) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiOptInFeature);

  InSequence sequence;
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(false));
  EXPECT_CALL(*autofill_field_promo_controller(), Show);
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(true));

  EXPECT_TRUE(client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kAutofillAi));
}

TEST_F(ChromeAutofillClientTest, AutofillImprovedPredictionsIPH_IsShown) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiOptInFeature);

  InSequence sequence;
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(false));
  EXPECT_CALL(*autofill_field_promo_controller(), Show);
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(true));

  EXPECT_TRUE(client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kAutofillAi));
}

TEST_F(ChromeAutofillClientTest,
       AutofillFieldIPH_HideOnShowAutofillSuggestions) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiOptInFeature);
  auto delegate = std::make_unique<MockAutofillSuggestionDelegate>();

  EXPECT_CALL(*autofill_field_promo_controller(), Hide);
  client()->ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                    delegate->GetWeakPtr());

  // Showing the Autofill Popup is an asynchronous task.
  task_environment()->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(autofill_field_promo_controller());
}
#endif  // !BUILDFLAG(IS_ANDROID)

class ChromeAutofillClientTestWithWindow : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    user_ed_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& window) {
                  return std::make_unique<MockBrowserUserEducationInterface>(
                      &window);
                }));

    BrowserWithTestWindowTest::SetUp();
    // Create the first tab so that `web_contents()` exists.
    AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  }

  MockBrowserUserEducationInterface* user_education() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser()));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestChromeAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

 private:
  TestAutofillClientInjector<TestChromeAutofillClient>
      test_autofill_client_injector_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
};

TEST_F(ChromeAutofillClientTestWithWindow, AutofillFieldIPH_NotifyFeatureUsed) {
  EXPECT_CALL(*user_education(),
              NotifyFeaturePromoFeatureUsed(
                  Ref(feature_engagement::kIPHAutofillAiOptInFeature),
                  FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
  client()->NotifyIphFeatureUsed(AutofillClient::IphFeature::kAutofillAi);
}
#endif

#if BUILDFLAG(IS_ANDROID)
// Test that TouchToFill credit card filling triggers accessibility
// announcement.
TEST_F(ChromeAutofillClientTest,
       DidFillForm_TouchToFillCreditCard_AnnouncesAccessibility) {
  MockAutofillAccessibilityHelper* mock_ptr = SetUpMockAccessibilityHelper();

  EXPECT_CALL(*mock_ptr, AnnounceTextForA11y(testing::_)).Times(1);

  client()->DidFillForm(AutofillTriggerSource::kTouchToFillCreditCard,
                        /*is_refill=*/false);

  TearDownMockAccessibilityHelper();
}

// Test that refill operations do not trigger accessibility announcements.
TEST_F(ChromeAutofillClientTest,
       DidFillForm_TouchToFillCreditCardRefill_NoAccessibilityAnnouncement) {
  MockAutofillAccessibilityHelper* mock_ptr = SetUpMockAccessibilityHelper();

  EXPECT_CALL(*mock_ptr, AnnounceTextForA11y(testing::_)).Times(0);

  client()->DidFillForm(AutofillTriggerSource::kTouchToFillCreditCard,
                        /*is_refill=*/true);

  TearDownMockAccessibilityHelper();
}

// Test that non-TouchToFill trigger sources do not make accessibility
// announcements.
TEST_F(ChromeAutofillClientTest,
       DidFillForm_OtherTriggerSource_NoAccessibilityAnnouncement) {
  MockAutofillAccessibilityHelper* mock_ptr = SetUpMockAccessibilityHelper();

  EXPECT_CALL(*mock_ptr, AnnounceTextForA11y(testing::_)).Times(0);

  client()->DidFillForm(AutofillTriggerSource::kPopup,
                        /*is_refill=*/false);

  TearDownMockAccessibilityHelper();
}

// Test that the correct localized accessibility message is announced.
TEST_F(ChromeAutofillClientTest, DidFillForm_VerifiesCorrectMessage) {
  MockAutofillAccessibilityHelper* mock_ptr = SetUpMockAccessibilityHelper();

  const std::u16string expected_message =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);

  EXPECT_CALL(*mock_ptr, AnnounceTextForA11y(expected_message)).Times(1);

  client()->DidFillForm(AutofillTriggerSource::kTouchToFillCreditCard,
                        /*is_refill=*/false);

  TearDownMockAccessibilityHelper();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
