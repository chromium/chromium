// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"

#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using optimization_guide::model_execution::prefs::
    ModelExecutionEnterprisePolicyValue;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
struct GetAutofillPredictionSettingsPolicyParam {
  ModelExecutionEnterprisePolicyValue policy =
      ModelExecutionEnterprisePolicyValue::kAllow;
  bool expectation = true;
};

class AutofillPredictionSettingsPolicyTest
    : public testing::TestWithParam<GetAutofillPredictionSettingsPolicyParam> {
 public:
  TestingPrefServiceSimple& prefs() { return prefs_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kAutofillPredictionImprovements};
  TestingPrefServiceSimple prefs_;
};

INSTANTIATE_TEST_SUITE_P(
    AutofillPredictionImprovementsFeaturesTest,
    AutofillPredictionSettingsPolicyTest,
    testing::Values(
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kAllow,
            .expectation = true},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging,
            .expectation = true},
        GetAutofillPredictionSettingsPolicyParam{
            .policy = ModelExecutionEnterprisePolicyValue::kDisable,
            .expectation = false}));

TEST_P(AutofillPredictionSettingsPolicyTest,
       IsAutofillPredictionImprovementsSupported) {
  const char* kEnterprisePref = optimization_guide::prefs::
      kAutofillPredictionImprovementsEnterprisePolicyAllowed;

  prefs().registry()->RegisterIntegerPref(kEnterprisePref, 0);

  prefs().SetManagedPref(kEnterprisePref,
                         base::Value(base::to_underlying(GetParam().policy)));

  EXPECT_EQ(IsAutofillPredictionImprovementsSupported(&prefs()),
            GetParam().expectation);
}
#endif

}  // namespace
}  // namespace autofill_prediction_improvements
