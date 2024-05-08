// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/ui/ui_util.h"
#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/browser/browser_context.h"
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

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Ref;
using ::testing::Return;
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

  MOCK_METHOD(void, ShowConfirmationBubbleView, (bool), (override));
};
#endif

class MockAutofillFieldPromoController : public AutofillFieldPromoController {
 public:
  ~MockAutofillFieldPromoController() override = default;
  MOCK_METHOD(void, Show, (const gfx::RectF&), (override));
  MOCK_METHOD(void, Hide, (), (override));
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
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PreparePersonalDataManager();
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));

    auto autofill_field_promo_controller_manual_fallback =
        std::make_unique<MockAutofillFieldPromoController>();
    autofill_field_promo_controller_manual_fallback_ =
        autofill_field_promo_controller_manual_fallback.get();
    client()->SetAutofillFieldPromoControllerManualFallbackForTesting(
        std::move(autofill_field_promo_controller_manual_fallback));

#if !BUILDFLAG(IS_ANDROID)
    SecurityStateTabHelper::CreateForWebContents(web_contents());

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

  TestPersonalDataManager* personal_data_manager() {
    return personal_data_manager_;
  }

  MockAutofillFieldPromoController*
  autofill_field_promo_controller_manual_fallback() {
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
                profile(), base::BindRepeating([](content::BrowserContext*) {
                  return std::make_unique<TestPersonalDataManager>();
                }));

    personal_data_manager_->SetAutofillProfileEnabled(true);
    personal_data_manager_->SetAutofillPaymentMethodsEnabled(true);
    personal_data_manager_->SetAutofillWalletImportEnabled(false);

    // Enable MSBB by default. If MSBB has been explicitly turned off, Fast
    // Checkout is not supported.
    profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  raw_ptr<TestPersonalDataManager> personal_data_manager_ = nullptr;
  raw_ptr<MockAutofillFieldPromoController>
      autofill_field_promo_controller_manual_fallback_;
  TestAutofillClientInjector<TestChromeAutofillClient>
      test_autofill_client_injector_;
  base::OnceCallback<void()> setup_flags_;
};

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_BelowMaxFlowTime) {
  // Arbitrary fixed date to avoid using Now().
  base::Time july_2022 = base::Time::FromSecondsSinceUnixEpoch(1658620440);
  base::TimeDelta below_max_flow_time = base::Minutes(10);

  autofill::TestAutofillClock test_clock(july_2022);

  FormInteractionsFlowId first_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  test_clock.Advance(below_max_flow_time);

  EXPECT_EQ(first_interaction_flow_id,
            client()->GetCurrentFormInteractionsFlowId());
}

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_AboveMaxFlowTime) {
  // Arbitrary fixed date to avoid using Now().
  base::Time july_2022 = base::Time::FromSecondsSinceUnixEpoch(1658620440);
  base::TimeDelta above_max_flow_time = base::Minutes(21);

  autofill::TestAutofillClock test_clock(july_2022);

  FormInteractionsFlowId first_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  test_clock.Advance(above_max_flow_time);

  EXPECT_NE(first_interaction_flow_id,
            client()->GetCurrentFormInteractionsFlowId());
}

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_AdvancedTwice) {
  // Arbitrary fixed date to avoid using Now().
  base::Time july_2022 = base::Time::FromSecondsSinceUnixEpoch(1658620440);
  base::TimeDelta above_half_max_flow_time = base::Minutes(15);

  autofill::TestAutofillClock test_clock(july_2022);

  FormInteractionsFlowId first_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  test_clock.Advance(above_half_max_flow_time);

  FormInteractionsFlowId second_interaction_flow_id =
      client()->GetCurrentFormInteractionsFlowId();

  test_clock.Advance(above_half_max_flow_time);

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
  EXPECT_CALL(save_card_bubble_controller(), ShowConfirmationBubbleView(true));
  client()->GetPaymentsAutofillClient()->CreditCardUploadCompleted(true);
}

TEST_F(ChromeAutofillClientTest,
       CreditCardUploadCompleted_ShowConfirmationBubbleView_CardNotSaved) {
  EXPECT_CALL(save_card_bubble_controller(), ShowConfirmationBubbleView(false));
  client()->GetPaymentsAutofillClient()->CreditCardUploadCompleted(false);
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
  profile2.set_source_for_testing(AutofillProfile::Source::kAccount);
  client()->ShowEditAddressProfileDialog(profile2, base::DoNothing());
  std::optional<AccountInfo> account = GetPrimaryAccountInfoFromBrowserContext(
      web_contents()->GetBrowserContext());
  EXPECT_EQ(controller->GetFooterMessage(),
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_UPDATE_PROMPT_ACCOUNT_ADDRESS_SOURCE_NOTICE,
                base::ASCIIToUTF16(account->email)));
}

TEST_F(ChromeAutofillClientTest, AutofillManualFallbackIPH_IsShown) {
  EXPECT_CALL(*autofill_field_promo_controller_manual_fallback(), Show);
  client()->ShowAutofillFieldIphForManualFallbackFeature(FormFieldData{});
}

TEST_F(ChromeAutofillClientTest,
       AutofillManualFallbackIPH_HideOnShowAutofillSuggestions) {
  auto delegate = std::make_unique<MockAutofillSuggestionDelegate>();

  EXPECT_CALL(*autofill_field_promo_controller_manual_fallback(), Hide);
  client()->ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                    delegate->GetWeakPtr());

  // Showing the Autofill Popup is an asynchronous task.
  task_environment()->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(
      autofill_field_promo_controller_manual_fallback());
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

#if BUILDFLAG(IS_ANDROID)
// Verify that the prompt to upload save a user's card without CVC is shown in a
// bottom sheet.
TEST_F(
    ChromeAutofillClientTest,
    ConfirmSaveCreditCardToCloud_CardSaveTypeIsOnlyCard_RequestsBottomSheet) {
  TestChromeAutofillClient* autofill_client = client();
  auto* bottom_sheet_bridge =
      autofill_client->InjectMockAutofillSaveCardBottomSheetBridge();

  std::u16string expected_description;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_description =
      u"To pay faster next time, save your card and billing address in your "
      u"Google Account";
#endif

  // Verify that `AutofillSaveCardUiInfo` has the correct attributes that
  // indicate upload save card prompt without CVC.
  EXPECT_CALL(*bottom_sheet_bridge,
              RequestShowContent(
                  AllOf(Field(&AutofillSaveCardUiInfo::is_for_upload, true),
                        Field(&AutofillSaveCardUiInfo::description_text,
                              expected_description)),
                  testing::NotNull()));

  autofill_client->ConfirmSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      ChromeAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveOnly)
          .with_show_prompt(true),
      base::DoNothing());
}

// Verify that the prompt to upload save a user's card with CVC is shown in a
// bottom sheet.
TEST_F(ChromeAutofillClientTest,
       ConfirmSaveCreditCardToCloud_CardSaveTypeIsWithCvc_RequestsBottomSheet) {
  TestChromeAutofillClient* autofill_client = client();
  auto* bottom_sheet_bridge =
      autofill_client->InjectMockAutofillSaveCardBottomSheetBridge();

  std::u16string expected_description;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_description =
      u"To pay faster next time, save your card, encrypted security code, and "
      u"billing address in your Google Account";
#endif

  // Verify that `AutofillSaveCardUiInfo` has the correct attributes that
  // indicate upload save card prompt with CVC.
  EXPECT_CALL(*bottom_sheet_bridge,
              RequestShowContent(
                  AllOf(Field(&AutofillSaveCardUiInfo::is_for_upload, true),
                        Field(&AutofillSaveCardUiInfo::description_text,
                              expected_description)),
                  testing::NotNull()));

  autofill_client->ConfirmSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      ChromeAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromeAutofillClientTest,
       ConfirmSaveCreditCardToCloud_DoesNotFailWithoutAWindow) {
  TestChromeAutofillClient* autofill_client = client();

  EXPECT_NO_FATAL_FAILURE(autofill_client->ConfirmSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      ChromeAutofillClient::SaveCreditCardOptions().with_show_prompt(true),
      base::DoNothing()));
}

// Verify that the prompt to local save a user's card is shown in a bottom
// sheet.
TEST_F(
    ChromeAutofillClientTest,
    ConfirmSaveCreditCardLocally_CardSaveTypeIsOnlyCard_RequestsBottomSheet) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableCvcStorageAndFilling};

  TestChromeAutofillClient* autofill_client = client();
  auto* bottom_sheet_bridge =
      autofill_client->InjectMockAutofillSaveCardBottomSheetBridge();

  // Verify that `AutofillSaveCardUiInfo` has the correct attributes that
  // indicate local save card prompt without CVC.
  EXPECT_CALL(
      *bottom_sheet_bridge,
      RequestShowContent(
          AllOf(
              Field(&AutofillSaveCardUiInfo::is_for_upload, false),
              Field(&AutofillSaveCardUiInfo::description_text,
                    u"To pay faster next time, save your card to your device")),
          testing::NotNull()));

  autofill_client->ConfirmSaveCreditCardLocally(
      CreditCard(),
      ChromeAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveOnly)
          .with_show_prompt(true),
      base::DoNothing());
}

// Verify that the prompt to local save a user's card and CVC is shown in a
// bottom sheet.
TEST_F(ChromeAutofillClientTest,
       ConfirmSaveCreditCardLocally_CardSaveTypeIsWithCvc_RequestsBottomSheet) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableCvcStorageAndFilling};

  TestChromeAutofillClient* autofill_client = client();
  auto* bottom_sheet_bridge =
      autofill_client->InjectMockAutofillSaveCardBottomSheetBridge();

  // Verify that `AutofillSaveCardUiInfo` has the correct attributes that
  // indicate local save card prompt with CVC.
  EXPECT_CALL(*bottom_sheet_bridge,
              RequestShowContent(
                  AllOf(Field(&AutofillSaveCardUiInfo::is_for_upload, false),
                        Field(&AutofillSaveCardUiInfo::description_text,
                              u"To pay faster next time, save your card and "
                              u"encrypted security code to your device")),
                  testing::NotNull()));

  autofill_client->ConfirmSaveCreditCardLocally(
      CreditCard(),
      ChromeAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromeAutofillClientTest,
       ConfirmSaveCreditCardLocally_DoesNotFailWithoutAWindow) {
  TestChromeAutofillClient* autofill_client = client();

  EXPECT_NO_FATAL_FAILURE(autofill_client->ConfirmSaveCreditCardLocally(
      CreditCard(),
      ChromeAutofillClient::SaveCreditCardOptions().with_show_prompt(true),
      base::DoNothing()));
}
#endif
}  // namespace
}  // namespace autofill
