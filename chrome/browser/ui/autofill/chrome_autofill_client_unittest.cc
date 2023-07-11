// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#endif

namespace autofill {
namespace {

using ::testing::InSequence;

#if BUILDFLAG(IS_ANDROID)
class MockAutofillSaveCardBottomSheetBridge
    : public AutofillSaveCardBottomSheetBridge {
 public:
  MockAutofillSaveCardBottomSheetBridge()
      : AutofillSaveCardBottomSheetBridge(
            base::android::ScopedJavaGlobalRef<jobject>(nullptr)) {}

  MOCK_METHOD(bool, RequestShowContent, (), (override));
};
#endif

// Exposes the protected constructor.
class TestChromeAutofillClient : public ChromeAutofillClient {
 public:
  explicit TestChromeAutofillClient(content::WebContents* web_contents)
      : ChromeAutofillClient(web_contents) {}

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
    SetAutofillSaveCardBottomSheetBridgeForTesting(std::move(mock));
    return pointer;
  }

  MockFastCheckoutClient fast_checkout_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
#endif
};

class ChromeAutofillClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PreparePersonalDataManager();
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));
  }

  void TearDown() override {
    // Avoid that the raw pointer becomes dangling.
    personal_data_manager_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TestChromeAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  TestPersonalDataManager* personal_data_manager() {
    return personal_data_manager_;
  }

  TestContentAutofillDriver* autofill_driver() {
    return test_autofill_driver_injector_[web_contents()];
  }

  TestBrowserAutofillManager* autofill_manager() {
    return test_autofill_manager_injector_[web_contents()];
  }

 private:
  void PreparePersonalDataManager() {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                profile(), base::BindRepeating([](content::BrowserContext*) {
                  return std::make_unique<TestPersonalDataManager>();
                }));

    personal_data_manager_->SetAutofillProfileEnabled(true);
    personal_data_manager_->SetAutofillCreditCardEnabled(true);
    personal_data_manager_->SetAutofillWalletImportEnabled(false);

    // Enable MSBB by default. If MSBB has been explicitly turned off, Fast
    // Checkout is not supported.
    profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  raw_ptr<TestPersonalDataManager> personal_data_manager_ = nullptr;
  TestAutofillClientInjector<TestChromeAutofillClient>
      test_autofill_client_injector_;
  TestAutofillDriverInjector<TestContentAutofillDriver>
      test_autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      test_autofill_manager_injector_;

  base::OnceCallback<void()> setup_flags_;
};

TEST_F(ChromeAutofillClientTest, GetFormInteractionsFlowId_BelowMaxFlowTime) {
  // Arbitrary fixed date to avoid using Now().
  base::Time july_2022 = base::Time::FromDoubleT(1658620440);
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
  base::Time july_2022 = base::Time::FromDoubleT(1658620440);
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
  base::Time july_2022 = base::Time::FromDoubleT(1658620440);
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

#if BUILDFLAG(IS_ANDROID)
class ChromeAutofillClientTestWithPaymentsAndroidBottomSheetFeature
    : public ChromeAutofillClientTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnablePaymentsAndroidBottomSheet};
};

TEST_F(ChromeAutofillClientTestWithPaymentsAndroidBottomSheetFeature,
       ConfirmSaveCreditCardToCloud_RequestsBottomSheet) {
  TestChromeAutofillClient* autofill_client = client();
  auto* bottom_sheet_bridge =
      autofill_client->InjectMockAutofillSaveCardBottomSheetBridge();

  EXPECT_CALL(*bottom_sheet_bridge, RequestShowContent());

  autofill_client->ConfirmSaveCreditCardToCloud(
      CreditCard(), LegalMessageLines(),
      ChromeAutofillClient::SaveCreditCardOptions().with_show_prompt(true),
      base::DoNothing());
}
#endif

}  // namespace
}  // namespace autofill
