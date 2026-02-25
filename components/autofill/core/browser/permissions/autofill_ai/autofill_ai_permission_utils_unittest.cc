// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_testing_pref_service.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Values;
using enum EntityTypeName;

constexpr auto kAutofillPredictionSettingsDisable =
    std::to_underlying(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable);

std::string GetTestSuffix(
    ::testing::TestParamInfo<AutofillAiAction> param_info) {
  switch (param_info.param) {
    case AutofillAiAction::kAddLocalEntityInstanceInSettings:
      return "kAddLocalEntityInstanceInSettings";
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
    case AutofillAiAction::kEnableOrDisable:
      return "kEnableOrDisable";
    case AutofillAiAction::kServerClassificationModel:
      return "kServerClassificationModel";
    case AutofillAiAction::kUseCachedServerClassificationModelResults:
      return "kUseCachedServerClassificationModelResults";
    case AutofillAiAction::kImportToWallet:
      return "kImportToWallet";
    case AutofillAiAction::kWalletDataSharingPromotion:
      return "kWalletDataSharingPromotion";
  }
  NOTREACHED();
}

DenseSet<EntityType> GetPrivatePasses() {
  DenseSet<EntityType> private_passes;
  for (const EntityType type : DenseSet<EntityType>::all()) {
    if (IsMaskedStorageSupported(type,
                                 EntityInstance::RecordType::kServerWallet)) {
      private_passes.insert(type);
    }
  }
  return private_passes;
}

class MockSyncService : public syncer::TestSyncService {
 public:
  MOCK_METHOD(syncer::DataTypeSet, GetActiveDataTypes, (), (const override));
};

// A test fixture that sets up default state so that all AutofillAI-related
// actions are permitted.
class AutofillAiPermissionUtilsTest : public ::testing::Test {
 public:
  AutofillAiPermissionUtilsTest() {
    // Features.
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kAutofillAiWithDataSchema, {}},
         {features::kAutofillAiWalletVehicleRegistration, {}},
         {features::kAutofillAiWalletFlightReservation, {}},
         {features::kAutofillAiServerModel,
          {{"autofill_ai_model_use_cache_results", "true"}}}},
        {});

    // Pref and identity state.
    client().set_entity_data_manager(std::make_unique<EntityDataManager>(
        client().GetPrefs(), client().GetIdentityManager(),
        client().GetSyncService(), webdata_helper_.autofill_webdata_service(),
        /*history_service=*/nullptr,
        /*strike_database=*/nullptr,
        /*accessibility_annotator_data_adapter=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US")));
    client().SetUpPrefsAndIdentityForAutofillAi();
    client().set_sync_service(&sync_service_);
  }

  void AddEntity() {
    edm().AddOrUpdateEntityInstance(test::GetPassportEntityInstance());
    webdata_helper_.WaitUntilIdle();
  }

  TestAutofillClient& client() { return client_; }
  EntityDataManager& edm() { return *client().GetEntityDataManager(); }
  MockSyncService& sync_service() { return sync_service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  NiceMock<MockSyncService> sync_service_;
  TestAutofillClient client_;
};

class AutofillAiMayPerformActionTest
    : public AutofillAiPermissionUtilsTest,
      public ::testing::WithParamInterface<AutofillAiAction> {
 public:
  AutofillAiMayPerformActionTest() {
    client().GetSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPayments, true);
    ON_CALL(sync_service(), GetActiveDataTypes())
        .WillByDefault(Return(syncer::DataTypeSet{syncer::AUTOFILL_VALUABLE}));
  }
};

// Verifies that the test fixture sets up the client so that everything but
// opt-in IPH and the Wallet data sharing promotion is permitted.
TEST_P(AutofillAiMayPerformActionTest, ActionsWhenEnabled) {
  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that `kAutofillAiWithDataSchema` is a requirement for all actions.
TEST_P(AutofillAiMayPerformActionTest, ReturnsFalseWhenMainFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiWithDataSchema);

  EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam()));
}

// Tests that when `kAutofillAiAvailableByDefault` and the user is opted out,
// everything but IPH, wallet data sharing promotion, and model related actions
// is permitted.
TEST_P(AutofillAiMayPerformActionTest,
       ReturnsTrueWhenAvailableByDefault_ExceptForModelRelatedActionsAndIph) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiAvailableByDefault};
  SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut);

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn, AutofillAiAction::kLogToMqls,
                AutofillAiAction::kServerClassificationModel,
                AutofillAiAction::kWalletDataSharingPromotion});

  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that when `kAutofillAiAvailableByDefault`, the user is opted out,
// and the enterprise policy is off, everything but IPH, opt-in and model
// related actions is permitted.
TEST_P(
    AutofillAiMayPerformActionTest,
    AvailableByDefaultAndEnterprisePolicyIsOff_TrueExceptForModelRelatedActionsIphAndOptIn) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiAvailableByDefault};
  SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut);
  client().GetPrefs()->SetInteger(
      optimization_guide::prefs::
          kAutofillPredictionImprovementsEnterprisePolicyAllowed,
      kAutofillPredictionSettingsDisable);

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kOptIn, AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kLogToMqls,
                AutofillAiAction::kServerClassificationModel,
                AutofillAiAction::kWalletDataSharingPromotion});

  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that when `kAutofillAiAvailableByDefault` and the user is opted in,
// everything but IPH and the Wallet data sharing promotion is permitted.
TEST_P(AutofillAiMayPerformActionTest,
       ReturnsTrueWhenAvailableByDefault_ExceptForIph) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiAvailableByDefault};

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that the server model cannot be run and its cache cannot be used if
// `kAutofillAiServerModel` is disabled.
TEST_P(AutofillAiMayPerformActionTest, ModelFeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiServerModel);

  // The opt-in IPH cannot be run either since we simulate a state in which the
  // user has opted into the feature.
  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kServerClassificationModel,
                AutofillAiAction::kUseCachedServerClassificationModelResults,
                AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
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
  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kUseCachedServerClassificationModelResults,
                AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that the opt-in IPH cannot be shown if its feature is off.
TEST_P(AutofillAiMayPerformActionTest, OptInIphFeatureOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      feature_engagement::kIPHAutofillAiOptInFeature);

  SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut);
  const bool is_allowed =
      GetParam() == AutofillAiAction::kOptIn ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      is_allowed);
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
    EXPECT_TRUE(MayPerformAutofillAiAction(client(), GetParam(),
                                           EntityType(kPassport)));
  } else {
    EXPECT_FALSE(MayPerformAutofillAiAction(client(), GetParam(),
                                            EntityType(kPassport)));
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
      GetParam() == AutofillAiAction::kEditAndDeleteEntityInstanceInSettings ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam()), is_allowed);
}

// Verifies that IPH, opt-in and list entities are permitted if the user has not
// opted into AutofillAI.
TEST_P(AutofillAiMayPerformActionTest, ActionsWhenNotOptedIntoAutofillAi) {
  SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut);
  const bool is_allowed =
      GetParam() == AutofillAiAction::kOptIn ||
      GetParam() == AutofillAiAction::kIphForOptIn ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      is_allowed);
}

// Tests that listing, editing and removing entities is permitted if user is no
// longer opted into AutofillAI, but there is data saved.
TEST_P(AutofillAiMayPerformActionTest,
       ActionsWhenAutofillNotOptedIntoAutofillAiButDataSaved) {
  AddEntity();
  SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut);
  const bool is_allowed =
      GetParam() == AutofillAiAction::kOptIn ||
      GetParam() == AutofillAiAction::kIphForOptIn ||
      GetParam() == AutofillAiAction::kEditAndDeleteEntityInstanceInSettings ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      is_allowed);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
// Tests that every action other than listing and editing data requires the user
// to be signed in.
TEST_P(AutofillAiMayPerformActionTest, SignedOut) {
  AddEntity();
  client().identity_test_environment().ClearPrimaryAccount();
  std::string debug_message;
  const bool is_allowed =
      GetParam() == AutofillAiAction::kEditAndDeleteEntityInstanceInSettings ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;

  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam(), std::nullopt,
                                       &debug_message),
            is_allowed);

  if (!is_allowed) {
    EXPECT_EQ(debug_message, "User not signed into Chrome.");
  }
}

TEST_P(AutofillAiMayPerformActionTest, SignInPending) {
  AddEntity();
  CoreAccountInfo account =
      client().identity_test_environment().MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);

  client()
      .identity_test_environment()
      .UpdatePersistentErrorOfRefreshTokenForAccount(
          account.account_id,
          GoogleServiceAuthError(
              GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  std::string debug_message;
  const bool is_allowed =
      GetParam() == AutofillAiAction::kEditAndDeleteEntityInstanceInSettings ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;

  EXPECT_EQ(MayPerformAutofillAiAction(client(), GetParam(), std::nullopt,
                                       &debug_message),
            is_allowed);

  if (!is_allowed) {
    EXPECT_EQ(debug_message, "User's sign-in is in a persistent error state.");
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Tests that the check whether a client can use model execution features is
// ignored.
TEST_P(AutofillAiMayPerformActionTest, CapabilityCheckIgnored) {
  AddEntity();
  client().SetCanUseModelExecutionFeatures(false);
  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that the check whether a client can use model execution features is
// ignored before opt-in or IPH.
TEST_P(AutofillAiMayPerformActionTest, CapabilityCheckIgnoredOptedOut) {
  SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut);
  client().SetCanUseModelExecutionFeatures(false);

  const bool is_allowed =
      GetParam() == AutofillAiAction::kOptIn ||
      GetParam() == AutofillAiAction::kIphForOptIn ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      is_allowed);
}

// Tests that only filling and cache use are allowed off-the-record.
TEST_P(AutofillAiMayPerformActionTest, OffTheRecord) {
  client().set_is_off_the_record(true);
  const bool is_allowed =
      GetParam() == AutofillAiAction::kFilling ||
      GetParam() ==
          AutofillAiAction::kUseCachedServerClassificationModelResults;
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      is_allowed);
}

TEST_P(AutofillAiMayPerformActionTest, CountryCode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAiIgnoreGeoIp);
  client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));
  EXPECT_FALSE(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)));
}

// Tests that if `kAutofillAiIgnoreGeoIp` and an allowlist is set, the feature
// is enabled in countries on the allowlist.
TEST_P(AutofillAiMayPerformActionTest, CountryCodeWithAllowlist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiIgnoreGeoIp,
      {{"autofill_ai_geo_ip_allowlist", "BR,MX"}});

  client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));
  EXPECT_FALSE(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)));

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  client().SetVariationConfigCountryCode(GeoIpCountryCode("BR"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));

  client().SetVariationConfigCountryCode(GeoIpCountryCode("MX"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that if `kAutofillAiIgnoreGeoIp` and a blocklist is set, the feature
// is disabled only in the countries on the allowlist.
TEST_P(AutofillAiMayPerformActionTest, CountryCodeWithBlocklist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiIgnoreGeoIp,
      {{"autofill_ai_geo_ip_blocklist", "FR,MX,CA"}});

  client().SetVariationConfigCountryCode(GeoIpCountryCode("FR"));
  EXPECT_FALSE(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)));

  client().SetVariationConfigCountryCode(GeoIpCountryCode("MX"));
  EXPECT_FALSE(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)));

  client().SetVariationConfigCountryCode(GeoIpCountryCode("CA"));
  EXPECT_FALSE(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)));

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));

  client().SetVariationConfigCountryCode(GeoIpCountryCode("US"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that users can edit stored data even if their GeoIP is on the
// blocklist.
TEST_P(AutofillAiMayPerformActionTest, CountryCodeWithBlocklistAndSavedData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiIgnoreGeoIp,
      {{"autofill_ai_geo_ip_blocklist", "IN"}});

  AddEntity();
  client().SetVariationConfigCountryCode(GeoIpCountryCode("IN"));
  const bool is_allowed =
      GetParam() == AutofillAiAction::kEditAndDeleteEntityInstanceInSettings ||
      GetParam() == AutofillAiAction::kListEntityInstancesInSettings;
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      is_allowed);
}

// Tests that every GeoIP is permitted if `kAutofillAiIgnoreGeoIp` is enabled
// and no blocklist or allowlist is set.
TEST_P(AutofillAiMayPerformActionTest, IgnoreGeoIp) {
  base::test::ScopedFeatureList feature_list{features::kAutofillAiIgnoreGeoIp};

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});

  client().SetVariationConfigCountryCode(GeoIpCountryCode("DE"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));

  client().SetVariationConfigCountryCode(GeoIpCountryCode("IT"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));

  client().SetVariationConfigCountryCode(GeoIpCountryCode("US"));
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that the blocklist has priority over the allowlist.
TEST_P(AutofillAiMayPerformActionTest, IgnoreGeoIpBlocklistAndAllowlist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillAiIgnoreGeoIp,
      {{"autofill_ai_geo_ip_blocklist", "IN"},
       {"autofill_ai_geo_ip_allowlist", "IN"}});

  client().SetVariationConfigCountryCode(GeoIpCountryCode("IN"));
  EXPECT_FALSE(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)));
}

TEST_P(AutofillAiMayPerformActionTest, AppLocale) {
  client().set_app_locale("de-DE");

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that Wallet-related actions are not available on non-supported
// countries.
TEST_P(AutofillAiMayPerformActionTest, kWalletSupportedCountries) {
  base::test::ScopedFeatureList feature_list{features::kAutofillAiIgnoreGeoIp};
  // Wallet is not supported in India.
  client().SetVariationConfigCountryCode(GeoIpCountryCode("IN"));
  constexpr auto kForbiddenActions = DenseSet(
      {AutofillAiAction::kImportToWallet, AutofillAiAction::kIphForOptIn,
       AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that if the Wallet data sharing setting is turned off, the Wallet
// data sharing promotion can be shown.
TEST_P(AutofillAiMayPerformActionTest, WalletDataSharingOff) {
  client().SetWalletStorageEnabled(false);

  constexpr auto kForbiddenActions = DenseSet({AutofillAiAction::kIphForOptIn});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

// Tests that if the Wallet data sharing setting is turned off and the country
// is not supported by Wallet, the data sharing promotion is not allowed.
TEST_P(AutofillAiMayPerformActionTest, WalletDataSharingOffUnsupportedCountry) {
  client().SetWalletStorageEnabled(false);
  client().SetVariationConfigCountryCode(GeoIpCountryCode("IN"));

  constexpr auto kForbiddenActions =
      DenseSet({AutofillAiAction::kIphForOptIn,
                AutofillAiAction::kWalletDataSharingPromotion});
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), GetParam(), EntityType(kPassport)),
      !kForbiddenActions.contains(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillAiMayPerformActionTest,
    Values(AutofillAiAction::kAddLocalEntityInstanceInSettings,
           AutofillAiAction::kCrowdsourcingVote,
           AutofillAiAction::kEditAndDeleteEntityInstanceInSettings,
           AutofillAiAction::kFilling,
           AutofillAiAction::kImport,
           AutofillAiAction::kIphForOptIn,
           AutofillAiAction::kListEntityInstancesInSettings,
           AutofillAiAction::kLogToMqls,
           AutofillAiAction::kOptIn,
           AutofillAiAction::kServerClassificationModel,
           AutofillAiAction::kUseCachedServerClassificationModelResults,
           AutofillAiAction::kWalletDataSharingPromotion),
    GetTestSuffix);

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
// Tests that opt-in status is tied to a GAIA id.
TEST_F(AutofillAiPermissionUtilsTest, OptInStatus) {
  const std::string initial_email =
      client()
          .GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  const std::string other_email = "something_else@gmail.com";
  ASSERT_NE(initial_email, other_email);

  // The initially signed in account is opted in.
  EXPECT_TRUE(GetAutofillAiOptInStatus(client()));

  client().identity_test_environment().ClearPrimaryAccount();
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));

  // After signing in with a different account, the opt-in is gone.
  client().identity_test_environment().MakePrimaryAccountAvailable(
      other_email, signin::ConsentLevel::kSignin);
  client().SetCanUseModelExecutionFeatures(true);
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));
  EXPECT_TRUE(
      SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedIn));
  EXPECT_TRUE(GetAutofillAiOptInStatus(client()));

  // Switch back to the old account and the old opt-in is back.
  client().identity_test_environment().ClearPrimaryAccount();
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));
  client().identity_test_environment().MakePrimaryAccountAvailable(
      initial_email, signin::ConsentLevel::kSignin);
  client().SetCanUseModelExecutionFeatures(true);
  EXPECT_TRUE(GetAutofillAiOptInStatus(client()));

  // Setting it to `false` works as well.
  EXPECT_TRUE(
      SetAutofillAiOptInStatus(client(), AutofillAiOptInStatus::kOptedOut));
  EXPECT_FALSE(GetAutofillAiOptInStatus(client()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AutofillAiPermissionUtilsTest,
       UsersCannotOptInIfAutofillForAddressesIsDisabled) {
  EXPECT_TRUE(MayPerformAutofillAiAction(client(), AutofillAiAction::kOptIn,
                                         std::nullopt));
  client().GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  EXPECT_FALSE(MayPerformAutofillAiAction(client(), AutofillAiAction::kOptIn,
                                          std::nullopt));
}

// TODO(crbug.com/482301350): Remove this test
TEST_F(
    AutofillAiPermissionUtilsTest,
    UsersCanOptInIfAutofillForAddressesIsDisabledWhenOtherDatatypesPrefEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAddOtherDatatypesPref};
  ASSERT_TRUE(MayPerformAutofillAiAction(client(), AutofillAiAction::kOptIn,
                                         std::nullopt));
  client().GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled, false);
  EXPECT_TRUE(MayPerformAutofillAiAction(client(), AutofillAiAction::kOptIn,
                                         std::nullopt));
}

// Tests that changes to the opt-in status are recorded in metrics.
TEST_F(AutofillAiPermissionUtilsTest, OptInStatusMetrics) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(GetAutofillAiOptInStatus(client()));

  using enum AutofillAiOptInStatus;
  EXPECT_TRUE(SetAutofillAiOptInStatus(client(), kOptedOut));
  histogram_tester.ExpectUniqueSample("Autofill.Ai.OptIn.Change", kOptedOut, 1);

  EXPECT_TRUE(SetAutofillAiOptInStatus(client(), kOptedIn));
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.Ai.OptIn.Change"),
              BucketsAre(Bucket(kOptedIn, 1), Bucket(kOptedOut, 1)));

  EXPECT_TRUE(SetAutofillAiOptInStatus(client(), kOptedOut));
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.Ai.OptIn.Change"),
              BucketsAre(Bucket(kOptedIn, 1), Bucket(kOptedOut, 2)));
}

// Tests that the prefs affect MayPerformAutofillAiAction() for kFilling and
// kImport. The `bool` parameters are the pref values.
class AutofillAiMayPerformFillOrImportTest
    : public AutofillAiPermissionUtilsTest,
      public testing::WithParamInterface<
          std::tuple<AutofillAiAction, bool, bool>> {
 public:
  AutofillAiAction action() const { return std::get<0>(GetParam()); }
  bool identity_entities_enabled() const { return std::get<1>(GetParam()); }
  bool travel_entities_enabled() const { return std::get<2>(GetParam()); }

  void SetUp() override {
    AutofillAiPermissionUtilsTest::SetUp();
    client().GetPrefs()->SetBoolean(prefs::kAutofillAiIdentityEntitiesEnabled,
                                    identity_entities_enabled());
    client().GetPrefs()->SetBoolean(prefs::kAutofillAiTravelEntitiesEnabled,
                                    travel_entities_enabled());
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillAiMayPerformFillOrImportTest,
    testing::Combine(testing::Values(AutofillAiAction::kFilling,
                                     AutofillAiAction::kImport),
                     testing::Bool(),
                     testing::Bool()));

// Tests that MayPerformAutofillAiAction() depends on the given EntityType and
// the pref state.
TEST_P(AutofillAiMayPerformFillOrImportTest,
       PrefsAndEntityAffectMayPerformAutofillAiAction) {
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), action(), EntityType(kPassport)),
      identity_entities_enabled());
  EXPECT_EQ(
      MayPerformAutofillAiAction(client(), action(), EntityType(kVehicle)),
      travel_entities_enabled());
}

class AutofillAiMayPerformImportToWalletTest
    : public AutofillAiPermissionUtilsTest {
 public:
  AutofillAiMayPerformImportToWalletTest() {
    client().GetSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPayments, true);
    ON_CALL(sync_service(), GetActiveDataTypes())
        .WillByDefault(Return(syncer::DataTypeSet{syncer::AUTOFILL_VALUABLE}));
  }
};

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_TrueForVehicleWhenSyncingWallet) {
  client().SetWalletStorageEnabled(true);
  EXPECT_TRUE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kVehicle)));
}

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_FalseForVehicleWhenWalletPrefDisabled) {
  client().SetWalletStorageEnabled(false);
  EXPECT_FALSE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kVehicle)));
}

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_FalseForPrivatePassesIfFeatureIsOff) {
  client().SetWalletStorageEnabled(true);
  for (const EntityType entity_type : GetPrivatePasses()) {
    EXPECT_FALSE(MayPerformAutofillAiAction(
        client(), AutofillAiAction::kImportToWallet, entity_type))
        << entity_type;
  }
}

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_TrueForPrivatePassIfFeatureIsOn) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWalletPrivatePasses};
  client().SetWalletStorageEnabled(true);
  for (const EntityType entity_type : GetPrivatePasses()) {
    EXPECT_TRUE(MayPerformAutofillAiAction(
        client(), AutofillAiAction::kImportToWallet, entity_type))
        << entity_type;
  }
}

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_FalseForVehicleWhenFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAiWalletVehicleRegistration);
  client().SetWalletStorageEnabled(true);
  EXPECT_FALSE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kVehicle)));
}

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_FalseWhenNotSyncingWallet) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWalletPrivatePasses};
  client().GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, false);
  client().SetWalletStorageEnabled(true);
  EXPECT_FALSE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kVehicle)));
  EXPECT_FALSE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kPassport)));
}

TEST_F(AutofillAiMayPerformImportToWalletTest,
       ImportToWallet_FalseWhenAutofillValuableIsNotActive) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWalletPrivatePasses};
  ON_CALL(sync_service(), GetActiveDataTypes())
      .WillByDefault(Return(syncer::DataTypeSet()));
  EXPECT_FALSE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kVehicle)));
  EXPECT_FALSE(MayPerformAutofillAiAction(
      client(), AutofillAiAction::kImportToWallet, EntityType(kPassport)));
}

}  // namespace

}  // namespace autofill
