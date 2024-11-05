// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_features.h"

#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {
namespace {

using optimization_guide::model_execution::prefs::
    ModelExecutionEnterprisePolicyValue;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
struct GetAutofillPredictionSettingsPolicyParam {
  ModelExecutionEnterprisePolicyValue policy =
      ModelExecutionEnterprisePolicyValue::kAllow;
  bool autofill_enabled = true;
  bool expectation = true;
};

class AutofillPredictionSettingsPolicyTest
    : public testing::TestWithParam<GetAutofillPredictionSettingsPolicyParam> {
 public:
  TestingPrefServiceSimple& prefs() { return prefs_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{kAutofillAi};
  TestingPrefServiceSimple prefs_;
};

INSTANTIATE_TEST_SUITE_P(
    AutofillAiFeaturesTest,
    AutofillPredictionSettingsPolicyTest,
    testing::Values(
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kAllow,
            .autofill_enabled = true,
            .expectation = true},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging,
            .autofill_enabled = true,
            .expectation = true},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kDisable,
            .autofill_enabled = true,
            .expectation = false},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kAllow,
            .autofill_enabled = false,
            .expectation = false},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging,
            .autofill_enabled = false,
            .expectation = false},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kDisable,
            .autofill_enabled = false,
            .expectation = false}));

TEST_P(AutofillPredictionSettingsPolicyTest, IsAutofillAiSupported) {
  const char* kEnterprisePref = optimization_guide::prefs::
      kAutofillPredictionImprovementsEnterprisePolicyAllowed;
  const char* kAutofillPref = autofill::prefs::kAutofillProfileEnabled;

  prefs().registry()->RegisterIntegerPref(kEnterprisePref, 0);
  prefs().registry()->RegisterBooleanPref(kAutofillPref, true);

  prefs().SetManagedPref(kEnterprisePref,
                         base::Value(base::to_underlying(GetParam().policy)));
  prefs().SetUserPref(kAutofillPref, base::Value(GetParam().autofill_enabled));

  EXPECT_EQ(IsAutofillAiSupported(&prefs()), GetParam().expectation);
}
#endif

}  // namespace
}  // namespace autofill_ai
