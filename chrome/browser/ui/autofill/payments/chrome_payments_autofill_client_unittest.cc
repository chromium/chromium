// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "ui/android/window_android.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

#if BUILDFLAG(IS_ANDROID)
class MockAutofillSaveCardBottomSheetBridge
    : public AutofillSaveCardBottomSheetBridge {
 public:
  MockAutofillSaveCardBottomSheetBridge()
      : AutofillSaveCardBottomSheetBridge(
            base::android::ScopedJavaGlobalRef<jobject>(nullptr)) {}

  MOCK_METHOD(void, Hide, (), (override));
};
#else  //! BUILDFLAG(IS_ANDROID)
class MockSaveCardBubbleController : public SaveCardBubbleControllerImpl {
 public:
  explicit MockSaveCardBubbleController(content::WebContents* web_contents)
      : SaveCardBubbleControllerImpl(web_contents) {}
  ~MockSaveCardBubbleController() override = default;

  MOCK_METHOD(void, ShowConfirmationBubbleView, (bool), (override));
};
#endif

class MockVirtualCardEnrollBubbleController
    : public VirtualCardEnrollBubbleControllerImpl {
 public:
  explicit MockVirtualCardEnrollBubbleController(
      content::WebContents* web_contents)
      : VirtualCardEnrollBubbleControllerImpl(web_contents) {}
  ~MockVirtualCardEnrollBubbleController() override = default;

#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, ShowConfirmationBubbleView, (bool), (override));
#endif
  MOCK_METHOD(bool, IsIconVisible, (), (const override));
};

class ChromePaymentsAutofillClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePaymentsAutofillClientTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
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
}

TEST_F(ChromePaymentsAutofillClientTest,
       CreditCardUploadCompleted_CallsAutofillSaveCardBottomSheetBridge) {
  MockAutofillSaveCardBottomSheetBridge* save_card_bridge =
      InjectMockAutofillSaveCardBottomSheetBridge();
  EXPECT_CALL(*save_card_bridge, Hide());
  chrome_payments_client()->CreditCardUploadCompleted(true);
}
#else   // !BUILDFLAG(IS_ANDROID)
// Verify that confirmation bubble view is shown after virtual card enrollment
// is completed.
TEST_F(ChromePaymentsAutofillClientTest,
       VirtualCardEnrollCompleted_ShowsConfirmation) {
  ON_CALL(virtual_card_bubble_controller(), IsIconVisible())
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(virtual_card_bubble_controller(),
              ShowConfirmationBubbleView(true));
  chrome_payments_client()->VirtualCardEnrollCompleted(true);
}

// Test that calling CreditCardUploadCompleted calls
// SaveCardBubbleControllerImpl::ShowConfirmationBubbleView.
TEST_F(ChromePaymentsAutofillClientTest,
       CreditCardUploadCompleted_CallsShowConfirmationBubbleView) {
  EXPECT_CALL(save_card_bubble_controller(), ShowConfirmationBubbleView(true));
  chrome_payments_client()->CreditCardUploadCompleted(true);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test that there is always an PaymentsWindowManager present if attempted
// to be retrieved.
TEST_F(ChromePaymentsAutofillClientTest, GetPaymentsWindowManager) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    EXPECT_EQ(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
  } else {
    EXPECT_NE(chrome_payments_client()->GetPaymentsWindowManager(), nullptr);
  }
}

}  // namespace autofill
