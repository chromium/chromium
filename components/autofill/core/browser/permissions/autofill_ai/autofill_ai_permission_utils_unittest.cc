// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_testing_pref_service.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::Values;

constexpr auto kAutofillPredictionSettingsDisable =
    base::to_underlying(optimization_guide::model_execution::prefs::
                            ModelExecutionEnterprisePolicyValue::kDisable);

std::string GetTestSuffix(
    ::testing::TestParamInfo<AutofillAiAction> param_info) {
  switch (param_info.param) {
    case AutofillAiAction::kAddEntityInstanceInSettings:
      return "kAddEntityInstanceInSettings";
    case AutofillAiAction::kCrowdsourcingVote:
      return "kCrowdsourcingVote";
    case AutofillAiAction::kEditAndDeleteEntityInstanceInSettings:
      return "kEditAndDeleteEntityInstanceInSettings";
    case AutofillAiAction::kFilling:
      return "kFilling";
    case AutofillAiAction::kImport:
      return "kImport";
    case AutofillAiAction::kIphForOptIn:
      return "kIphForOptIn";
    case AutofillAiAction::kListEntityInstancesInSettings:
      return "kListEntityInstancesInSettings";
    case AutofillAiAction::kLogToMqls:
      return "kLogToMqls";
    case AutofillAiAction::kOptIn:
      return "kOptIn";
    case AutofillAiAction::kServerClassificationModel:
      return "kServerClassificationModel";
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return "kUseCachedServerClassificationModelResults";
  }
  NOTREACHED();
}

// A test fixture that sets up default state so that all AutofillAI-related
// actions are permitted.
class AutofillAiPermissionUtilsTest : public ::testing::Test {
 public:
  AutofillAiPermissionUtilsTest() {
    // Features.
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kAutofillAiWithDataSchema, {}},
         {features::kAutofillAiServerModel,
          {{"autofill_ai_model_use_cache_results", "true"}}}},
        {});

    // Pref and identity state.
    client().set_entity_data_manager(std::make_unique<EntityDataManager>(
        webdata_helper_.autofill_webdata_service(), /*history_service=*/nullptr,
        /*strike_database=*/nullptr));
    client().SetUpPrefsAndIdentityForAutofillAi();
  }

  void AddEntity() {
    edm().AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
    webdata_helper_.WaitUntilIdle();
  }

  TestAutofillClient& client() { return client_; }
  EntityDataManager& edm() { return *client().GetEntityDataManager(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClient client_;
};

class AutofillAiMayPerformActionTest
    : public AutofillAiPermissionUtilsTest,
      public ::testing::WithParamInterface<AutofillAiAction> {
 public:
  using AutofillAiPermissionUtilsTest::AutofillAiPermissionUtilsTest;
};

// Verifies that the test fixture sets up the client so that everything but
// opt-in IPH is permitted.
TEST_P(AutofillAiMayPerformActionTest, ActionsWhenEnabled) {
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()),
            GetParam() != AutofillAiAction::kIphForOptIn);
}

// Tests that `kAutofillAiWithDataSchema` is a requirement for all actions.
TEST_P(AutofillAiMayPerformActionTest, ReturnsFalseWhenMainFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiWithDataSchema);

  EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam()));
}

// Tests that the server model cannot be run and its cache cannot be used if
// `kAutofillAiServerModel` is disabled.
TEST_P(AutofillAiMayPerformActionTest, ModelFeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiServerModel);

  // The opt-in IPH cannot be run either since we simulate a state in which the
  // user has opted into the feature.
  const bool is_allowed =
      (GetParam() != AutofillAiAction::kServerClassificationModel) &&
      (GetParam() !=
       AutofillAiAction::kUseCachedServerClassificationModelResults) &&
      (GetParam() != AutofillAiAction::kIphForOptIn);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Tests that the server model cache cannot be used if the feature parameter
// governing it is false.
TEST_P(AutofillAiMayPerformActionTest, FeatureParamForModelCacheUseOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kAutofillAiServerModel,
        {{"autofill_ai_model_use_cache_results", "false"}}}},
      {});

  // The opt-in IPH cannot be run either since we simulate a state in which the
  // user has opted into the feature.
  const bool is_allowed =
      (GetParam() !=
       AutofillAiAction::kUseCachedServerClassificationModelResults) &&
      (GetParam() != AutofillAiAction::kIphForOptIn);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Tests that the opt-in IPH cannot be shown if its feature is off.
TEST_P(AutofillAiMayPerformActionTest, OptInIphFeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      feature_engagement::kIPHAutofillAiOptInFeature);

  SetAutofillAiOptInStatus(client(), false);
  const bool is_allowed =
      GetParam() == AutofillAiAction::kOptIn ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Tests that listing entities is the only action permitted if the
// AutofillAI enterprise policy is disabled regardless of whether data
// is saved in the EntityDataManager.
TEST_P(AutofillAiMayPerformActionTest,
       ActionsWhenAutofillAiEnterprisePolicyDisabled) {
  client().GetPrefs()->SetInteger(
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed,
      kAutofillPredictionSettingsDisable);
  if (GetParam() == AutofillAiAction::kListEntityInstancesInSettings) {
    EXPECT_TRUE(MayPerformAutofillAiAction(client(), GetParam()));
  } else {
    EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam()));
  }
}

// Tests that no action is permitted if address Autofill is disabled and no data
// is saved in the EntityDataManager.
TEST_P(AutofillAiMayPerformActionTest, ActionsWhenAddressAutofillDisabled) {
  client().SetAutofillProfileEnabled(false);
  EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam()));
}

// Tests that listing, editing and removing entities is permitted if address
// Autofill is disabled and there is data is saved in the EntityDataManager.
TEST_P(AutofillAiMayPerformActionTest,
       ActionsWhenAddressAutofillDisabledWithDataSaved) {
  AddEntity();
  client().SetAutofillProfileEnabled(false);
  const bool is_allowed =
      (GetParam() ==
       AutofillAiAction::kEditAndDeleteEntityInstanceInSettings) ||
      (GetParam() == AutofillAiAction::kListEntityInstancesInSettings);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Verifies that IPH, opt-in and list entities are permitted if the user has not
// opted into AutofillAI.
TEST_P(AutofillAiMayPerformActionTest, ActionsWhenNotOptedIntoAutofillAi) {
  SetAutofillAiOptInStatus(client(), false);
  const bool is_allowed =
      (GetParam() == AutofillAiAction::kOptIn) ||
      (GetParam() == AutofillAiAction::kIphForOptIn) ||
      (GetParam() == AutofillAiAction::kListEntityInstancesInSettings);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Tests that listing, editing and removing entities is permitted if user is no
// longer opted into AutofillAI, but there is data saved.
TEST_P(AutofillAiMayPerformActionTest,
       ActionsWhenAutofillNotOptedIntoAutofillAiButDataSaved) {
  AddEntity();
  SetAutofillAiOptInStatus(client(), false);
  const bool is_allowed =
      (GetParam() == AutofillAiAction::kOptIn) ||
      (GetParam() == AutofillAiAction::kIphForOptIn) ||
      (GetParam() ==
       AutofillAiAction::kEditAndDeleteEntityInstanceInSettings) ||
      (GetParam() == AutofillAiAction::kListEntityInstancesInSettings);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
// Tests that every action other than listing and editing data requires the user
// to be signed in.
TEST_P(AutofillAiMayPerformActionTest, SignedOut) {
  AddEntity();
  client().identity_test_environment().ClearPrimaryAccount();
  const bool is_allowed =
      (GetParam() ==
       AutofillAiAction::kEditAndDeleteEntityInstanceInSettings) ||
      (GetParam() == AutofillAiAction::kListEntityInstancesInSettings);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Tests that every action other than listing and editing data requires that
// user's account capabilities include running a model.
TEST_P(AutofillAiMayPerformActionTest, MayNotRunModel) {
  AddEntity();
  client().SetCanUseModelExecutionFeatures(false);
  const bool is_allowed =
      (GetParam() ==
       AutofillAiAction::kEditAndDeleteEntityInstanceInSettings) ||
      (GetParam() == AutofillAiAction::kListEntityInstancesInSettings);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Tests that only filling and cache use are allowed off-the-record.
TEST_P(AutofillAiMayPerformActionTest, OffTheRecord) {
  client().set_is_off_the_record(true);
  const bool is_allowed =
      (GetParam() == AutofillAiAction::kFilling) ||
      (GetParam() ==
       AutofillAiAction::kUseCachedServerClassificationModelResults);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

TEST_P(AutofillAiMayPerformActionTest, CountryCode) {
  client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));
  EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam()));
}

TEST_P(AutofillAiMayPerformActionTest, AppLocale) {
  client().set_app_locale("de-DE");
  EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam()));
}

// Tests that listing, editing and removing entities is permitted even if the
// app locale is unsupported as long as there is data saved.
TEST_P(AutofillAiMayPerformActionTest, AppLocaleWithDataSaved) {
  AddEntity();
  client().set_app_locale("de-DE");
  const bool is_allowed =
      (GetParam() ==
       AutofillAiAction::kEditAndDeleteEntityInstanceInSettings) ||
      (GetParam() == AutofillAiAction::kListEntityInstancesInSettings);
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillAiMayPerformActionTest,
    Values(AutofillAiAction::kAddEntityInstanceInSettings,
           AutofillAiAction::kCrowdsourcingVote,
           AutofillAiAction::kEditAndDeleteEntityInstanceInSettings,
           AutofillAiAction::kFilling,
           AutofillAiAction::kImport,
           AutofillAiAction::kIphForOptIn,
           AutofillAiAction::kListEntityInstancesInSettings,
           AutofillAiAction::kLogToMqls,
           AutofillAiAction::kOptIn,
           AutofillAiAction::kServerClassificationModel,
           AutofillAiAction::kUseCachedServerClassificationModelResults),
    GetTestSuffix);

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
// Tests that opt-in status is tied to a GAIA id.
TEST_F(AutofillAiPermissionUtilsTest, Muh) {
  const std::string initial_email =
      client()
          .GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  const std::string other_email = "something_else@gmail.com";
  ASSERT_NE(initial_email, other_email);

  // The initially signed in account is opted in.
  EXPECT_TRUE(GetAutofillAiOptInStatus(client()));

  // Signed out clients are never opted in.
  client().identity_test_environment().ClearPrimaryAccount();
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));

  // After signing in with a different account, the opt-in is gone.
  client().identity_test_environment().MakePrimaryAccountAvailable(
      other_email, signin::ConsentLevel::kSignin);
  client().SetCanUseModelExecutionFeatures(true);
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));
  EXPECT_TRUE(SetAutofillAiOptInStatus(client(), true));
  EXPECT_TRUE(GetAutofillAiOptInStatus(client()));

  // Switch back to the old account and the old opt-in is back.
  client().identity_test_environment().ClearPrimaryAccount();
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));
  client().identity_test_environment().MakePrimaryAccountAvailable(
      initial_email, signin::ConsentLevel::kSignin);
  client().SetCanUseModelExecutionFeatures(true);
  EXPECT_TRUE(GetAutofillAiOptInStatus(client()));

  // Setting it to `false` works as well.
  EXPECT_TRUE(SetAutofillAiOptInStatus(client(), false));
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

}  // namespace autofill
