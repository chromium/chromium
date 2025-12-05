// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include <optional>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/functional/callback.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_payment_method_accessory_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/mock_touch_to_fill_payment_method_controller.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/browser/ui/android/autofill/autofill_save_iban_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_controller.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/android_bnpl_strategy.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "ui/android/window_android.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/ui_features.h"  // nogncheck
#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"
#endif                                      // BUILDFLAG(IS_ANDROID)

using ::autofill::test::CreateLoyaltyCard;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;

namespace autofill {

#if BUILDFLAG(IS_ANDROID)

Matcher<payments::BnplIssuerContext> EqualsBnplIssuerContext(
    BnplIssuer::IssuerId issuer_id,
    payments::BnplIssuerEligibilityForPage eligibility) {
  return AllOf(Field(&payments::BnplIssuerContext::issuer,
                     Property(&BnplIssuer::issuer_id, Eq(issuer_id))),
               Field(&payments::BnplIssuerContext::eligibility, eligibility));
}

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
  MOCK_METHOD(void, Hide, (), (override));
};

class MockAutofillSaveIbanBottomSheetBridge
    : public AutofillSaveIbanBottomSheetBridge {
 public:
  MockAutofillSaveIbanBottomSheetBridge()
      : AutofillSaveIbanBottomSheetBridge(
            base::android::ScopedJavaGlobalRef<jobject>(nullptr)) {}

  MOCK_METHOD(void, Hide, (), (override));
};

class MockAutofillSnackbarControllerImpl
    : public AutofillSnackbarControllerImpl {
 public:
  explicit MockAutofillSnackbarControllerImpl(
      content::WebContents* web_contents)
      : AutofillSnackbarControllerImpl(web_contents) {}

  MOCK_METHOD(void,
              Show,
              (AutofillSnackbarType, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              ShowWithDurationAndCallback,
              (AutofillSnackbarType,
               base::TimeDelta,
               base::OnceClosure,
               std::optional<base::OnceClosure>),
              (override));
  MOCK_METHOD(void,
              ShowPaymentsSnackbar,
              (AutofillSnackbarType type,
               const CreditCard& filled_card,
               base::OnceClosure),
              (override));
};

class MockAutofillMessageController : public AutofillMessageController {
 public:
  explicit MockAutofillMessageController(content::WebContents* web_contents)
      : AutofillMessageController(web_contents) {}

  MOCK_METHOD(void, Show, (std::unique_ptr<AutofillMessageModel>), (override));
};
#else  //! BUILDFLAG(IS_ANDROID)
class MockSaveCardBubbleController : public SaveCardBubbleControllerImpl {
 public:
  explicit MockSaveCardBubbleController(content::WebContents* web_contents)
      : SaveCardBubbleControllerImpl(web_contents) {}
  ~MockSaveCardBubbleController() override = default;

  MOCK_METHOD(void,
              OfferLocalSave,
              (const CreditCard&,
               payments::PaymentsAutofillClient::SaveCreditCardOptions,
               payments::PaymentsAutofillClient::LocalSaveCardPromptCallback),
              (override));
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

class MockVirtualCardEnrollBubbleController
    : public VirtualCardEnrollBubbleControllerImpl {
 public:
  explicit MockVirtualCardEnrollBubbleController(
      content::WebContents* web_contents)
      : VirtualCardEnrollBubbleControllerImpl(web_contents) {}
  ~MockVirtualCardEnrollBubbleController() override = default;

  MOCK_METHOD(void,
              ShowConfirmationBubbleView,
              (payments::PaymentsAutofillClient::PaymentsRpcResult),
              (override));
};

class ChromePaymentsAutofillClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePaymentsAutofillClientTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableCvcStorageAndFilling,
         features::kAutofillEnablePrefetchingRiskDataForRetrieval},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ChromeAutofillClient::CreateForWebContents(web_contents());
#if BUILDFLAG(IS_ANDROID)
    MockPaymentMethodAccessoryController::GetOrCreate(web_contents())
        ->RegisterFillingSourceObserver(mock_filling_source_observer_.Get());
#endif
    auto mock_virtual_card_bubble_controller =
        std::make_unique<MockVirtualCardEnrollBubbleController>(web_contents());
    const auto* user_data_key =
        mock_virtual_card_bubble_controller->UserDataKey();
    web_contents()->SetUserData(user_data_key,
                                std::move(mock_virtual_card_bubble_controller));
#if !BUILDFLAG(IS_ANDROID)
    auto mock_save_card_bubble_controller =
        std::make_unique<MockSaveCardBubbleController>(web_contents());
    user_data_key = mock_save_card_bubble_controller->UserDataKey();
    web_contents()->SetUserData(user_data_key,
                                std::move(mock_save_card_bubble_controller));
#endif
  }

  ChromeAutofillClient* client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }

  payments::ChromePaymentsAutofillClient* chrome_payments_client() {
    return static_cast<payments::ChromePaymentsAutofillClient*>(
        client()->GetPaymentsAutofillClient());
  }

  MockVirtualCardEnrollBubbleController& virtual_card_bubble_controller() {
    return static_cast<MockVirtualCardEnrollBubbleController&>(
        *VirtualCardEnrollBubbleController::GetOrCreate(web_contents()));
  }
#if BUILDFLAG(IS_ANDROID)
  // Injects a new MockAutofillSaveCardBottomSheetBridge and returns a pointer
  // to the mock.
  MockAutofillSaveCardBottomSheetBridge*
  InjectMockAutofillSaveCardBottomSheetBridge() {
    std::unique_ptr<MockAutofillSaveCardBottomSheetBridge> mock =
        std::make_unique<MockAutofillSaveCardBottomSheetBridge>();
    MockAutofillSaveCardBottomSheetBridge* pointer = mock.get();
    chrome_payments_client()->SetAutofillSaveCardBottomSheetBridgeForTesting(
        std::move(mock));
    return pointer;
  }

  MockAutofillSaveIbanBottomSheetBridge*
  InjectMockAutofillSaveIbanBottomSheetBridge() {
    std::unique_ptr<MockAutofillSaveIbanBottomSheetBridge> mock =
        std::make_unique<MockAutofillSaveIbanBottomSheetBridge>();
    MockAutofillSaveIbanBottomSheetBridge* pointer = mock.get();
    chrome_payments_client()->SetAutofillSaveIbanBottomSheetBridgeForTesting(
        std::move(mock));
    return pointer;
  }

  MockAutofillSnackbarControllerImpl*
  InjectMockAutofillSnackbarControllerImpl() {
    std::unique_ptr<MockAutofillSnackbarControllerImpl> mock =
        std::make_unique<MockAutofillSnackbarControllerImpl>(web_contents());
    MockAutofillSnackbarControllerImpl* pointer = mock.get();
    client()->SetAutofillSnackbarControllerImplForTesting(std::move(mock));
    return pointer;
  }

  MockAutofillMessageController* InjectMockAutofillMessageController() {
    std::unique_ptr<MockAutofillMessageController> mock =
        std::make_unique<MockAutofillMessageController>(web_contents());
    MockAutofillMessageController* pointer = mock.get();
    chrome_payments_client()->SetAutofillMessageControllerForTesting(
        std::move(mock));
    return pointer;
  }

  MockTouchToFillPaymentMethodController*
  InjectMockTouchToFillPaymentMethodController() {
    std::unique_ptr<MockTouchToFillPaymentMethodController> mock =
        std::make_unique<MockTouchToFillPaymentMethodController>();
    MockTouchToFillPaymentMethodController* pointer = mock.get();
    chrome_payments_client()->SetTouchToFillPaymentMethodControllerForTesting(
        std::move(mock));
    return pointer;
  }

  void InjectFeatureEngagementMockTracker() {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          auto tracker =
              std::make_unique<feature_engagement::test::MockTracker>();

          ON_CALL(*tracker, IsInitialized()).WillByDefault(Return(true));

          return tracker;
        }));
  }

#else  // !BUILDFLAG(IS_ANDROID)
  MockSaveCardBubbleController& save_card_bubble_controller() {
    return static_cast<MockSaveCardBubbleController&>(
        *SaveCardBubbleController::GetOrCreate(web_contents()));
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_ANDROID)
  base::MockCallback<AccessoryController::FillingSourceObserver>
      mock_filling_source_observer_;
#endif
};
#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromePaymentsAutofillClientTest,
       GetOrCreateAutofillSaveCardBottomSheetBridge_IsNotNull) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  TestTabModel tab_model(profile());
  tab_model.SetWebContentsList({web_contents()});
  TabModelList::AddTabModel(&tab_model);

  EXPECT_NE(
      chrome_payments_client()->GetOrCreateAutofillSaveCardBottomSheetBridge(),
      nullptr);

  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(ChromePaymentsAutofillClientTest,
       ShowSaveCreditCardLocally_CardSaveOnly_RequestsBottomSheet) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();

  EXPECT_CALL(
      *save_card_bridge,
      RequestShowContent(
          AllOf(
              Field(&AutofillSaveCardUiInfo::is_for_upload, false),
              Field(&AutofillSaveCardUiInfo::description_text,
                    u"To pay faster next time, save your card to your device")),
          NotNull()));

  chrome_payments_client()->ShowSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveOnly)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ShowSaveCreditCardLocally_CardSaveWithCvc_RequestsBottomSheet) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();

  EXPECT_CALL(*save_card_bridge,
              RequestShowContent(
                  AllOf(Field(&AutofillSaveCardUiInfo::is_for_upload, false),
                        Field(&AutofillSaveCardUiInfo::description_text,
                              u"To pay faster next time, save your card and "
                              u"encrypted security code to your device")),
                  NotNull()));

  chrome_payments_client()->ShowSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ShowSaveCreditCardLocally_WindowNotSet_DoesNotFail) {
  EXPECT_NO_FATAL_FAILURE(chrome_payments_client()->ShowSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_show_prompt(true),
      base::DoNothing()));
}

// Verify that the prompt to upload save a user's card without CVC is shown in a
// bottom sheet.
TEST_F(ChromePaymentsAutofillClientTest,
       ShowSaveCreditCardToCloud_CardSaveTypeIsOnlyCard_RequestsBottomSheet) {
  MockAutofillSaveCardBottomSheetBridge* bottom_sheet_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();

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
                  NotNull()));

  chrome_payments_client()->ShowSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveOnly)
          .with_show_prompt(true),
      base::DoNothing());
}

// Verify that the prompt to upload save a user's card with CVC is shown in a
// bottom sheet.
TEST_F(ChromePaymentsAutofillClientTest,
       ShowSaveCreditCardToCloud_CardSaveTypeIsWithCvc_RequestsBottomSheet) {
  MockAutofillSaveCardBottomSheetBridge* bottom_sheet_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();

  std::u16string expected_description;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_description =
      u"Pay faster when your card is saved. Card details are encrypted in "
      u"your Google Account.";
#endif

  // Verify that `AutofillSaveCardUiInfo` has the correct attributes that
  // indicate upload save card prompt with CVC.
  EXPECT_CALL(*bottom_sheet_bridge,
              RequestShowContent(
                  AllOf(Field(&AutofillSaveCardUiInfo::is_for_upload, true),
                        Field(&AutofillSaveCardUiInfo::description_text,
                              expected_description)),
                  NotNull()));

  chrome_payments_client()->ShowSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ShowSaveCreditCardToCloud_DoesNotFailWithoutAWindow) {
  EXPECT_NO_FATAL_FAILURE(chrome_payments_client()->ShowSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_show_prompt(true),
      base::DoNothing()));
}

TEST_F(
    ChromePaymentsAutofillClientTest,
    CreditCardUploadCompletedSuccessful_CallsSaveCardBottomSheetBridgeAndSnackbarControllerWithDurationAndCallback) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  EXPECT_CALL(*save_card_bridge, Hide);
  EXPECT_CALL(
      *snackbar_controller,
      ShowWithDurationAndCallback(AutofillSnackbarType::kSaveCardSuccess,
                                  payments::ChromePaymentsAutofillClient::
                                      kSaveCardConfirmationSnackbarDuration,
                                  _, Ne(std::nullopt)));

  std::optional<base::OnceClosure> callback = base::OnceClosure();
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(callback));
}

TEST_F(
    ChromePaymentsAutofillClientTest,
    CreditCardUploadCompletedSuccessfulButNoCallback_CallsSaveCardBottomSheetBridgeAndSnackbarControllerWithoutDuration) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  EXPECT_CALL(*save_card_bridge, Hide);
  EXPECT_CALL(*snackbar_controller,
              Show(AutofillSnackbarType::kSaveCardSuccess, _));

  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::nullopt);
}

TEST_F(
    ChromePaymentsAutofillClientTest,
    CreditCardUploadCompletedFailure_CallsSaveCardBottomSheetBridgeAndAutofillMessageController) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();
  MockAutofillMessageController* message_controller =
      InjectMockAutofillMessageController();

  EXPECT_CALL(*save_card_bridge, Hide);
  EXPECT_CALL(*message_controller, Show)
      .WillOnce([](std::unique_ptr<AutofillMessageModel> model) {
        EXPECT_EQ(model->GetType(),
                  AutofillMessageModel::Type::kSaveCardFailure);
      });

  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      std::nullopt);
}

TEST_F(
    ChromePaymentsAutofillClientTest,
    CreditCardUploadCompletedOnClientSideTimeout_HidesSaveCardBottomSheetBridge_NoErrorConfirmation) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();
  MockAutofillMessageController* message_controller =
      InjectMockAutofillMessageController();

  EXPECT_CALL(*save_card_bridge, Hide);
  EXPECT_CALL(*message_controller, Show).Times(0);

  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout,
      std::nullopt);
}

TEST_F(ChromePaymentsAutofillClientTest,
       VirtualCardEnrollSuccessful_CallsSnackbarController) {
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  EXPECT_CALL(*snackbar_controller,
              Show(AutofillSnackbarType::kVirtualCardEnrollSuccess, _));

  chrome_payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

TEST_F(ChromePaymentsAutofillClientTest,
       VirtualCardEnrollFailure_CallsAutofillMessageController) {
  MockAutofillMessageController* message_controller =
      InjectMockAutofillMessageController();
  test_api(virtual_card_bubble_controller())
      .SetUiModel(std::make_unique<VirtualCardEnrollUiModel>(
          VirtualCardEnrollmentFields()));
  EXPECT_CALL(*message_controller, Show)
      .WillOnce([](std::unique_ptr<AutofillMessageModel> model) {
        EXPECT_EQ(model->GetType(),
                  AutofillMessageModel::Type::kVirtualCardEnrollFailure);
      });

  chrome_payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);
}

TEST_F(
    ChromePaymentsAutofillClientTest,
    VirtualCardEnrollClientSideTimeout_DoesNotCallAutofillMessageController) {
  MockAutofillMessageController* message_controller =
      InjectMockAutofillMessageController();
  EXPECT_CALL(*message_controller, Show).Times(0);

  chrome_payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout);
}

TEST_F(ChromePaymentsAutofillClientTest,
       GetOrCreateAutofillSaveIbanBottomSheetBridge_IsNotNull) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents()->GetNativeView());

  TestTabModel tab_model(profile());
  tab_model.SetWebContentsList({web_contents()});
  TabModelList::AddTabModel(&tab_model);

  EXPECT_NE(
      chrome_payments_client()->GetOrCreateAutofillSaveIbanBottomSheetBridge(),
      nullptr);

  TabModelList::RemoveTabModel(&tab_model);
}

TEST_F(
    ChromePaymentsAutofillClientTest,
    IbanUploadCompletedSuccessful_CallsSaveIbanBottomSheetBridgeAndSnackbarController) {
  MockAutofillSaveIbanBottomSheetBridge* save_iban_bridge =
      InjectMockAutofillSaveIbanBottomSheetBridge();
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  EXPECT_CALL(*save_iban_bridge, Hide);
  EXPECT_CALL(*snackbar_controller,
              Show(AutofillSnackbarType::kSaveServerIbanSuccess, _));
  chrome_payments_client()->IbanUploadCompleted(/*iban_saved=*/true,
                                                /*max_strikes=*/false);
}

TEST_F(ChromePaymentsAutofillClientTest,
       OnCardDataAvailable_BnplCard_ShowsBnplSnackbar) {
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card.set_issuer_id(kBnplAffirmIssuerId);
  card.set_is_bnpl_card(true);

  FilledCardInformationBubbleOptions options;
  options.filled_card = card;

  EXPECT_CALL(
      *snackbar_controller,
      ShowPaymentsSnackbar(AutofillSnackbarType::kBnpl, options.filled_card, _))
      .WillOnce([=](AutofillSnackbarType type, const CreditCard& card,
                    base::OnceClosure callback) {
        snackbar_controller
            ->AutofillSnackbarControllerImpl::ShowPaymentsSnackbar(
                type, card, std::move(callback));
      });
  EXPECT_CALL(*snackbar_controller, Show(AutofillSnackbarType::kBnpl, _));

  chrome_payments_client()->OnCardDataAvailable(options);
}

TEST_F(ChromePaymentsAutofillClientTest,
       OnCardDataAvailable_VirtualCard_ShowsVirtualCardSnackbar) {
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::RecordType::kVirtualCard);

  FilledCardInformationBubbleOptions options;
  options.filled_card = card;

  EXPECT_CALL(*snackbar_controller,
              ShowPaymentsSnackbar(AutofillSnackbarType::kVirtualCard,
                                   options.filled_card, _))
      .WillOnce([=](AutofillSnackbarType type, const CreditCard& card,
                    base::OnceClosure callback) {
        snackbar_controller
            ->AutofillSnackbarControllerImpl::ShowPaymentsSnackbar(
                type, card, std::move(callback));
      });
  EXPECT_CALL(*snackbar_controller,
              Show(AutofillSnackbarType::kVirtualCard, _));

  chrome_payments_client()->OnCardDataAvailable(options);
}

TEST_F(ChromePaymentsAutofillClientTest,
       OnCardDataAvailable_ShowsCardInfoRetrievalSnackbar) {
  MockAutofillSnackbarControllerImpl* snackbar_controller =
      InjectMockAutofillSnackbarControllerImpl();

  CreditCard card = test::GetCreditCard();
  card.set_record_type(CreditCard::RecordType::kMaskedServerCard);

  FilledCardInformationBubbleOptions options;
  options.filled_card = card;

  EXPECT_CALL(*snackbar_controller,
              ShowPaymentsSnackbar(AutofillSnackbarType::kCardInfoRetrieval,
                                   options.filled_card, _))
      .WillOnce([=](AutofillSnackbarType type, const CreditCard& card,
                    base::OnceClosure callback) {
        snackbar_controller
            ->AutofillSnackbarControllerImpl::ShowPaymentsSnackbar(
                type, card, std::move(callback));
      });
  EXPECT_CALL(*snackbar_controller,
              Show(AutofillSnackbarType::kCardInfoRetrieval, _));

  chrome_payments_client()->OnCardDataAvailable(options);
}

// Test that calling `ShowLoyaltyCards` passes the correct lists of loyalty
// cards.
TEST_F(ChromePaymentsAutofillClientTest, ShowTouchToFillLoyaltyCard) {
  MockTouchToFillPaymentMethodController* ttf_payment_method_controller =
      InjectMockTouchToFillPaymentMethodController();

  const LoyaltyCard affiliated_card_1 = LoyaltyCard(
      /*loyalty_card_id=*/ValuableId("id_2"),
      /*merchant_name=*/"Walgreens",
      /*program_name=*/"CustomerCard",
      /*program_logo=*/GURL(""),
      /*loyalty_card_number=*/"998766823", {GURL("https://example.com")});
  const LoyaltyCard affiliated_card_2 =
      LoyaltyCard(/*loyalty_card_id=*/ValuableId("id_3"),
                  /*merchant_name=*/"Ticket Maester",
                  /*program_name=*/"TourLoyal",
                  /*program_logo=*/GURL(""),
                  /*loyalty_card_number=*/"37262999281",
                  {GURL("https://affiliated.example.com")});
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com"));

  const std::vector<LoyaltyCard> cards = {CreateLoyaltyCard(),
                                          affiliated_card_1, affiliated_card_2};
  EXPECT_CALL(
      *ttf_payment_method_controller,
      ShowLoyaltyCards(_, _, ElementsAre(affiliated_card_1, affiliated_card_2),
                       ElementsAreArray(cards), _));

  chrome_payments_client()->ShowTouchToFillLoyaltyCard(/*delegate=*/nullptr,
                                                       cards);
}

// Test that calling ShowLoyaltyCards checks/updates for IPH status correctly if
// IPH was never shown before.
TEST_F(ChromePaymentsAutofillClientTest,
       ShowTouchToFillLoyaltyCardFirstTimeUsage) {
  MockTouchToFillPaymentMethodController* ttf_payment_method_controller =
      InjectMockTouchToFillPaymentMethodController();
  InjectFeatureEngagementMockTracker();
  feature_engagement::test::MockTracker* tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(
              Profile::FromBrowserContext(
                  web_contents()->GetBrowserContext())));
  ON_CALL(*tracker,
          WouldTriggerHelpUI(
              Ref(feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature)))
      .WillByDefault(Return(true));
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com"));

  EXPECT_CALL(*ttf_payment_method_controller,
              ShowLoyaltyCards(_, _, _, _,
                               /*first_time_usage=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*tracker,
              NotifyEvent("keyboard_accessory_loyalty_cards_autofilled"));

  chrome_payments_client()->ShowTouchToFillLoyaltyCard(/*delegate=*/nullptr,
                                                       {CreateLoyaltyCard()});
}

// Test that calling ShowLoyaltyCards does not update IPH status if IPH already
// shown.
TEST_F(ChromePaymentsAutofillClientTest,
       ShowTouchToFillLoyaltyCardNotFirstTimeUsage) {
  MockTouchToFillPaymentMethodController* ttf_payment_method_controller =
      InjectMockTouchToFillPaymentMethodController();
  InjectFeatureEngagementMockTracker();
  feature_engagement::test::MockTracker* tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserContext(
              Profile::FromBrowserContext(
                  web_contents()->GetBrowserContext())));
  ON_CALL(*tracker,
          WouldTriggerHelpUI(
              Ref(feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature)))
      .WillByDefault(Return(false));
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://example.com"));

  EXPECT_CALL(*ttf_payment_method_controller,
              ShowLoyaltyCards(_, _, _, _,
                               /*first_time_usage=*/false))
      .WillOnce(Return(true));
  EXPECT_CALL(*tracker,
              NotifyEvent("keyboard_accessory_loyalty_cards_autofilled"))
      .Times(0);

  chrome_payments_client()->ShowTouchToFillLoyaltyCard(/*delegate=*/nullptr,
                                                       {CreateLoyaltyCard()});
}

TEST_F(ChromePaymentsAutofillClientTest, ShowTouchToFillBnplIssuers) {
  MockTouchToFillPaymentMethodController* ttf_payment_method_controller =
      InjectMockTouchToFillPaymentMethodController();
  const std::vector<payments::BnplIssuerContext> issuer_context = {
      payments::BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(),
          payments::BnplIssuerEligibilityForPage::kIsEligible)};

  EXPECT_CALL(*ttf_payment_method_controller,
              ShowBnplIssuers(ElementsAre(EqualsBnplIssuerContext(
                                  issuer_context[0].issuer.issuer_id(),
                                  issuer_context[0].eligibility)),
                              /*app_locale=*/"en-US", _, _));

  chrome_payments_client()->ShowTouchToFillBnplIssuers(
      issuer_context, /*app_locale=*/"en-US",
      /*selected_issuer_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest, OnPurchaseAmountExtracted) {
  MockTouchToFillPaymentMethodController* ttf_payment_method_controller =
      InjectMockTouchToFillPaymentMethodController();
  std::optional<int64_t> extracted_amount = 12345;
  std::optional<std::string> app_locale = "en-US";
  const std::vector<payments::BnplIssuerContext> issuer_context = {
      payments::BnplIssuerContext(
          test::GetTestLinkedBnplIssuer(),
          payments::BnplIssuerEligibilityForPage::kIsEligible)};

  EXPECT_CALL(
      *ttf_payment_method_controller,
      OnPurchaseAmountExtracted(ElementsAre(EqualsBnplIssuerContext(
                                    issuer_context[0].issuer.issuer_id(),
                                    issuer_context[0].eligibility)),
                                extracted_amount,
                                /*is_amount_supported_by_any_issuer=*/true,
                                /*app_locale=*/app_locale, _, _));

  chrome_payments_client()->OnPurchaseAmountExtracted(
      issuer_context, extracted_amount,
      /*is_amount_supported_by_any_issuer=*/true, app_locale,
      /*selected_issuer_callback=*/base::DoNothing(),
      /*cancel_callback=*/base::DoNothing());
}

#else   // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/410047802): Disable test on Linux TSan due to flakiness/issue.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ShowSaveCreditCardLocally_CallsOfferLocalSave \
  DISABLED_ShowSaveCreditCardLocally_CallsOfferLocalSave
#else
#define MAYBE_ShowSaveCreditCardLocally_CallsOfferLocalSave \
  ShowSaveCreditCardLocally_CallsOfferLocalSave
#endif  // BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
TEST_F(ChromePaymentsAutofillClientTest,
       MAYBE_ShowSaveCreditCardLocally_CallsOfferLocalSave) {
  EXPECT_CALL(save_card_bubble_controller(), OfferLocalSave);

  chrome_payments_client()->ShowSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions(),
      base::DoNothing());
}

// Test that calling `CreditCardUploadCompleted` calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView on card upload
// success.
TEST_F(ChromePaymentsAutofillClientTest,
       CreditCardUploadCompletedSuccess_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(true, _));
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::nullopt);
}

// Test that calling `CreditCardUploadCompleted` calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView on card upload
// failure.
TEST_F(ChromePaymentsAutofillClientTest,
       CreditCardUploadCompletedFailure_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(false, _));
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
      std::nullopt);
}

// Test that calling `CreditCardUploadCompleted` does not show failure
// confirmation on card upload client-side timeout.
TEST_F(
    ChromePaymentsAutofillClientTest,
    CreditCardUploadCompletedClientSideTimeout_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(), HideSaveCardBubble());
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(false, _))
      .Times(0);
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kClientSideTimeout,
      std::nullopt);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Verify that the confirmation bubble view is shown after virtual card
// enrollment is completed.
TEST_F(ChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompleted_ShowsConfirmation) {
  EXPECT_CALL(
      virtual_card_bubble_controller(),
      ShowConfirmationBubbleView(
          payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess));
  chrome_payments_client()->VirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

// Test that there is always an PaymentsWindowManager present if attempted
// to be retrieved.
TEST_F(ChromePaymentsAutofillClientTest, GetPaymentsWindowManager) {
    EXPECT_NE(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
}

TEST_F(ChromePaymentsAutofillClientTest, RiskDataCaching_DataCached) {
  base::MockCallback<base::OnceCallback<void(const std::string&)>> callback1;
  base::MockCallback<base::OnceCallback<void(const std::string&)>> callback2;
  chrome_payments_client()->SetCachedRiskDataLoadedCallbackForTesting(
      callback1.Get());
  chrome_payments_client()->SetRiskDataForTesting("risk_data");

  EXPECT_CALL(callback1, Run("risk_data")).Times(1);
  EXPECT_CALL(callback2, Run).Times(0);

  chrome_payments_client()->LoadRiskData(callback2.Get());
}

// Test that BNPL strategy is created and returned correctly.
TEST_F(ChromePaymentsAutofillClientTest, GetBnplStrategy) {
  payments::BnplStrategy* strategy =
      chrome_payments_client()->GetBnplStrategy();
  ASSERT_NE(strategy, nullptr);

  // Test that the same instance is returned on subsequent calls.
  EXPECT_EQ(strategy, chrome_payments_client()->GetBnplStrategy());
}

// Test that BNPL UI delegate is created and returned correctly.
TEST_F(ChromePaymentsAutofillClientTest, GetBnplUiDelegate) {
  payments::BnplUiDelegate* ui_delegate =
      chrome_payments_client()->GetBnplUiDelegate();
  ASSERT_NE(ui_delegate, nullptr);

  // Test that the same instance is returned on subsequent calls.
  EXPECT_EQ(ui_delegate, chrome_payments_client()->GetBnplUiDelegate());
}

// Test that `DisablePaymentsAutofill` correctly disables the client's support
// for autofill payment methods.
TEST_F(ChromePaymentsAutofillClientTest, DisablePaymentsAutofill) {
  EXPECT_TRUE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());

  chrome_payments_client()->DisablePaymentsAutofill();

  EXPECT_FALSE(chrome_payments_client()->IsAutofillPaymentMethodsEnabled());
}

#if !BUILDFLAG(IS_ANDROID)
class ChromePaymentsAutofillIOSPromoClientTest
    : public ChromePaymentsAutofillClientTest {
 public:
  ChromePaymentsAutofillIOSPromoClientTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableCvcStorageAndFilling,
         features::kAutofillEnablePrefetchingRiskDataForRetrieval},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that calling `CreditCardUploadCompleted` still calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView on card upload
// success as callback, after failing to show the iOS promo.
TEST_F(ChromePaymentsAutofillIOSPromoClientTest,
       IOSPaymentPromoFailedToShow_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(),
              ShowConfirmationBubbleView(true, _));
  chrome_payments_client()->CreditCardUploadCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::nullopt);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill
