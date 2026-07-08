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
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/at_memory_cross_tab_copy_paste_tracker_factory.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/personal_context/personal_context_enablement_service_factory.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
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
#include "components/autofill/core/browser/at_memory_cross_tab_copy_paste_tracker.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
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
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#endif  //   BUILDFLAG(IS_ANDROID)

namespace autofill {
namespace {

using ::autofill::test::CreateFormDataForRenderFrameHost;
using ::autofill::test::CreateTestFormField;
using ::testing::_;
using ::testing::A;
using ::testing::AllOf;
using ::testing::ElementsAre;
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

class MockPersonalContextEnablementService
    : public personal_context::PersonalContextEnablementService {
 public:
  MockPersonalContextEnablementService() = default;
  ~MockPersonalContextEnablementService() override = default;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(personal_context::PersonalContextEnablementState,
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
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  using ChromeAutofillClient::at_memory_copy_paste_observer;
#endif
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
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void InitializePersonalContextEnablementService() {
    personal_context_enablement_service_ =
        static_cast<MockPersonalContextEnablementService*>(
            PersonalContextEnablementServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockPersonalContextEnablementService>();
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
    personal_context_enablement_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestChromeAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  TestChromeAutofillClient* client(content::WebContents* web_contents) {
    return test_autofill_client_injector_[web_contents];
  }

  ContentAutofillDriver* driver(content::RenderFrameHost* rfh) {
    return ContentAutofillDriver::GetForRenderFrameHost(rfh);
  }

  MockPersonalContextEnablementService* personal_context_enablement_service() {
    return personal_context_enablement_service_;
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
#endif  // !BUILDFLAG(IS_ANDROID)

 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        PersonalDataManagerFactory::GetInstance(),
        base::BindRepeating(&CreateTestPersonalDataManager)}};
  }

 private:
  static std::unique_ptr<KeyedService> CreateTestPersonalDataManager(
      content::BrowserContext* context) {
    auto pdm = std::make_unique<TestPersonalDataManager>();
    pdm->test_address_data_manager().SetAutofillProfileEnabled(true);
    pdm->test_payments_data_manager().SetAutofillPaymentMethodsEnabled(true);
    pdm->test_payments_data_manager().SetAutofillWalletImportEnabled(false);
    return pdm;
  }

  test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockAutofillFieldPromoController> autofill_field_promo_controller_;
#endif  // !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockPersonalContextEnablementService>
      personal_context_enablement_service_;
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
    FrameTokenWithPredecessor child_frame_information;
    child_frame_information.token = child_form.host_frame();
    main_form.set_child_frames({child_frame_information});
  }

  {
    TestAutofillManagerWaiter waiter(main_driver->GetAutofillManager(),
                                     {AutofillManagerEvent::kFormsSeen});
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

class ChromeAutofillClientTestWithMockWindow : public ChromeAutofillClientTest {
 public:
  ChromeAutofillClientTestWithMockWindow() {
    scoped_feature_list_.InitAndEnableFeature(features::kAutofillActorMode);
    manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<MockAutofillManager>>();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories =
        ChromeAutofillClientTest::GetTestingFactories();
    // Register the fake actor service before any tabs are added, so that
    // any TabFeatures created (including the first tab) use the fake service
    // instead of creating a real one that gets destroyed later.
    factories.push_back(
        {actor::ActorKeyedServiceFactory::GetInstance(),
         base::BindRepeating([](content::BrowserContext* context)
                                 -> std::unique_ptr<KeyedService> {
           return std::make_unique<actor::ActorKeyedServiceFake>(
               Profile::FromBrowserContext(context));
         })});
    return factories;
  }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    ChromeAutofillClientTest::SetUp();

    // Register the testing profile in the profile attributes storage.
    ProfileAttributesInitParams params;
    params.profile_path = profile()->GetPath();
    params.profile_name = u"Test Profile";
    profile_manager_->profile_attributes_storage()->AddProfile(
        std::move(params));

    SetUpMockTabAndWindow(
        base::BindRepeating(
            &ChromeAutofillClientTestWithMockWindow::web_contents,
            base::Unretained(this)),
        profile(), main_mocks_);
  }

  void TearDown() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
    manager_injector_.reset();
    ChromeAutofillClientTest::TearDown();
    profile_manager_.reset();
  }

  tabs::MockTabInterface& mock_tab_interface() { return main_mocks_.mock_tab; }
  MockBrowserWindowInterface& mock_browser_window_interface() {
    return main_mocks_.mock_window;
  }
  ui::UnownedUserDataHost& unowned_user_data_host() {
    return main_mocks_.user_data_host;
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

 protected:
  struct TabAndWindowMocks {
    tabs::MockTabInterface mock_tab;
    MockBrowserWindowInterface mock_window;
    ui::UnownedUserDataHost user_data_host;
    base::WeakPtrFactory<tabs::MockTabInterface> mock_tab_weak_factory{
        &mock_tab};
  };

  void SetUpMockTabAndWindow(
      base::RepeatingCallback<content::WebContents*()> web_contents_callback,
      Profile* profile,
      TabAndWindowMocks& mocks) {
    ON_CALL(mocks.mock_tab, GetContents())
        .WillByDefault(
            [web_contents_callback]() { return web_contents_callback.Run(); });
    ON_CALL(mocks.mock_tab, GetProfile()).WillByDefault(Return(profile));
    ON_CALL(mocks.mock_tab, GetBrowserWindowInterface())
        .WillByDefault(Return(&mocks.mock_window));
    ON_CALL(mocks.mock_tab, GetTabHandle())
        .WillByDefault(Return(mocks.mock_tab.GetHandle().raw_value()));

    ON_CALL(mocks.mock_window, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(mocks.user_data_host));
    ON_CALL(mocks.mock_window, GetProfile()).WillByDefault(Return(profile));

    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents_callback.Run(), &mocks.mock_tab);

    ON_CALL(mocks.mock_tab, GetWeakPtr())
        .WillByDefault(Return(mocks.mock_tab_weak_factory.GetWeakPtr()));
  }

  void SetUpMockTabAndWindow(content::WebContents* web_contents,
                             Profile* profile,
                             TabAndWindowMocks& mocks) {
    SetUpMockTabAndWindow(
        base::BindRepeating([](content::WebContents* wc) { return wc; },
                            web_contents),
        profile, mocks);
  }
  std::unique_ptr<TestAutofillManagerInjector<MockAutofillManager>>
      manager_injector_;

 private:
  TabAndWindowMocks main_mocks_;
  glic::GlicProfileManager glic_profile_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeAutofillClientTestWithMockWindow,
       AutofillFieldIPH_NotifyFeatureUsed) {
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());

  EXPECT_CALL(mock_user_education,
              NotifyFeaturePromoFeatureUsed(
                  Ref(feature_engagement::kIPHAutofillAiOptInFeature),
                  FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
  client()->NotifyIphFeatureUsed(AutofillClient::IphFeature::kAutofillAi);
}

// Tests that `OpenGeminiInSidebar` invokes Glic with the correct options and
// prompt.
TEST_F(ChromeAutofillClientTestWithMockWindow, OpenGeminiInSidebar) {
  glic::MockGlicKeyedService* mock_glic_service = SetUpMockGlicKeyedService();
  ASSERT_TRUE(mock_glic_service);

  // We expect that the glic service is invoked with kAutofill as the invocation
  // source and containing the correct prompt.
  EXPECT_CALL(
      *mock_glic_service,
      Invoke(AllOf(
          Property(&glic::GlicInvokeOptions::GetInvocationSource,
                   glic::mojom::InvocationSource::kAutofill),
          Field(&glic::GlicInvokeOptions::prompts, ElementsAre("test prompt")),
          Field(&glic::GlicInvokeOptions::focus_on_show, true))))
      .WillOnce(testing::Return(base::WeakPtr<glic::GlicInstance>()));

  client()->OpenGeminiInSidebar(u"test prompt");
}

// Tests that `OnActorTaskStateChange` calls `ReparseKnownForms` on all drivers
// when a new task gets assigned.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       OnActorTaskStateChange_ReparseForms) {
  actor::ActorKeyedServiceFake* actor_service =
      static_cast<actor::ActorKeyedServiceFake*>(
          actor::ActorKeyedService::Get(profile()));
  ASSERT_TRUE(actor_service);

  MockAutofillManager* mock_manager = (*manager_injector_)[web_contents()];
  ASSERT_TRUE(mock_manager);

  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);

  // Associate the active tab with the task.
  task->AddTab(mock_tab_interface().GetHandle(),
               /*stop_task_on_detach=*/true, base::DoNothing());

  // Verify first call (assignment) triggers `ReparseKnownForms`.
  EXPECT_CALL(*mock_manager, ReparseKnownForms()).Times(1);
  actor_service->NotifyTaskStateChanged(*task);
  testing::Mock::VerifyAndClearExpectations(mock_manager);

  // Verify second call (no new task) does NOT trigger ReparseKnownForms
  EXPECT_CALL(*mock_manager, ReparseKnownForms()).Times(0);
  actor_service->NotifyTaskStateChanged(*task);
  testing::Mock::VerifyAndClearExpectations(mock_manager);
}

#endif  //  !BUILDFLAG(IS_ANDROID)

#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
     BUILDFLAG(IS_CHROMEOS)) &&                                       \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that `ShowAutofillAtMemoryPromo` is propagated to the browser user
// education service when AtMemory is enabled.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       ShowAutofillAtMemoryPromo_Enabled) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);
  InitializePersonalContextEnablementService();
  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());
  EXPECT_CALL(mock_user_education,
              MaybeShowFeaturePromo(testing::Truly(
                  [](const user_education::FeaturePromoParams& params) {
                    return &*params.feature ==
                           &feature_engagement::kIPHAutofillAtMemoryFeature;
                  })))
      .WillOnce(Return(true));

  client()->ShowAutofillAtMemoryPromo();
}

// Tests that `ShowAutofillAtMemoryPromo` is not propagated to the browser user
// education service when AtMemory eligibility checks fail.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       ShowAutofillAtMemoryPromo_ServiceDisabled) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);
  InitializePersonalContextEnablementService();
  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(Return(personal_context::PersonalContextEnablementState::
                                 kDisabledNotEligible));

  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo).Times(0);

  client()->ShowAutofillAtMemoryPromo();
}

// Tests that `ShowAutofillAtMemoryPromo` is not propagated to the browser user
// education service when AtMemory feature is disabled.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       ShowAutofillAtMemoryPromo_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAtMemory);
  InitializePersonalContextEnablementService();
  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo).Times(0);

  client()->ShowAutofillAtMemoryPromo();
}

// Tests that `ShowAutofillAtMemoryPromo` is not propagated to the browser user
// education service when the Personal Context toggle is off.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       ShowAutofillAtMemoryPromo_PersonalContextToggleOff) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);
  InitializePersonalContextEnablementService();
  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  profile()->GetPrefs()->SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);

  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo).Times(0);

  client()->ShowAutofillAtMemoryPromo();
}

// Tests that `AtMemoryCopyPasteObserver` does not track copy/paste signals and
// does not trigger the promo for incognito profiles.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       AtMemoryCopyPasteObserver_IncognitoNoTracking) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);

  // Verify that for regular profiles, the tracker exists.
  Profile* regular_profile = profile();
  EXPECT_NE(AtMemoryCrossTabCopyPasteTrackerFactory::GetForBrowserContext(
                regular_profile),
            nullptr);

  // Create an `OffTheRecord` (incognito) profile.
  Profile* incognito_profile =
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile);

  // Verify that for incognito profile, the tracker factory returns nullptr.
  EXPECT_EQ(AtMemoryCrossTabCopyPasteTrackerFactory::GetForBrowserContext(
                incognito_profile),
            nullptr);

  // Create web contents and client for the incognito profile.
  std::unique_ptr<content::WebContents> incognito_main_web_contents =
      content::WebContentsTester::CreateTestWebContents(incognito_profile,
                                                        nullptr);
  TestChromeAutofillClient* incognito_main_client =
      client(incognito_main_web_contents.get());
  ASSERT_TRUE(incognito_main_client);

  // Setup a secondary `WebContents` for pasting (so the copy and paste are
  // in different tabs).
  std::unique_ptr<content::WebContents> incognito_secondary_web_contents =
      content::WebContentsTester::CreateTestWebContents(incognito_profile,
                                                        nullptr);
  TestChromeAutofillClient* incognito_secondary_client =
      client(incognito_secondary_web_contents.get());
  ASSERT_TRUE(incognito_secondary_client);

  // Setup mock tab/window for the incognito `WebContents`.
  TabAndWindowMocks incognito_main_mocks;
  SetUpMockTabAndWindow(incognito_main_web_contents.get(), incognito_profile,
                        incognito_main_mocks);

  // Setup mock tab/window for the incognito `WebContents`.
  TabAndWindowMocks incognito_secondary_mocks;
  SetUpMockTabAndWindow(incognito_secondary_web_contents.get(),
                        incognito_profile, incognito_secondary_mocks);

  // Create mock user education for both windows.
  MockBrowserUserEducationInterface incognito_main_mock_user_education(
      &incognito_main_mocks.mock_window);
  MockBrowserUserEducationInterface incognito_secondary_mock_user_education(
      &incognito_secondary_mocks.mock_window);

  // Verify that `MaybeShowFeaturePromo` is not called for either window.
  EXPECT_CALL(incognito_main_mock_user_education, MaybeShowFeaturePromo)
      .Times(0);
  EXPECT_CALL(incognito_secondary_mock_user_education, MaybeShowFeaturePromo)
      .Times(0);

  // Create `SessionTabHelper` for both tabs to give them valid IDs.
  sessions::SessionTabHelper::CreateForWebContents(
      incognito_main_web_contents.get(),
      sessions::SessionTabHelper::DelegateLookup());
  sessions::SessionTabHelper::CreateForWebContents(
      incognito_secondary_web_contents.get(),
      sessions::SessionTabHelper::DelegateLookup());

  // Copy on the first tab, and paste on the second tab.
  incognito_main_client->at_memory_copy_paste_observer()
      .OnTextCopiedToClipboard(
          incognito_main_web_contents->GetPrimaryMainFrame(), u"some text");
  incognito_secondary_client->at_memory_copy_paste_observer().OnPaste();
}

// Tests that `AtMemoryCopyPasteObserver` tracks copy/paste signals and
// triggers the promo for regular profiles.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       AtMemoryCopyPasteObserver_RegularProfileTracking) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);
  InitializePersonalContextEnablementService();
  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  // Setup a secondary `WebContents` for pasting (so the copy and paste are in
  // different tabs).
  std::unique_ptr<content::WebContents> secondary_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  TestChromeAutofillClient* secondary_client =
      client(secondary_web_contents.get());
  ASSERT_TRUE(secondary_client);

  // Setup mock tab/window for the secondary `WebContents`.
  TabAndWindowMocks secondary_mocks;
  SetUpMockTabAndWindow(secondary_web_contents.get(), profile(),
                        secondary_mocks);

  // Create mock user education for both windows.
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());
  MockBrowserUserEducationInterface secondary_mock_user_education(
      &secondary_mocks.mock_window);

  // Expect the promo to be shown on the SECONDARY tab (where the paste
  // happens).
  EXPECT_CALL(secondary_mock_user_education,
              MaybeShowFeaturePromo(testing::Truly(
                  [](const user_education::FeaturePromoParams& params) {
                    return &*params.feature ==
                           &feature_engagement::kIPHAutofillAtMemoryFeature;
                  })))
      .WillOnce(Return(true));

  // The promo should not be shown on the primary tab (where the copy happens).
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo).Times(0);

  // Create `SessionTabHelper` for both tabs to give them valid IDs.
  sessions::SessionTabHelper::CreateForWebContents(
      web_contents(), sessions::SessionTabHelper::DelegateLookup());
  sessions::SessionTabHelper::CreateForWebContents(
      secondary_web_contents.get(),
      sessions::SessionTabHelper::DelegateLookup());

  // Copy on the first tab, and paste on the second tab.
  client()->at_memory_copy_paste_observer().OnTextCopiedToClipboard(
      web_contents()->GetPrimaryMainFrame(), u"some text");
  secondary_client->at_memory_copy_paste_observer().OnPaste();
}

#endif  // (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
     BUILDFLAG(IS_CHROMEOS)) &&                                       \
    !BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Tests that `ShowAutofillAtMemoryPromo` is not propagated to the browser user
// education service on non-branded builds even when all other conditions are
// met.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       ShowAutofillAtMemoryPromo_NonBrandedBuild) {
  base::test::ScopedFeatureList feature_list(features::kAutofillAtMemory);
  InitializePersonalContextEnablementService();
  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));

  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());
  EXPECT_CALL(mock_user_education, MaybeShowFeaturePromo).Times(0);

  client()->ShowAutofillAtMemoryPromo();
}
#endif  // (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that if there is no enablement service available to the profile, client
// defaults to kDisabledNotEligible state.
TEST_F(ChromeAutofillClientTest, GetPersonalContextEnablementState_NoService) {
  EXPECT_EQ(
      client()->GetPersonalContextEnablementState(),
      personal_context::PersonalContextEnablementState::kDisabledNotEligible);
}

// Tests that the client correctly pipes the state from the enablement service.
TEST_F(ChromeAutofillClientTest, GetPersonalContextEnablementState_HappyPath) {
  InitializePersonalContextEnablementService();

  EXPECT_CALL(*personal_context_enablement_service(), GetEnablementState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEnablementState::kEnabled));
  EXPECT_EQ(client()->GetPersonalContextEnablementState(),
            personal_context::PersonalContextEnablementState::kEnabled);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeAutofillClientTest, HideSuggestions_ProductFilter) {
  testing::NiceMock<MockAutofillPopupController> mock_controller;
  ON_CALL(mock_controller, GetMainFillingProduct)
      .WillByDefault(Return(FillingProduct::kAddress));
  client()->set_suggestion_controller_for_testing(mock_controller.GetWeakPtr());

  // Attempt to hide with a non-matching product filter should be ignored.
  EXPECT_CALL(mock_controller, Hide).Times(0);
  client()->HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                            FillingProduct::kPassword);
  testing::Mock::VerifyAndClearExpectations(&mock_controller);

  // Attempt to hide with a matching product filter should succeed.
  EXPECT_CALL(mock_controller, Hide(SuggestionHidingReason::kAcceptSuggestion));
  client()->HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                            FillingProduct::kAddress);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ChromeAutofillClientTest, IsAutofillProfileEnabled_BlockedByPolicy) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);
  NavigateAndCommit(GURL("https://example.com"));

  EXPECT_TRUE(client()->IsAutofillProfileEnabled());

  profile()->GetPrefs()->Set(
      prefs::kAutofillTypesBlocked,
      base::test::ParseJson(
          R"([{"url_pattern": "https://example.com", "blocked_types": ["contact_info"]}])"));

  EXPECT_FALSE(client()->IsAutofillProfileEnabled());
}

}  // namespace
}  // namespace autofill
