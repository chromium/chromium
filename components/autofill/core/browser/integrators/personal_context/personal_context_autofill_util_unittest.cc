// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/personal_context/personal_context_autofill_util.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using personal_context::PersonalContextEnablementService;
using personal_context::PersonalContextEnablementState;
using ::testing::NiceMock;
using ::testing::Return;

class MockPersonalContextEnablementService
    : public PersonalContextEnablementService {
 public:
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(PersonalContextEnablementState,
              GetEnablementState,
              (),
              (override));
};

class PersonalContextAutofillUtilTest : public testing::Test {
 public:
  PersonalContextAutofillUtilTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kAutofillAiWithDataSchema, {}},
            {features::kAutofillAiServerModel, {}},
            {features::kAutofillAiAvailableByDefault, {}},
            {features::kAutofillAmbientAutofill,
             {{"ambient_autofill_eligible_tiers", "1"}}},
        },
        /*disabled_features=*/{});
    client_.GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, 1);
    client_.SetUpPrefsAndIdentityForAutofillAi();
    client_.set_entity_data_manager(std::make_unique<EntityDataManager>(
        client_.GetPrefs(), client_.GetIdentityManager(),
        /*sync_service=*/nullptr, webdata_helper_.autofill_webdata_service(),
        /*history_service=*/nullptr,
        /*personal_context_access_manager=*/nullptr,
        /*strike_database=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US")));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  TestAutofillClient client_;
};

}  // namespace

TEST_F(PersonalContextAutofillUtilTest,
       ShouldShowPersonalContextAutofillSetting) {
  using enum PersonalContextEnablementState;
  NiceMock<MockPersonalContextEnablementService> service;

  auto check_state = [&](PersonalContextEnablementState state) {
    ON_CALL(service, GetEnablementState()).WillByDefault(Return(state));
    return ShouldShowPersonalContextAutofillSetting(client_, &service);
  };

  EXPECT_FALSE(check_state(kDisabledNotEligible));
  EXPECT_FALSE(check_state(kDisabledNeedsOptIn));
  EXPECT_TRUE(check_state(kEnabled));

  EXPECT_FALSE(ShouldShowPersonalContextAutofillSetting(
      client_, /*enablement_service=*/nullptr));
}

TEST_F(PersonalContextAutofillUtilTest,
       ShouldShowPersonalContextAutofillSetting_BothDisabled) {
  NiceMock<MockPersonalContextEnablementService> service;
  ON_CALL(service, GetEnablementState())
      .WillByDefault(Return(PersonalContextEnablementState::kEnabled));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillAmbientAutofill,
                             features::kAutofillAtMemory});
  EXPECT_FALSE(ShouldShowPersonalContextAutofillSetting(client_, &service));
}

TEST_F(PersonalContextAutofillUtilTest,
       ShouldShowPersonalContextAutofillSetting_AmbientAutofillEnabled) {
  NiceMock<MockPersonalContextEnablementService> service;
  ON_CALL(service, GetEnablementState())
      .WillByDefault(Return(PersonalContextEnablementState::kEnabled));

  EXPECT_TRUE(ShouldShowPersonalContextAutofillSetting(client_, &service));
}

}  // namespace autofill
