// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include <optional>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller_impl.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "ui/android/window_android.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::NotNull;

namespace autofill {

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
  MOCK_METHOD(void, Hide, (), (override));
};

class MockAutofillSnackbarControllerImpl
    : public AutofillSnackbarControllerImpl {
 public:
  explicit MockAutofillSnackbarControllerImpl(
      content::WebContents* web_contents)
      : AutofillSnackbarControllerImpl(web_contents) {}

  MOCK_METHOD(void, Show, (AutofillSnackbarType), (override));
  MOCK_METHOD(void,
              ShowWithDurationAndCallback,
              (AutofillSnackbarType,
               base::TimeDelta,
               std::optional<base::OnceClosure>),
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
        {features::kAutofillEnableSaveCardLoadingAndConfirmation,
         features::kAutofillEnableVcnEnrollLoadingAndConfirmation,
         features::kAutofillEnableCvcStorageAndFilling,
         features::kAutofillEnablePrefetchingRiskDataForRetrieval},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ChromeAutofillClient::CreateForWebContents(web_contents());
    auto mock_virtual_card_bubble_controller =
        std::make_unique<MockVirtualCardEnrollBubbleController>(web_contents());
    web_contents()->SetUserData(
        mock_virtual_card_bubble_controller->UserDataKey(),
        std::move(mock_virtual_card_bubble_controller));
#if !BUILDFLAG(IS_ANDROID)
    auto mock_save_card_bubble_controller =
        std::make_unique<MockSaveCardBubbleController>(web_contents());
    web_contents()->SetUserData(mock_save_card_bubble_controller->UserDataKey(),
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

  MockAutofillSnackbarControllerImpl*
  InjectMockAutofillSnackbarControllerImpl() {
    std::unique_ptr<MockAutofillSnackbarControllerImpl> mock =
        std::make_unique<MockAutofillSnackbarControllerImpl>(web_contents());
    MockAutofillSnackbarControllerImpl* pointer = mock.get();
    chrome_payments_client()->SetAutofillSnackbarControllerImplForTesting(
        std::move(mock));
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
#else  // !BUILDFLAG(IS_ANDROID)
  MockSaveCardBubbleController& save_card_bubble_controller() {
    return static_cast<MockSaveCardBubbleController&>(
        *SaveCardBubbleController::GetOrCreate(web_contents()));
  }
#endif

 private:
  base::test::ScopedFeatureList feature_list_;
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
       ConfirmSaveCreditCardLocally_CardSaveOnly_RequestsBottomSheet) {
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

  chrome_payments_client()->ConfirmSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveOnly)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ConfirmSaveCreditCardLocally_CardSaveWithCvc_RequestsBottomSheet) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();

  EXPECT_CALL(*save_card_bridge,
              RequestShowContent(
                  AllOf(Field(&AutofillSaveCardUiInfo::is_for_upload, false),
                        Field(&AutofillSaveCardUiInfo::description_text,
                              u"To pay faster next time, save your card and "
                              u"encrypted security code to your device")),
                  NotNull()));

  chrome_payments_client()->ConfirmSaveCreditCardLocally(
      CreditCard(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ConfirmSaveCreditCardLocally_WindowNotSet_DoesNotFail) {
  EXPECT_NO_FATAL_FAILURE(
      chrome_payments_client()->ConfirmSaveCreditCardLocally(
          CreditCard(),
          payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
              .with_show_prompt(true),
          base::DoNothing()));
}

// Verify that the prompt to upload save a user's card without CVC is shown in a
// bottom sheet.
TEST_F(
    ChromePaymentsAutofillClientTest,
    ConfirmSaveCreditCardToCloud_CardSaveTypeIsOnlyCard_RequestsBottomSheet) {
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
                  testing::NotNull()));

  chrome_payments_client()->ConfirmSaveCreditCardToCloud(
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
       ConfirmSaveCreditCardToCloud_CardSaveTypeIsWithCvc_RequestsBottomSheet) {
  MockAutofillSaveCardBottomSheetBridge* bottom_sheet_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();

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

  chrome_payments_client()->ConfirmSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      payments::ChromePaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(payments::ChromePaymentsAutofillClient::
                                   CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true),
      base::DoNothing());
}

TEST_F(ChromePaymentsAutofillClientTest,
       ConfirmSaveCreditCardToCloud_DoesNotFailWithoutAWindow) {
  EXPECT_NO_FATAL_FAILURE(
      chrome_payments_client()->ConfirmSaveCreditCardToCloud(
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
                                  testing::Ne(std::nullopt)));

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
              Show(AutofillSnackbarType::kSaveCardSuccess));

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
              Show(AutofillSnackbarType::kVirtualCardEnrollSuccess));

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
#else   // !BUILDFLAG(IS_ANDROID)
TEST_F(ChromePaymentsAutofillClientTest,
       ConfirmSaveCreditCardLocally_CallsOfferLocalSave) {
  EXPECT_CALL(save_card_bubble_controller(), OfferLocalSave);

  chrome_payments_client()->ConfirmSaveCreditCardLocally(
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
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    EXPECT_EQ(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
  } else {
    EXPECT_NE(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
  }
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

}  // namespace autofill
