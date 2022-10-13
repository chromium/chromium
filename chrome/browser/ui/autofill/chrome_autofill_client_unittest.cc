// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ScopedFeatureList;

namespace autofill {

class ChromeAutofillClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PreparePersonalDataManager();
    ChromeAutofillClient::CreateForWebContents(web_contents());
    chrome_autofill_client_ =
        ChromeAutofillClient::FromWebContents(web_contents());
  }

 protected:
  ChromeAutofillClient* client() { return chrome_autofill_client_; }
  TestPersonalDataManager* personal_data_manager() {
    return personal_data_manager_;
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

    // Enable MSBB by default. If MSBB has been explicitly turned off, Fast
    // Checkout is not supported.
    profile()->GetPrefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  raw_ptr<ChromeAutofillClient> chrome_autofill_client_ = nullptr;
  raw_ptr<TestPersonalDataManager> personal_data_manager_ = nullptr;
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
TEST_F(ChromeAutofillClientTest, IsFastCheckoutSupportedWithDisabledFeature) {
  ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(::features::kFastCheckout);

  EXPECT_FALSE(client()->IsFastCheckoutSupported());
}

TEST_F(ChromeAutofillClientTest,
       IsFastCheckoutSupportedWithDisabledAssistantFeature) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kFastCheckout},
      {autofill_assistant::features::kAutofillAssistant});

  EXPECT_FALSE(client()->IsFastCheckoutSupported());
}

TEST_F(ChromeAutofillClientTest,
       IsFastCheckoutSupportedWithDisabledPersonalDataManager) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kFastCheckout,
       autofill_assistant::features::kAutofillAssistant},
      {});

  personal_data_manager()->SetAutofillCreditCardEnabled(false);
  EXPECT_FALSE(client()->IsFastCheckoutSupported());

  personal_data_manager()->SetAutofillCreditCardEnabled(true);
  personal_data_manager()->SetAutofillProfileEnabled(false);
  EXPECT_FALSE(client()->IsFastCheckoutSupported());
}

TEST_F(ChromeAutofillClientTest, NoFastCheckoutSupportWithDisabledMSBB) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {::features::kFastCheckout,
       autofill_assistant::features::kAutofillAssistant},
      {});

  // If MSBB has been explicitly turned off, Fast Checkout is not supported.
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  EXPECT_FALSE(client()->IsFastCheckoutSupported());
}

TEST_F(ChromeAutofillClientTest,
       IsFastCheckoutSupportedWithConsentAndDisabledAutofillAssistantPref) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{::features::kFastCheckout,
        {{::features::kFastCheckoutConsentlessExecutionParam.name, "false"}}},
       {autofill_assistant::features::kAutofillAssistant, {}}},
      {});

  // If a user requires consent and Autofill Assistant has been explicitly
  // turned off, Fast Checkout is not supported.
  profile()->GetPrefs()->SetBoolean(
      autofill_assistant::prefs::kAutofillAssistantEnabled, false);

  EXPECT_FALSE(client()->IsFastCheckoutSupported());
}

TEST_F(ChromeAutofillClientTest,
       IsFastCheckoutSupportedWithoutConsentAndDisabledAutofillAssistantPref) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{::features::kFastCheckout,
        {{::features::kFastCheckoutConsentlessExecutionParam.name, "true"}}},
       {autofill_assistant::features::kAutofillAssistant, {}}},
      {});

  // If a user does not require consent, the Autofill Assistant pref is ignored.
  profile()->GetPrefs()->SetBoolean(
      autofill_assistant::prefs::kAutofillAssistantEnabled, false);

  EXPECT_TRUE(client()->IsFastCheckoutSupported());
}

TEST_F(ChromeAutofillClientTest, IsFastCheckoutSupportedWithConsent) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{::features::kFastCheckout,
        {{::features::kFastCheckoutConsentlessExecutionParam.name, "false"}}},
       {autofill_assistant::features::kAutofillAssistant, {}}},
      {});

  EXPECT_TRUE(client()->IsFastCheckoutSupported());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
