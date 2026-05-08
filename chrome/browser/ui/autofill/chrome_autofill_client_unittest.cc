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
#include "build/build_config.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_enablement_service_factory.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
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
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service.h"
#include "components/accessibility_annotator/core/accessibility_annotator_types.h"
#include "components/autofill/content/browser/autofill_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
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
#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#else
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
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
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

class MockAccessibilityAnnotatorEnablementService
    : public accessibility_annotator::AccessibilityAnnotatorEnablementService {
 public:
  MockAccessibilityAnnotatorEnablementService() = default;
  ~MockAccessibilityAnnotatorEnablementService() override = default;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(accessibility_annotator::RemoteAnnotatorEnablementState,
              GetEnablementState,
              (),
              (override));
};

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
       bool,
       std::optional<
           payments::PaymentsAutofillClient::OnConfirmationClosedCallback>),
      (override));
  MOCK_METHOD(void, HideSaveCardBubble, (), (override));
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

  void InitializeAccessibilityAnnotatorEnablementService() {
    accessibility_annotator_enablement_service_ =
        static_cast<MockAccessibilityAnnotatorEnablementService*>(
            AccessibilityAnnotatorEnablementServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockAccessibilityAnnotatorEnablementService>();
                    })));
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
    accessibility_annotator_enablement_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestChromeAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  ContentAutofillDriver* driver(content::RenderFrameHost* rfh) {
    return ContentAutofillDriver::GetForRenderFrameHost(rfh);
  }

  MockAccessibilityAnnotatorEnablementService*
  accessibility_annotator_enablement_service() {
    return accessibility_annotator_enablement_service_;
  }

#if !BUILDFLAG(IS_ANDROID)
  MockAutofillFieldPromoController* autofill_field_promo_controller() {
    return autofill_field_promo_controller_;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

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
        base::BindRepeating(&CreateTestPersonalDataManager)}};
  }

  static std::unique_ptr<KeyedService> CreateTestPersonalDataManager(
      content::BrowserContext* context) {
    auto pdm = std::make_unique<TestPersonalDataManager>();
    pdm->test_address_data_manager().SetAutofillProfileEnabled(true);
    pdm->test_payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
    pdm->test_payments_data_manager().SetAutofillWalletImportEnabled(false);
    return pdm;
  }

  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockAutofillFieldPromoController> autofill_field_promo_controller_;
#endif  // !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockAccessibilityAnnotatorEnablementService>
      accessibility_annotator_enablement_service_;
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

#if !BUILDFLAG(IS_ANDROID)
// Test the scenario when the plus address survey delay is not configured. The
// random delay of the survey should be between the 10s and 60s.

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
                  /*card_saved=*/true, /*is_for_save_and_fill=*/true,
                  A<std::optional<payments::PaymentsAutofillClient::
                                      OnConfirmationClosedCallback>>()));
  client()->GetPaymentsAutofillClient()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      /*on_confirmation_closed_callback=*/std::nullopt);
}

TEST_F(ChromeAutofillClientTest,
       CreditCardUploadCompleted_ShowConfirmationBubbleView_CardNotSaved) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(
                  /*card_saved=*/false, /*is_for_save_and_fill=*/false,
                  A<std::optional<payments::PaymentsAutofillClient::
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
                  /*card_saved=*/false, /*is_for_save_and_fill=*/false,
                  A<std::optional<payments::PaymentsAutofillClient::
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
    AddTab(browser(), chrome::ChromeUINewTabURLAsGURL());
  }

  void TearDown() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
    BrowserWithTestWindowTest::TearDown();
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

  glic::MockGlicKeyedService* SetUpMockGlicKeyedService() {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
    glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(
            [](glic::GlicProfileManager* glic_profile_manager,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              Profile* profile = Profile::FromBrowserContext(context);
              return std::make_unique<glic::MockGlicKeyedService>(
                  context, IdentityManagerFactory::GetForProfile(profile),
                  TestingBrowserProcess::GetGlobal()->profile_manager(),
                  glic_profile_manager,
                  /*contextual_cueing_service=*/nullptr,
                  /*actor_keyed_service=*/nullptr);
            },
            &glic_profile_manager_));
    return static_cast<glic::MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                           /*create=*/true));
  }

 private:
  TestAutofillClientInjector<TestChromeAutofillClient>
      test_autofill_client_injector_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
  glic::GlicProfileManager glic_profile_manager_;
};

TEST_F(ChromeAutofillClientTestWithWindow, AutofillFieldIPH_NotifyFeatureUsed) {
  EXPECT_CALL(*user_education(),
              NotifyFeaturePromoFeatureUsed(
                  Ref(feature_engagement::kIPHAutofillAiOptInFeature),
                  FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
  client()->NotifyIphFeatureUsed(AutofillClient::IphFeature::kAutofillAi);
}

// Tests that `OpenGeminiInSidebar` invokes Glic with the correct options and
// prompt.
TEST_F(ChromeAutofillClientTestWithWindow, OpenGeminiInSidebar) {
  glic::MockGlicKeyedService* mock_glic_service = SetUpMockGlicKeyedService();
  ASSERT_TRUE(mock_glic_service);

  // We expect that the glic service is invoked with kAutofill as the invocation
  // source and containing the correct prompt.
  EXPECT_CALL(
      *mock_glic_service,
      Invoke(AllOf(Property(&glic::GlicInvokeOptions::GetInvocationSource,
                            glic::mojom::InvocationSource::kAutofill),
                   Field(&glic::GlicInvokeOptions::prompts,
                         testing::ElementsAre("test prompt")))))
      .WillOnce(testing::Return(base::WeakPtr<glic::GlicInstance>()));

  client()->OpenGeminiInSidebar(u"test prompt");
}
#endif

// Tests that if there is no enablement service available to the profile, client
// defaults to kDisabledNotEligible state.
TEST_F(ChromeAutofillClientTest,
       GetAccessibilityAnnotatorEnablementState_NoService) {
  EXPECT_EQ(client()->GetAccessibilityAnnotatorEnablementState(),
            accessibility_annotator::RemoteAnnotatorEnablementState::
                kDisabledNotEligible);
}

// Tests that the client correctly pipes the state from the enablement service.
TEST_F(ChromeAutofillClientTest,
       GetAccessibilityAnnotatorEnablementState_HappyPath) {
  InitializeAccessibilityAnnotatorEnablementService();

  EXPECT_CALL(*accessibility_annotator_enablement_service(),
              GetEnablementState())
      .WillRepeatedly(Return(
          accessibility_annotator::RemoteAnnotatorEnablementState::kEnabled));
  EXPECT_EQ(client()->GetAccessibilityAnnotatorEnablementState(),
            accessibility_annotator::RemoteAnnotatorEnablementState::kEnabled);
}

}  // namespace
}  // namespace autofill
