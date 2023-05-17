// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

#include "components/password_manager/core/browser/password_manager_eviction_util.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::PasswordManagerSetting;

namespace {

constexpr char kUnenrollmentHistogram[] =
    "PasswordManager.UnenrolledFromUPMDueToErrors";
constexpr char kUnenrollmentReasonHistogram[] =
    "PasswordManager.UPMUnenrollmentReason";

constexpr int kInternalError = 8;
constexpr int kDeveloperError = 10;
constexpr int kBackendGeneric = 11009;
constexpr int kInvalidData = 11011;
constexpr int kUnexpectedError = 11013;

void EnableUPMFeatureWithTestParams(
    base::test::ScopedFeatureList* feature_list) {
  feature_list->InitAndEnableFeatureWithParameters(
      password_manager::features::kUnifiedPasswordManagerAndroid,
      {
          // INTERNAL_ERROR=8, BACKEND_GENERIC=11009
          {password_manager::features::kIgnoredGmsApiErrors.name, "8,11009"},
          // DEVELOPER_ERROR=10, INVALID_DATA=11011
          {password_manager::features::kRetriableGmsApiErrors.name, "10,11011"},
      });
}

}  // namespace

class PasswordManagerEvictionUtilTest : public testing::Test {
 protected:
  PasswordManagerEvictionUtilTest();
  ~PasswordManagerEvictionUtilTest() override;

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  TestingPrefServiceSimple* pref_service() { return &test_pref_service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple test_pref_service_;
};

PasswordManagerEvictionUtilTest::PasswordManagerEvictionUtilTest() {
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      false);
  test_pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::
          kUnenrolledFromGoogleMobileServicesAfterApiErrorCode,
      0);
  test_pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kTimesReenrolledToGoogleMobileServices, 0);
  test_pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kTimesAttemptedToReenrollToGoogleMobileServices,
      0);

  test_pref_service_.registry()->RegisterIntegerPref(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      0);
  test_pref_service_.registry()->RegisterDoublePref(
      password_manager::prefs::kTimeOfLastMigrationAttempt, 0.0);
  test_pref_service_.registry()->RegisterBooleanPref(
      password_manager::prefs::kSettingsMigratedToUPM, false);
}

PasswordManagerEvictionUtilTest::~PasswordManagerEvictionUtilTest() = default;

TEST_F(PasswordManagerEvictionUtilTest, EvictsUser) {
  EnableUPMFeatureWithTestParams(feature_list());

  pref_service()->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      1);
  pref_service()->SetDouble(
      password_manager::prefs::kTimeOfLastMigrationAttempt, 20.22);
  pref_service()->SetBoolean(password_manager::prefs::kSettingsMigratedToUPM,
                             true);

  base::HistogramTester histogram_tester;

  password_manager_upm_eviction::EvictCurrentUser(kInternalError,
                                                  pref_service());

  EXPECT_TRUE(pref_service()->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(pref_service()->GetInteger(
                password_manager::prefs::
                    kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            kInternalError);

  EXPECT_EQ(pref_service()->GetInteger(
                password_manager::prefs::
                    kCurrentMigrationVersionToGoogleMobileServices),
            0);
  EXPECT_EQ(pref_service()->GetDouble(
                password_manager::prefs::kTimeOfLastMigrationAttempt),
            0.0);
  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kSettingsMigratedToUPM));

  histogram_tester.ExpectUniqueSample(kUnenrollmentHistogram, true, 1);
  histogram_tester.ExpectUniqueSample(kUnenrollmentReasonHistogram,
                                      kInternalError, 1);
}

TEST_F(PasswordManagerEvictionUtilTest, IndicatesEvictedUser) {
  EnableUPMFeatureWithTestParams(feature_list());

  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);

  EXPECT_TRUE(
      password_manager_upm_eviction::IsCurrentUserEvicted(pref_service()));
}

TEST_F(PasswordManagerEvictionUtilTest, IndicatesNotEvictedUser) {
  feature_list()->InitAndEnableFeature(
      password_manager::features::kUnifiedPasswordManagerAndroid);

  EXPECT_FALSE(
      password_manager_upm_eviction::IsCurrentUserEvicted(pref_service()));
}

TEST_F(PasswordManagerEvictionUtilTest, ReenrollsUser) {
  EnableUPMFeatureWithTestParams(feature_list());

  pref_service()->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  pref_service()->SetInteger(
      password_manager::prefs::
          kUnenrolledFromGoogleMobileServicesAfterApiErrorCode,
      kInternalError);
  pref_service()->SetInteger(
      password_manager::prefs::kTimesReenrolledToGoogleMobileServices, 1);
  pref_service()->SetInteger(
      password_manager::prefs::kTimesAttemptedToReenrollToGoogleMobileServices,
      1);

  password_manager_upm_eviction::ReenrollCurrentUser(pref_service());

  EXPECT_FALSE(pref_service()->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));
  EXPECT_EQ(pref_service()->GetInteger(
                password_manager::prefs::
                    kUnenrolledFromGoogleMobileServicesAfterApiErrorCode),
            0);
  EXPECT_EQ(
      pref_service()->GetInteger(
          password_manager::prefs::kTimesReenrolledToGoogleMobileServices),
      0);
  EXPECT_EQ(pref_service()->GetInteger(
                password_manager::prefs::
                    kTimesAttemptedToReenrollToGoogleMobileServices),
            0);
}

TEST_F(PasswordManagerEvictionUtilTest, ShouldNotIgnoreByDefault) {
  feature_list()->InitAndEnableFeature(
      password_manager::features::kUnifiedPasswordManagerAndroid);

  EXPECT_FALSE(
      password_manager_upm_eviction::ShouldIgnoreOnApiError(kInternalError));
}

TEST_F(PasswordManagerEvictionUtilTest, ShouldNotRetryByDefault) {
  feature_list()->InitAndEnableFeature(
      password_manager::features::kUnifiedPasswordManagerAndroid);

  EXPECT_FALSE(
      password_manager_upm_eviction::ShouldRetryOnApiError(kInternalError));
}

TEST_F(PasswordManagerEvictionUtilTest, ShouldIgnoreOnlyListedError) {
  EnableUPMFeatureWithTestParams(feature_list());

  EXPECT_TRUE(
      password_manager_upm_eviction::ShouldIgnoreOnApiError(kInternalError));
  EXPECT_TRUE(
      password_manager_upm_eviction::ShouldIgnoreOnApiError(kBackendGeneric));
  EXPECT_FALSE(
      password_manager_upm_eviction::ShouldIgnoreOnApiError(kDeveloperError));
  EXPECT_FALSE(
      password_manager_upm_eviction::ShouldIgnoreOnApiError(kUnexpectedError));
}

TEST_F(PasswordManagerEvictionUtilTest, ShouldRetryOnlyListedError) {
  EnableUPMFeatureWithTestParams(feature_list());

  EXPECT_TRUE(
      password_manager_upm_eviction::ShouldRetryOnApiError(kDeveloperError));
  EXPECT_TRUE(
      password_manager_upm_eviction::ShouldRetryOnApiError(kInvalidData));
  EXPECT_FALSE(
      password_manager_upm_eviction::ShouldRetryOnApiError(kInternalError));
  EXPECT_FALSE(
      password_manager_upm_eviction::ShouldRetryOnApiError(kUnexpectedError));
}
