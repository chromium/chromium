// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ablation_study.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "url/gurl.h"

using autofill::features::kAutofillAblationStudyAblationWeightPerMilleParam;
using autofill::features::kAutofillAblationStudyEnabledForAddressesParam;
using autofill::features::kAutofillAblationStudyEnabledForPaymentsParam;
using autofill::features::kAutofillEnableAblationStudy;

namespace autofill {

namespace {

// Calls GetAblationGroup |n| times on different security origins and returns a
// histogram of the number of times certain AblationGroups were returned.
std::map<AblationGroup, int> RunNIterations(
    const AutofillAblationStudy& study,
    int n,
    FormTypeForAblationStudy form_type) {
  std::map<AblationGroup, int> result;
  for (int i = 0; i < n; ++i) {
    GURL url(base::StringPrintf("https://www.example%d.com", i));
    AblationGroup ablation_group = study.GetAblationGroup(url, form_type);
    result[ablation_group]++;
  }
  return result;
}

}  // namespace

class AutofillAblationStudyTest : public testing::Test {
 public:
  AutofillAblationStudyTest() = default;
  ~AutofillAblationStudyTest() override = default;

  base::Time GetDefaultTime() {
    base::Time time;
    CHECK(base::Time::FromString("Thu, 6 May 2021, 13:00:00 GMT", &time));
    return time;
  }

  GURL GetDefaultUrl() { return GURL("https://www.example.com/home"); }
};

// Tests in UTC timezone.
class AutofillAblationStudyTestInUTC : public AutofillAblationStudyTest {
 public:
  AutofillAblationStudyTestInUTC() = default;
  ~AutofillAblationStudyTestInUTC() override = default;

  void SetUp() override {
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("GMT"));

    std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
    ASSERT_EQ(0, zone->getRawOffset());
  }

  void TearDown() override { icu::TimeZone::adoptDefault(nullptr); }
};

TEST_F(AutofillAblationStudyTestInUTC, DaysSinceLocalWindowsEpoch) {
  // Ensure that time zone alignment works out with the day boundaries of the
  // loal timezone.
  struct {
    const char* time_string;
    int expected_days_since_windows_epoch;
  } kTests[] = {
      {"Mon, 1 Jan 1601, 00:00:00 GMT", 0},
      {"Mon, 1 Jan 1601, 01:00:00 GMT", 0},
      {"Mon, 1 Jan 1601, 23:00:00 GMT", 0},
      {"Tue, 2 Jan 1601, 00:00:00 GMT", 1},
      {"Tue, 2 Jan 1601, 01:00:00 GMT", 1},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.time_string);
    base::Time time;
    ASSERT_TRUE(base::Time::FromString(test.time_string, &time));
    EXPECT_EQ(test.expected_days_since_windows_epoch,
              DaysSinceLocalWindowsEpoch(time));
  }
}

// Tests in EST timezone.
class AutofillAblationStudyTestInEST : public AutofillAblationStudyTest {
 public:
  AutofillAblationStudyTestInEST() = default;
  ~AutofillAblationStudyTestInEST() override = default;

  void SetUp() override {
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("EST"));

    std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
    ASSERT_EQ(-18000000, zone->getRawOffset());
  }

  void TearDown() override { icu::TimeZone::adoptDefault(nullptr); }
};

TEST_F(AutofillAblationStudyTestInEST, DaysSinceLocalWindowsEpoch) {
  // Ensure that time zone alignment works out with the day boundaries of the
  // loal timezone.
  struct {
    const char* time_string;
    int expected_days_since_windows_epoch;
  } kTests[] = {
      {"Mon, 1 Jan 1601, 00:00:00 EST", 0},
      {"Mon, 1 Jan 1601, 01:00:00 EST", 0},
      {"Mon, 1 Jan 1601, 23:00:00 EST", 0},
      {"Tue, 2 Jan 1601, 00:00:00 EST", 1},
      {"Tue, 2 Jan 1601, 01:00:00 EST", 1},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.time_string);
    base::Time time;
    ASSERT_TRUE(base::Time::FromString(test.time_string, &time));
    EXPECT_EQ(test.expected_days_since_windows_epoch,
              DaysSinceLocalWindowsEpoch(time));
  }
}

TEST_F(AutofillAblationStudyTestInUTC, GetAblationHash_IdenticalInput) {
  EXPECT_EQ(GetAblationHash("seed1", GetDefaultUrl(), GetDefaultTime()),
            GetAblationHash("seed1", GetDefaultUrl(), GetDefaultTime()));
}

TEST_F(AutofillAblationStudyTestInUTC, GetAblationHash_DependsOnSeed) {
  // Different seeds produce different hashes.
  EXPECT_NE(GetAblationHash("seed1", GetDefaultUrl(), GetDefaultTime()),
            GetAblationHash("seed2", GetDefaultUrl(), GetDefaultTime()));
}

TEST_F(AutofillAblationStudyTestInUTC, GetAblationHash_DependsOnOrigin) {
  // Different origins produce different hashes
  base::Time t = GetDefaultTime();

  // Different scheme
  EXPECT_NE(GetAblationHash("seed", GURL("https://www.example.com"), t),
            GetAblationHash("seed", GURL("http://www.example.com"), t));

  // Different domain
  EXPECT_NE(GetAblationHash("seed", GURL("https://www.example.com"), t),
            GetAblationHash("seed", GURL("https://www.foo.com"), t));

  // Different path makes no difference, the path is not part of the security
  // origin.
  EXPECT_EQ(GetAblationHash("seed", GURL("https://www.example.com"), t),
            GetAblationHash("seed", GURL("https://www.example.com/a"), t));
}

TEST_F(AutofillAblationStudyTestInUTC, GetAblationHash_DependsOnDay) {
  GURL url = GetDefaultUrl();
  base::Time t = GetDefaultTime();

  // 1 minute difference but not crossing the break of the day.
  EXPECT_EQ(GetAblationHash("seed", url, t),
            GetAblationHash("seed", url, t + base::Minutes(1)));

  // 1 day difference leads to crossing the break of the day.
  EXPECT_NE(GetAblationHash("seed", url, t),
            GetAblationHash("seed", url, t + base::Hours(24)));
}

// Ensure that if the feature is disabled, only kDefault is returned.
TEST_F(AutofillAblationStudyTestInUTC, FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kAutofillEnableAblationStudy);
  AutofillAblationStudy study;
  auto result = RunNIterations(study, 100, FormTypeForAblationStudy::kAddress);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(100, result[AblationGroup::kDefault]);
}

// Ensure that if the weight is invalid, only kDefault is returned.
TEST_F(AutofillAblationStudyTestInUTC, InvalidParameters) {
  base::test::ScopedFeatureList features;
  AutofillAblationStudy study;
  base::FieldTrialParams feature_parameters{
      {kAutofillAblationStudyEnabledForAddressesParam.name, "true"},
      {kAutofillAblationStudyEnabledForPaymentsParam.name, "true"},
      // Ablation weight is > 1000 (the maximum)
      {kAutofillAblationStudyAblationWeightPerMilleParam.name, "5000"},
  };
  features.InitAndEnableFeatureWithParameters(kAutofillEnableAblationStudy,
                                              feature_parameters);
  auto result = RunNIterations(study, 100, FormTypeForAblationStudy::kAddress);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(100, result[AblationGroup::kDefault]);
}

// Ensure that if the feature is enabled but the individual form types are
// disabled, only kDefault is returned.
TEST_F(AutofillAblationStudyTestInUTC, FormTypesDisabled) {
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {kAutofillAblationStudyEnabledForAddressesParam.name, "false"},
      {kAutofillAblationStudyEnabledForPaymentsParam.name, "false"},
      {kAutofillAblationStudyAblationWeightPerMilleParam.name, "1000"},
  };
  features.InitAndEnableFeatureWithParameters(kAutofillEnableAblationStudy,
                                              feature_parameters);
  AutofillAblationStudy study;
  auto result = RunNIterations(study, 100, FormTypeForAblationStudy::kAddress);
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(100, result[AblationGroup::kDefault]);
}

// Perform some plausibility check if the feature is fully enabled.
TEST_F(AutofillAblationStudyTestInUTC, IntegrationTest) {
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {kAutofillAblationStudyEnabledForAddressesParam.name, "true"},
      {kAutofillAblationStudyEnabledForPaymentsParam.name, "true"},
      // 10% chance for ablation group, 10% chance for control group,
      // 80% change for default group.
      {kAutofillAblationStudyAblationWeightPerMilleParam.name, "100"},
  };
  features.InitAndEnableFeatureWithParameters(kAutofillEnableAblationStudy,
                                              feature_parameters);
  AutofillAblationStudy study;
  auto result = RunNIterations(study, 1000, FormTypeForAblationStudy::kAddress);
  EXPECT_EQ(3u, result.size());
  // Note that these are not guaranteed but the chances are good enough that we
  // can risk it.
  EXPECT_NE(0, result[AblationGroup::kDefault]);
  EXPECT_NE(0, result[AblationGroup::kAblation]);
  EXPECT_NE(0, result[AblationGroup::kControl]);
  EXPECT_LT(result[AblationGroup::kAblation] + result[AblationGroup::kControl],
            result[AblationGroup::kDefault]);
}

}  // namespace autofill
