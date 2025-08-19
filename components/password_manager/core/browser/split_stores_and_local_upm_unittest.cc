// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/split_stores_and_local_upm.h"

#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr std::string kSplitStoresUpmMinVersionForNonAuto = "240212000";
constexpr std::string kSplitStoresUpmMinVersionForAuto = "241512000";

class SplitStoresAndLocalUpmTest : public ::testing::Test {
 public:
  SplitStoresAndLocalUpmTest() {
    RegisterLegacySplitStoresPref(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOff) {
  EXPECT_FALSE(GetLegacySplitStoresPref(pref_service()));
}

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOn) {
  SetLegacySplitStoresPrefForTest(pref_service(), true);

  EXPECT_TRUE(GetLegacySplitStoresPref(pref_service()));
}

struct IsGmsCoreUpdateRequiredTestCase {
  std::string test_case_desc;
  std::string gms_version;
  bool expected_is_update_required_automotive;
  bool expected_is_update_required;
};

class SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired
    : public SplitStoresAndLocalUpmTest,
      public ::testing::WithParamInterface<IsGmsCoreUpdateRequiredTestCase> {
 public:
  SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired() = default;
};

TEST_P(SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired,
       IsGmsCoreUpdateRequired) {
  IsGmsCoreUpdateRequiredTestCase p = GetParam();

  bool expected_is_update_required =
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
      false
#else
      base::android::device_info::is_automotive()
          ? p.expected_is_update_required_automotive
          : p.expected_is_update_required
#endif
      ;
  base::android::device_info::set_gms_version_code_for_test(p.gms_version);
  EXPECT_EQ(expected_is_update_required, IsGmsCoreUpdateRequired());
}

INSTANTIATE_TEST_SUITE_P(
    SplitStoresAndLocalUpmTest,
    SplitStoresAndLocalUpmTestIsGmsCoreUpdateRequired,
    testing::Values(
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "TrueForGmsDoesNotSupportSeparatedStores",
            .gms_version = "1",
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = true},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "FalseForGmsSupportSeparatedStoresForNonAuto",
            .gms_version = kSplitStoresUpmMinVersionForNonAuto,
            .expected_is_update_required_automotive = true,
            .expected_is_update_required = false},
        IsGmsCoreUpdateRequiredTestCase{
            .test_case_desc = "FalseForGmsSupportSeparatedStoresForAuto",
            .gms_version = kSplitStoresUpmMinVersionForAuto,
            .expected_is_update_required_automotive = false,
            .expected_is_update_required = false}),
    [](const ::testing::TestParamInfo<IsGmsCoreUpdateRequiredTestCase>& info) {
      return info.param.test_case_desc;
    });

}  // namespace
}  // namespace password_manager
