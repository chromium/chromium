// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/personal_context/personal_context_eligibility_service_factory.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client_test_api.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/autofill/content/browser/autofill_test_util.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/actor/actor_autofill_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
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
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/device_reauth/mock_device_auth_availability_checker.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_type.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/browser/ui/android/autofill/autofill_snackbar_controller_impl.h"
#include "chrome/browser/ui/android/autofill/autofill_snackbar_type.h"
#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_test_api.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"
#include "components/autofill/core/browser/ui/mock_autofill_tapjacking_protector.h"
#include "ui/android/window_android.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_options.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"  // nogncheck
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"  // nogncheck
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
#include "components/actor/core/task_source_info.h"
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
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;

class MockPersonalContextEligibilityService
    : public personal_context::PersonalContextEligibilityService {
 public:
  MockPersonalContextEligibilityService() = default;
  ~MockPersonalContextEligibilityService() override = default;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(bool, IsInitialized, (), (const, override));
  MOCK_METHOD(personal_context::PersonalContextEligibilityState,
              GetEligibilityState,
              (),
              (override));
  MOCK_METHOD(bool, IsEligibleForEncryption, (), (const, override));
  MOCK_METHOD(
      std::optional<personal_context::PersonalContextNonEligibilityReason>,
      GetNonEligibilityReason,
      (),
      (const, override));
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
  MOCK_METHOD(bool, IsMaybeShowing, (), (const, override));
  MOCK_METHOD(const base::Feature&, GetFeaturePromo, (), (const, override));
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

    auto save_card_bubble_controller =
        std::make_unique<MockSaveCardBubbleController>(web_contents());
    const auto* user_data_key = save_card_bubble_controller->UserDataKey();
    web_contents()->SetUserData(user_data_key,
                                std::move(save_card_bubble_controller));
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void InitializePersonalContextEligibilityService() {
    personal_context_eligibility_service_ =
        static_cast<MockPersonalContextEligibilityService*>(
            PersonalContextEligibilityServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockPersonalContextEligibilityService>();
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
    test_api(*client()).set_autofill_field_promo_controller(
        std::move(autofill_field_promo_controller));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  void TearDown() override {
    // Avoid that the raw pointer becomes dangling.
#if !BUILDFLAG(IS_ANDROID)
    autofill_field_promo_controller_ = nullptr;
#endif  // !BUILDFLAG(IS_ANDROID)
    personal_context_eligibility_service_ = nullptr;
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

  MockPersonalContextEligibilityService*
  personal_context_eligibility_service() {
    return personal_context_eligibility_service_;
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
  raw_ptr<MockPersonalContextEligibilityService>
      personal_context_eligibility_service_;
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
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiValuablesFeature);

  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillRepeatedly(Return(false));

  EXPECT_FALSE(client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kAutofillAi));
}

TEST_F(ChromeAutofillClientTest, AutofillFieldIPH_IsShown) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiValuablesFeature);

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
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiValuablesFeature);

  InSequence sequence;
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(false));
  EXPECT_CALL(*autofill_field_promo_controller(), Show);
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(true));

  EXPECT_TRUE(client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kAutofillAi));
}

TEST_F(ChromeAutofillClientTest, AutofillWalletDirectOffersFieldIPH_IsShown) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillWalletDirectOffersFeature);

  InSequence sequence;
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(false));
  EXPECT_CALL(*autofill_field_promo_controller(), Show);
  EXPECT_CALL(*autofill_field_promo_controller(), IsMaybeShowing)
      .WillOnce(Return(true));

  EXPECT_TRUE(client()->ShowAutofillFieldIphForFeature(
      FormFieldData{}, AutofillClient::IphFeature::kWalletDirectOffers));
}

TEST_F(ChromeAutofillClientTest,
       AutofillFieldIPH_HideOnShowAutofillSuggestions) {
  SetUpIphForTesting(feature_engagement::kIPHAutofillAiValuablesFeature);
  auto delegate = std::make_unique<MockAutofillSuggestionDelegate>();

  EXPECT_CALL(*autofill_field_promo_controller(), Hide);
  client()->ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                    delegate->GetWeakPtr());

  // Showing the Autofill Popup is an asynchronous task.
  task_environment()->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(autofill_field_promo_controller());
}

TEST_F(ChromeAutofillClientTest,
       ShowAutofillSuggestions_AbortsIfQueriedFieldChanges) {
  auto delegate = std::make_unique<MockAutofillSuggestionDelegate>();

  FieldGlobalId field1(LocalFrameToken(base::UnguessableToken::Create()),
                       FieldRendererId(1));
  FieldGlobalId field2(LocalFrameToken(base::UnguessableToken::Create()),
                       FieldRendererId(2));

  ON_CALL(*delegate, GetQueriedFieldId).WillByDefault(Return(field1));

  // Because ShowAutofillSuggestionsImpl() early-returns upon detecting a
  // queried field mismatch (field2 != field1), it aborts before creating or
  // showing an AutofillSuggestionController. Thus, no popup session is
  // created and delegate->OnSuggestionsHidden() must not be called (0 times).
  EXPECT_CALL(*delegate, OnSuggestionsHidden).Times(0);

  client()->ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                    delegate->GetWeakPtr());

  ON_CALL(*delegate, GetQueriedFieldId).WillByDefault(Return(field2));

  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  testing::Mock::VerifyAndClearExpectations(delegate.get());

  // When the queried field ID matches (field2 == field2),
  // ShowAutofillSuggestionsImpl() passes the guard check and proceeds to
  // create AutofillSuggestionController and call Show(). In this headless unit
  // test environment without window focus, Show() immediately dismisses the
  // popup with kNoFrameHasFocus, triggering delegate->OnSuggestionsHidden()
  // exactly once.
  EXPECT_CALL(*delegate,
              OnSuggestionsHidden(SuggestionHidingReason::kNoFrameHasFocus))
      .Times(1);

  client()->ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                    delegate->GetWeakPtr());

  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
}

class MockContentAutofillDriver : public TestContentAutofillDriver {
 public:
  using TestContentAutofillDriver::TestContentAutofillDriver;
  ~MockContentAutofillDriver() override = default;

  MOCK_METHOD(void,
              TriggerFormExtractionInAllFrames,
              (bool, base::OnceCallback<void(bool)>),
              (override));
};

class ChromeAutofillClientTestWithMockWindow : public ChromeAutofillClientTest {
 public:
  ChromeAutofillClientTestWithMockWindow() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillActorMode,
         ::features::kLazyKeyedServiceInstantiation},
        {});
    driver_injector_ = std::make_unique<
        TestAutofillDriverInjector<MockContentAutofillDriver>>();
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
    driver_injector_.reset();
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
    scoped_glic_bypass_.emplace();
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

  std::unique_ptr<TestAutofillDriverInjector<MockContentAutofillDriver>>
      driver_injector_;

 private:
  TabAndWindowMocks main_mocks_;
  glic::GlicProfileManager glic_profile_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeAutofillClientTestWithMockWindow,
       AutofillFieldIPH_NotifyFeatureUsed) {
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());

  EXPECT_CALL(mock_user_education,
              NotifyFeaturePromoFeatureUsed(
                  Ref(feature_engagement::kIPHAutofillAiValuablesFeature),
                  FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
  client()->NotifyIphFeatureUsed(AutofillClient::IphFeature::kAutofillAi);
}

TEST_F(ChromeAutofillClientTestWithMockWindow,
       AutofillWalletDirectOffersFieldIPH_NotifyFeatureUsed) {
  MockBrowserUserEducationInterface mock_user_education(
      &mock_browser_window_interface());

  EXPECT_CALL(
      mock_user_education,
      NotifyFeaturePromoFeatureUsed(
          Ref(feature_engagement::kIPHAutofillWalletDirectOffersFeature),
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
  client()->NotifyIphFeatureUsed(
      AutofillClient::IphFeature::kWalletDirectOffers);
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

// Like `ChromeAutofillClientTestWithMockWindow`, but the profile is latched to
// Gemini Enterprise.
class ChromeAutofillClientGeminiEnterpriseTest
    : public ChromeAutofillClientTestWithMockWindow {
 private:
  glic::ScopedGlicProviderOverride provider_override_{
      glic::GlicProvider::kGeminiEnterprise};
};

// Autofill doesn't open Gemini Enterprise.
TEST_F(ChromeAutofillClientGeminiEnterpriseTest, GlicEntryPointsDisabled) {
  glic::MockGlicKeyedService* mock_glic_service = SetUpMockGlicKeyedService();
  ASSERT_TRUE(mock_glic_service);
  ASSERT_TRUE(glic::GlicEnabling::IsEnabledForProfile(profile()));
  ASSERT_EQ(glic::GetGlicProviderSpec(profile()).GetSettingsSectionId(),
            glic::kGeminiEnterpriseSettingsSectionId);

  EXPECT_FALSE(client()->IsGlicEnabled());
  EXPECT_CALL(*mock_glic_service, Invoke).Times(0);
  client()->OpenGeminiInSidebar(u"test prompt");
}

// Gemini Enterprise consent isn't reported to Autofill as Gemini in Chrome
// consent.
TEST_F(ChromeAutofillClientGeminiEnterpriseTest, GlicConsentNotReported) {
  glic::MockGlicKeyedService* mock_glic_service = SetUpMockGlicKeyedService();
  ASSERT_TRUE(mock_glic_service);
  mock_glic_service->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kCompleted);
  ASSERT_TRUE(glic::GlicEnabling::HasConsentedForProfile(profile()));

  EXPECT_FALSE(client()->IsGlicConsented());
}

// Tests that `OnActorTaskStateChange` calls `TriggerFormExtractionInAllFrames`
// when a new task gets assigned.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       OnActorTaskStateChange_ReparseForms) {
  actor::ActorKeyedServiceFake* actor_service =
      static_cast<actor::ActorKeyedServiceFake*>(
          actor::ActorKeyedService::Get(profile()));
  ASSERT_TRUE(actor_service);

  MockContentAutofillDriver* mock_driver = (*driver_injector_)[web_contents()];
  ASSERT_TRUE(mock_driver);

  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);

  // Associate the active tab with the task.
  actor_service->GetSurfaceRegistry().OnTabCreated(mock_tab_interface());
  task->AddTab(mock_tab_interface().GetHandle(),
               /*stop_task_on_detach=*/true, base::DoNothing());

  EXPECT_FALSE(client()->IsTabInActorMode());

  MockFunction<void()> check;
  {
    InSequence s;
    // First call (assignment) triggers `TriggerFormExtractionInAllFrames()`
    // with actor mode already enabled.
    EXPECT_CALL(*mock_driver,
                TriggerFormExtractionInAllFrames(/*force=*/true, _))
        .WillOnce([this]() { EXPECT_TRUE(client()->IsTabInActorMode()); });
    EXPECT_CALL(check, Call);
    // Second call (no new task) does not trigger
    // `TriggerFormExtractionInAllFrames()`.
    EXPECT_CALL(*mock_driver, TriggerFormExtractionInAllFrames).Times(0);
  }

  actor_service->NotifyTaskStateChanged(*task);
  EXPECT_TRUE(client()->IsTabInActorMode());
  check.Call();
  actor_service->NotifyTaskStateChanged(*task);
}

// Tests that `OnActorTaskStateChange()` tracks whether the user controls the
// task, so that user fills during a pause are not logged as critical actions.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       OnActorTaskStateChange_TracksUserControl) {
  actor::ActorKeyedServiceFake* actor_service =
      static_cast<actor::ActorKeyedServiceFake*>(
          actor::ActorKeyedService::Get(profile()));
  ASSERT_TRUE(actor_service);

  MockContentAutofillDriver* mock_driver = (*driver_injector_)[web_contents()];
  ASSERT_TRUE(mock_driver);
  EXPECT_CALL(*mock_driver, TriggerFormExtractionInAllFrames)
      .Times(::testing::AnyNumber());

  actor::TaskId task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);

  // Associate the active tab with the task.
  actor_service->GetSurfaceRegistry().OnTabCreated(mock_tab_interface());
  task->AddTab(mock_tab_interface().GetHandle(),
               /*stop_task_on_detach=*/true, base::DoNothing());

  actor_service->NotifyTaskStateChanged(*task);
  ASSERT_TRUE(client()->GetActorAutofillManager()->active_actor_task());
  EXPECT_FALSE(client()
                   ->GetActorAutofillManager()
                   ->active_actor_task()
                   ->is_under_user_control);

  // The user takes over.
  actor_service->PauseTaskForTesting(task_id, /*from_actor=*/false);
  actor_service->NotifyTaskStateChanged(*task);
  ASSERT_TRUE(client()->GetActorAutofillManager()->active_actor_task());
  EXPECT_TRUE(client()
                  ->GetActorAutofillManager()
                  ->active_actor_task()
                  ->is_under_user_control);
  EXPECT_TRUE(client()->IsTabInActorMode());
}

class ChromeAutofillClientActorTaskSourceTest
    : public ChromeAutofillClientTestWithMockWindow {
 protected:
  // Starts a task created by `source_info`, adds it to the tab and notifies
  // the client about the task state.
  void StartTaskOnTab(const actor::TaskSourceInfo& source_info) {
    auto* actor_service = static_cast<actor::ActorKeyedServiceFake*>(
        actor::ActorKeyedService::Get(profile()));
    ASSERT_TRUE(actor_service);

    MockContentAutofillDriver* mock_driver =
        (*driver_injector_)[web_contents()];
    ASSERT_TRUE(mock_driver);
    EXPECT_CALL(*mock_driver, TriggerFormExtractionInAllFrames)
        .Times(::testing::AnyNumber());

    actor::TaskId task_id =
        actor_service->CreateTaskWithSourceInfoForTesting(source_info);
    actor::ActorTask* task = actor_service->GetTask(task_id);
    ASSERT_TRUE(task);
    actor_service->GetSurfaceRegistry().OnTabCreated(mock_tab_interface());
    task->AddTab(mock_tab_interface().GetHandle(),
                 /*stop_task_on_detach=*/true, base::DoNothing());

    actor_service->NotifyTaskStateChanged(*task);
  }

  std::optional<ActorAutofillManager::ActorTaskInfo> active_actor_task() {
    return client()->GetActorAutofillManager()->active_actor_task();
  }
};

// Tests that `OnActorTaskStateChange()` marks a Glic task's source as
// supported, so that its fills are logged as critical actions.
TEST_F(ChromeAutofillClientActorTaskSourceTest,
       OnActorTaskStateChange_GlicTaskSourceIsSupported) {
  StartTaskOnTab(actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                                       "conversation-123"));

  ASSERT_TRUE(active_actor_task());
  EXPECT_TRUE(active_actor_task()->critical_action_logger_supports_task_source);
}

// Tests that `OnActorTaskStateChange()` marks a non-Glic task's source as
// unsupported, so that its fills are not logged as critical actions, while the
// tab stays in actor mode.
TEST_F(ChromeAutofillClientActorTaskSourceTest,
       OnActorTaskStateChange_NonGlicTaskSourceIsNotSupported) {
  StartTaskOnTab(actor::TaskSourceInfo(
      actor::TaskSourceInfo::Client::kExperimentalActor, std::nullopt));

  ASSERT_TRUE(active_actor_task());
  EXPECT_FALSE(
      active_actor_task()->critical_action_logger_supports_task_source);
  EXPECT_TRUE(client()->IsTabInActorMode());
}

// Tests that `OnActorTaskStateChange()` on a page without a tab (a headless
// actor page) only picks up tasks that act on that page.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       OnActorTaskStateChange_HeadlessPageIgnoresTasksOnOtherPages) {
  actor::ActorKeyedServiceFake* actor_service =
      static_cast<actor::ActorKeyedServiceFake*>(
          actor::ActorKeyedService::Get(profile()));
  ASSERT_TRUE(actor_service);

  actor::ScopedHeadlessActorSurface headless =
      actor_service->CreateHeadlessActorSurface();
  const actor::ActorSurfaceHandle headless_handle =
      headless.actor_surface_handle();
  content::WebContents* headless_contents =
      headless_handle.Get()->GetWebContents();
  TestChromeAutofillClient* headless_client = client(headless_contents);
  ASSERT_TRUE(headless_client);
  ASSERT_FALSE(tabs::TabInterface::MaybeGetFromContents(headless_contents));

  // A task acting on the tab must not put the headless page in actor mode.
  actor::TaskId tab_task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* tab_task = actor_service->GetTask(tab_task_id);
  ASSERT_TRUE(tab_task);
  actor_service->GetSurfaceRegistry().OnTabCreated(mock_tab_interface());
  tab_task->AddTab(mock_tab_interface().GetHandle(),
                   /*stop_task_on_detach=*/true, base::DoNothing());
  actor_service->NotifyTaskStateChanged(*tab_task);

  EXPECT_TRUE(client()->IsTabInActorMode());
  EXPECT_FALSE(headless_client->IsTabInActorMode());

  // A task acting on the headless page puts it in actor mode.
  actor::TaskId headless_task_id = actor_service->CreateTaskForTesting();
  actor::ActorTask* headless_task = actor_service->GetTask(headless_task_id);
  ASSERT_TRUE(headless_task);
  headless_task->AddActorSurface(headless_handle,
                                 /*stop_task_on_detach=*/false,
                                 base::DoNothing());
  actor_service->NotifyTaskStateChanged(*headless_task);

  ASSERT_TRUE(headless_client->GetActorAutofillManager()->active_actor_task());
  EXPECT_EQ(
      headless_client->GetActorAutofillManager()->active_actor_task()->task_id,
      headless_task_id);
  EXPECT_EQ(client()->GetActorAutofillManager()->active_actor_task()->task_id,
            tab_task_id);
}

// Test that `actor::ActorTaskOptions::skip_history_recording` is forwarded to
// `ActorAutofillManager::ActorTaskInfo::skip_history_recording`.
TEST_F(ChromeAutofillClientTestWithMockWindow,
       OnActorTaskStateChange_PropagatesSkipHistoryRecording) {
  actor::ActorKeyedServiceFake* actor_service =
      static_cast<actor::ActorKeyedServiceFake*>(
          actor::ActorKeyedService::Get(profile()));
  ASSERT_TRUE(actor_service);

  MockContentAutofillDriver* mock_driver = (*driver_injector_)[web_contents()];
  ASSERT_TRUE(mock_driver);
  EXPECT_CALL(*mock_driver, TriggerFormExtractionInAllFrames)
      .Times(::testing::AnyNumber());

  actor::ActorTaskOptions options;
  options.skip_history_recording = true;
  actor::TaskId task_id = actor_service->CreateTaskForTesting(
      actor::TestTaskSourceInfo(), std::move(options));
  actor::ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);

  actor_service->GetSurfaceRegistry().OnTabCreated(mock_tab_interface());
  task->AddTab(mock_tab_interface().GetHandle(),
               /*stop_task_on_detach=*/true, base::DoNothing());

  actor_service->NotifyTaskStateChanged(*task);
  EXPECT_TRUE(client()->IsTabInActorMode());
  ASSERT_TRUE(client()->GetActorAutofillManager());
  ASSERT_TRUE(
      client()->GetActorAutofillManager()->active_actor_task().has_value());
  EXPECT_TRUE(client()
                  ->GetActorAutofillManager()
                  ->active_actor_task()
                  ->skip_history_recording);
}

#endif  //  !BUILDFLAG(IS_ANDROID)

// Tests that if there is no enablement service available to the profile, client
// defaults to kDisabledNotEligible state.
TEST_F(ChromeAutofillClientTest, GetPersonalContextEligibilityState_NoService) {
  EXPECT_EQ(
      client()->GetPersonalContextEligibilityState(),
      personal_context::PersonalContextEligibilityState::kDisabledNotEligible);
}

// Tests that the client correctly pipes the state from the enablement service.
TEST_F(ChromeAutofillClientTest, GetPersonalContextEligibilityState_HappyPath) {
  InitializePersonalContextEligibilityService();

  EXPECT_CALL(*personal_context_eligibility_service(), GetEligibilityState())
      .WillRepeatedly(
          Return(personal_context::PersonalContextEligibilityState::kEligible));
  EXPECT_EQ(client()->GetPersonalContextEligibilityState(),
            personal_context::PersonalContextEligibilityState::kEligible);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeAutofillClientTest, HideSuggestions_ProductFilter) {
  testing::NiceMock<MockAutofillPopupController> mock_controller;
  ON_CALL(mock_controller, GetMainFillingProduct)
      .WillByDefault(Return(FillingProduct::kAddress));
  test_api(*client()).set_suggestion_controller(mock_controller.GetWeakPtr());

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

TEST_F(ChromeAutofillClientTest,
       UpdateAutofillDataListValues_FrameTokenFilter) {
  LocalFrameToken token1(base::UnguessableToken::Create());
  LocalFrameToken token2(base::UnguessableToken::Create());

  testing::NiceMock<MockAutofillPopupController> mock_controller;
  mock_controller.set_anchor_frame_token(token1);

  test_api(*client()).set_suggestion_controller(mock_controller.GetWeakPtr());

  std::vector<SelectOption> options = {{.value = u"val", .text = u"txt"}};

  // Attempt to update with a non-matching frame token should be ignored.
  EXPECT_CALL(mock_controller, UpdateDataListValues).Times(0);
  client()->UpdateAutofillDataListValues(token2, options);
  testing::Mock::VerifyAndClearExpectations(&mock_controller);

  // Attempt to update with a matching frame token should succeed.
  EXPECT_CALL(
      mock_controller,
      UpdateDataListValues(ElementsAre(Field(&SelectOption::value, u"val"))));
  client()->UpdateAutofillDataListValues(token1, options);
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
// Tests that IsAutofillEnabled correctly returns false when all active autofill
// types (including AI data types) are globally blocked by enterprise policy.
TEST_F(ChromeAutofillClientTest, IsAutofillEnabled_BlockedByPolicy) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);
  NavigateAndCommit(GURL("https://example.com"));

  // Disable profile and payments so IsAutofillEnabled depends on the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled, false);

  // Enable the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiTravelEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiShoppingEntitiesEnabled,
                                    true);

  EXPECT_TRUE(client()->IsAutofillEnabled());

  // Block only identity docs.
  profile()->GetPrefs()->Set(
      prefs::kAutofillTypesBlocked,
      base::test::ParseJson(
          R"([{"url_pattern": "https://example.com", "blocked_types": ["identity_docs"]}])"));
  // Still true because travel and shopping are enabled.
  EXPECT_TRUE(client()->IsAutofillEnabled());

  // Block identity docs and travel.
  profile()->GetPrefs()->Set(
      prefs::kAutofillTypesBlocked,
      base::test::ParseJson(
          R"([{"url_pattern": "https://example.com", "blocked_types": ["identity_docs", "travel"]}])"));
  EXPECT_TRUE(client()->IsAutofillEnabled());

  // Block all three.
  profile()->GetPrefs()->Set(
      prefs::kAutofillTypesBlocked,
      base::test::ParseJson(
          R"([{"url_pattern": "https://example.com", "blocked_types": ["identity_docs", "travel", "shopping"]}])"));
  EXPECT_FALSE(client()->IsAutofillEnabled());
}

// Tests that IsAutofillEnabled does not consider AI data types when the
// enterprise policy feature flag is disabled, strictly adhering to the original
// behavior.
TEST_F(ChromeAutofillClientTest,
       IsAutofillEnabled_AiTypesGatedByEnterprisePolicyFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnableAutofillSettingsEnterprisePolicy);

  // Disable profile and payments so IsAutofillEnabled depends on the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled, false);

  // Enable the AI types.
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiTravelEntitiesEnabled,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillAiShoppingEntitiesEnabled,
                                    true);

  // If the enterprise policy flag is OFF, IsAutofillEnabled does not check AI
  // types.
  EXPECT_FALSE(client()->IsAutofillEnabled());
}

TEST_F(ChromeAutofillClientTest, GetAffiliationService) {
  EXPECT_EQ(AffiliationServiceFactory::GetForProfile(profile()),
            client()->GetAffiliationService());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromeAutofillClientTest,
       ShowAutofillAiSuggestionRemovedNotification_ActionCallbackTriggered) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAmbientAutofillSuppression);

  base::MockCallback<base::OnceClosure> on_undo_clicked;
  EXPECT_CALL(on_undo_clicked, Run);

  client()->ShowAutofillAiSuggestionRemovedNotification(
      test::GetPassportEntityInstance(
          {.record_type = EntityInstance::RecordType::kPersonalContext}),
      on_undo_clicked.Get());

  AutofillSnackbarControllerImpl* snackbar_controller =
      client()->GetAutofillSnackbarController();
  ASSERT_TRUE(snackbar_controller);
  EXPECT_EQ(snackbar_controller->GetSnackbarType(),
            AutofillSnackbarType::kAutofillAiSuppressionUndo);

  // Triggering the action callback should invoke on_undo_clicked.
  snackbar_controller->OnActionClicked();
}

TEST_F(ChromeAutofillClientTest, ShowTapjackingProtector) {
  auto mock_protector = std::make_unique<MockAutofillTapjackingProtector>();
  EXPECT_CALL(
      *mock_protector,
      Show(AutofillTapjackingProtector::AuthorizationType::kPayments, _));

  test_api(*client()).set_autofill_tapjacking_protector(
      std::move(mock_protector));

  base::MockCallback<AutofillTapjackingProtector::AuthorizationCallback>
      callback;
  client()->ShowTapjackingProtector(
      AutofillTapjackingProtector::AuthorizationType::kPayments,
      callback.Get());
}

class MockEmailVerificationBottomSheetBridge
    : public EmailVerificationBottomSheetBridge {
 public:
  explicit MockEmailVerificationBottomSheetBridge(
      content::WebContents* web_contents)
      : EmailVerificationBottomSheetBridge(web_contents) {}
  ~MockEmailVerificationBottomSheetBridge() override = default;

  MOCK_METHOD(void,
              ShowJavaBottomSheet,
              (const std::u16string& title, const std::u16string& description),
              (override));
  MOCK_METHOD(void, HideJavaBottomSheet, (), (override));
};

TEST_F(ChromeAutofillClientTest,
       EmailVerification_DelegatesShowAndHidePopupToController) {
  auto window = ui::WindowAndroid::CreateForTesting();
  window->get()->AddChild(web_contents()->GetNativeView());

  auto mock_bridge =
      std::make_unique<MockEmailVerificationBottomSheetBridge>(web_contents());
  MockEmailVerificationBottomSheetBridge* bridge_ptr = mock_bridge.get();
  test_api(*test_api(*client()).email_verification_controller())
      .set_bottom_sheet_bridge(std::move(mock_bridge));

  EXPECT_CALL(*bridge_ptr, ShowJavaBottomSheet);
  client()->ShowEmailVerificationPopup(
      gfx::RectF(0, 0, 10, 10), url::Origin::Create(GURL("https://google.com")),
      gfx::Image(), u"user@example.com", base::DoNothing());

  EXPECT_CALL(*bridge_ptr, HideJavaBottomSheet);
  client()->HideEmailVerificationPopup();
}
#endif  // BUILDFLAG(IS_ANDROID)

class OpenUrlTestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    last_params_ = params;
    return source;
  }

  const std::optional<content::OpenURLParams>& last_params() const {
    return last_params_;
  }

 private:
  std::optional<content::OpenURLParams> last_params_;
};

TEST_F(ChromeAutofillClientTest, OpenGmailForOtps) {
  OpenUrlTestWebContentsDelegate delegate;
  content::WebContentsDelegate* original_delegate =
      web_contents()->GetDelegate();
  base::ScopedClosureRunner reset_delegate(
      base::BindOnce(&content::WebContents::SetDelegate,
                     base::Unretained(web_contents()), original_delegate));
  web_contents()->SetDelegate(&delegate);

  client()->OpenGmailForOtps();

  ASSERT_TRUE(delegate.last_params().has_value());
  EXPECT_EQ(delegate.last_params()->url, GURL("https://mail.google.com"));
  EXPECT_EQ(delegate.last_params()->disposition,
            ui::mojom::WindowOpenDisposition::kNewForegroundTab);
}

TEST_F(ChromeAutofillClientTest,
       ShowGmailOtpOptInBubble_WithoutTabInterface_RunsCallbackWithDiscarded) {
  base::test::TestFuture<GmailOtpOptInResult> future;
  client()->ShowGmailOtpOptInBubble(u"user@gmail.com", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), GmailOtpOptInResult::kDiscarded);
}

TEST_F(ChromeAutofillClientTest, GetOtpPhishGuardDelegate_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kOtpPhishGuard);
  EXPECT_NE(client()->GetOtpPhishGuardDelegate(), nullptr);
}

TEST_F(ChromeAutofillClientTest, GetOtpPhishGuardDelegate_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kOtpPhishGuard);
  EXPECT_EQ(client()->GetOtpPhishGuardDelegate(), nullptr);
}

TEST_F(ChromeAutofillClientTest, GetOtpCoordinator_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list(features::kAutofillOtpCoordinator);
  EXPECT_NE(client()->GetOtpCoordinator(), nullptr);
}

TEST_F(ChromeAutofillClientTest, GetOtpCoordinator_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillOtpCoordinator);
  EXPECT_EQ(client()->GetOtpCoordinator(), nullptr);
}

class ChromeAutofillClientReauthAvailabilityTest : public testing::Test {
 protected:
  using CheckerFactory =
      ChromeAutofillClientTestApi::DeviceAuthAvailabilityCheckerFactory;

  std::unique_ptr<TestingProfile> BuildProfileWithEntityDataManager() {
    return TestingProfile::Builder()
        .AddTestingFactory(
            AutofillEntityDataManagerFactory::GetInstance(),
            base::BindRepeating(
                [](AutofillWebDataServiceTestHelper* helper,
                   content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  return std::make_unique<EntityDataManager>(
                      Profile::FromBrowserContext(context)->GetPrefs(), nullptr,
                      nullptr, helper->autofill_webdata_service(), nullptr,
                      nullptr, nullptr, GeoIpCountryCode("US"));
                },
                base::Unretained(&webdata_helper_)))
        .Build();
  }

  void SetCanAuthenticate(bool can_authenticate) {
    ON_CALL(checker_factory_, Run).WillByDefault([can_authenticate]() {
      auto checker = std::make_unique<
          NiceMock<device_reauth::MockDeviceAuthAvailabilityChecker>>();
      ON_CALL(*checker, CanAuthenticateWithBiometricOrScreenLock)
          .WillByDefault(Return(can_authenticate));
      return checker;
    });
  }

  std::unique_ptr<content::WebContents> CreateWebContentsAndVerifyReauth(
      Profile* profile,
      int expected_queries,
      bool expected_reauth) {
    EXPECT_CALL(checker_factory_, Run).Times(expected_queries);
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    EXPECT_EQ(AutofillEntityDataManagerFactory::GetForProfile(profile)
                  ->GetReauthAvailability(),
              expected_reauth);
    testing::Mock::VerifyAndClearExpectations(&checker_factory_);
    return web_contents;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler test_render_host_factories_;
  test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClientInjector<TestChromeAutofillClient>
      test_autofill_client_injector_;
  base::test::ScopedFeatureList scoped_feature_list_{
      {features::kAutofillAiWalletPrivatePasses,
       password_manager::features::kDeviceReauthRefactor},
      {}};
  base::MockCallback<CheckerFactory> checker_factory_;
  ChromeAutofillClientTestApi::ScopedDeviceAuthAvailabilityCheckerFactory
      scoped_checker_factory_{checker_factory_.Get()};
};

TEST_F(ChromeAutofillClientReauthAvailabilityTest,
       QueriesReauthAvailabilityOncePerProfileWithinCacheDuration) {
  auto profile1 = BuildProfileWithEntityDataManager();
  auto profile2 = BuildProfileWithEntityDataManager();
  EntityDataManager* edm1 =
      AutofillEntityDataManagerFactory::GetForProfile(profile1.get());
  EntityDataManager* edm2 =
      AutofillEntityDataManagerFactory::GetForProfile(profile2.get());
  ASSERT_TRUE(edm1);
  ASSERT_TRUE(edm2);
  ASSERT_NE(edm1, edm2);
  EXPECT_EQ(edm1->GetReauthAvailability(), std::nullopt);
  EXPECT_EQ(edm2->GetReauthAvailability(), std::nullopt);

  SetCanAuthenticate(true);
  auto wc1 = CreateWebContentsAndVerifyReauth(profile1.get(), 1, true);
  auto wc2 = CreateWebContentsAndVerifyReauth(profile1.get(), 0, true);
  // Incognito profiles share the original profile's `EntityDataManager`.
  auto incognito_wc = CreateWebContentsAndVerifyReauth(
      profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true), 0, true);

  SetCanAuthenticate(false);
  auto p2_wc1 = CreateWebContentsAndVerifyReauth(profile2.get(), 1, false);
  auto p2_wc2 = CreateWebContentsAndVerifyReauth(profile2.get(), 0, false);
}

TEST_F(ChromeAutofillClientReauthAvailabilityTest,
       RequeriesReauthAvailabilityAfterCacheDurationExpires) {
  auto profile = BuildProfileWithEntityDataManager();

  SetCanAuthenticate(true);
  auto wc1 = CreateWebContentsAndVerifyReauth(profile.get(), 1, true);

  // Throttled before `kReauthAvailabilityCacheDuration` elapses.
  SetCanAuthenticate(false);
  task_environment_.FastForwardBy(
      EntityDataManager::kReauthAvailabilityCacheDuration / 2);
  auto wc2 = CreateWebContentsAndVerifyReauth(profile.get(), 0, true);

  // Refreshed once the cache duration expires.
  task_environment_.FastForwardBy(
      EntityDataManager::kReauthAvailabilityCacheDuration / 2);
  auto wc3 = CreateWebContentsAndVerifyReauth(profile.get(), 1, false);
}

TEST_F(ChromeAutofillClientReauthAvailabilityTest,
       QueriesReauthAvailabilityWithAmbientAutofillEnabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillAmbientAutofill,
       password_manager::features::kDeviceReauthRefactor},
      {features::kAutofillAiWalletPrivatePasses});

  auto profile = BuildProfileWithEntityDataManager();
  EXPECT_EQ(AutofillEntityDataManagerFactory::GetForProfile(profile.get())
                ->GetReauthAvailability(),
            std::nullopt);

  SetCanAuthenticate(true);
  auto wc1 = CreateWebContentsAndVerifyReauth(profile.get(), 1, true);
  auto wc2 = CreateWebContentsAndVerifyReauth(profile.get(), 0, true);
}

// Tests that a pending HTTPS navigation does not make an HTTP context secure.
TEST_F(ChromeAutofillClientTest,
       IsContextSecure_HttpWithPendingHttpsNavigation) {
  const GURL http_url("http://example.test/form.html");
  const GURL https_url("https://secure.example.test/");

  NavigateAndCommit(http_url);
  EXPECT_FALSE(client()->IsContextSecure());

  std::unique_ptr<content::NavigationSimulator> pending_nav =
      content::NavigationSimulator::CreateBrowserInitiated(https_url,
                                                           web_contents());
  pending_nav->Start();

  ASSERT_NE(web_contents()->GetController().GetVisibleEntry(), nullptr);
  EXPECT_EQ(web_contents()->GetController().GetVisibleEntry()->GetURL(),
            https_url);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), http_url);
  EXPECT_FALSE(client()->IsContextSecure());
}

// Tests that a pending navigation does not mask a DANGEROUS HTTPS state.
TEST_F(ChromeAutofillClientTest,
       IsContextSecure_DangerousHttpsWithPendingNavigation) {
  const GURL https_url("https://broken.example.test/form.html");

  NavigateAndCommit(https_url);
  content::NavigationEntry* const entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(entry, nullptr);
  entry->GetSSL().certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_NE(entry->GetSSL().certificate, nullptr);
  entry->GetSSL().cert_status = net::CERT_STATUS_DATE_INVALID;
  entry->GetSSL().content_status = content::SSLStatus::RAN_INSECURE_CONTENT;

  EXPECT_FALSE(client()->IsContextSecure());
  EXPECT_EQ(client()->GetSecurityLevelForUmaHistograms(),
            security_state::DANGEROUS);

  std::unique_ptr<content::NavigationSimulator> pending_nav =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://other.example.test/"), web_contents());
  pending_nav->Start();

  // While the visible entry is the uncommitted pending navigation (whose
  // security level is NONE), Autofill must continue evaluating the committed
  // document's DANGEROUS security state.
  ASSERT_EQ(chrome_security_state::GetSecurityLevel(web_contents()),
            security_state::NONE);
  EXPECT_FALSE(client()->IsContextSecure());
  EXPECT_EQ(client()->GetSecurityLevelForUmaHistograms(),
            security_state::DANGEROUS);
}

// Tests that an HTTPS error page is not considered a secure context.
TEST_F(ChromeAutofillClientTest, IsContextSecure_HttpsErrorPage) {
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL("https://unreachable.example.test/"),
      net::ERR_CONNECTION_TIMED_OUT);

  content::NavigationEntry* const entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetPageType(), content::PAGE_TYPE_ERROR);
  EXPECT_TRUE(web_contents()->GetLastCommittedURL().SchemeIsCryptographic());
  EXPECT_FALSE(client()->IsContextSecure());
}

// Tests that a pending HTTP navigation does not degrade a SECURE HTTPS state.
TEST_F(ChromeAutofillClientTest,
       IsContextSecure_ValidHttpsWithPendingHttpNavigation) {
  const GURL https_url("https://secure.example.test/form.html");

  NavigateAndCommit(https_url);
  content::NavigationEntry* const entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(entry, nullptr);
  entry->GetSSL().certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_NE(entry->GetSSL().certificate, nullptr);

  EXPECT_TRUE(client()->IsContextSecure());
  EXPECT_EQ(client()->GetSecurityLevelForUmaHistograms(),
            security_state::SECURE);

  std::unique_ptr<content::NavigationSimulator> pending_nav =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://insecure.example.test/"), web_contents());
  pending_nav->Start();

  ASSERT_EQ(chrome_security_state::GetSecurityLevel(web_contents()),
            security_state::NONE);
  EXPECT_TRUE(client()->IsContextSecure());
  EXPECT_EQ(client()->GetSecurityLevelForUmaHistograms(),
            security_state::SECURE);
}
}  // namespace
}  // namespace autofill
