// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_data_model.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/test_autofill_data_model.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using structured_address::VerificationStatus;

namespace {

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);

}  // namespace

TEST(AutofillDataModelTest, IsVerified) {
  TestAutofillDataModel model("guid", std::string());
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("http://www.example.com");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("https://www.example.com");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("file:///tmp/example.txt");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("data:text/plain;charset=utf-8;base64,ZXhhbXBsZQ==");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin("chrome://settings/autofill");
  EXPECT_FALSE(model.IsVerified());

  model.set_origin(kSettingsOrigin);
  EXPECT_TRUE(model.IsVerified());

  model.set_origin("Some gibberish string");
  EXPECT_TRUE(model.IsVerified());

  model.set_origin(std::string());
  EXPECT_FALSE(model.IsVerified());
}

TEST(AutofillDataModelTest, GetMetadata) {
  TestAutofillDataModel model("guid", std::string());
  model.set_use_count(10);
  model.set_use_date(kArbitraryTime);

  AutofillMetadata metadata = model.GetMetadata();
  EXPECT_EQ(model.use_count(), metadata.use_count);
  EXPECT_EQ(model.use_date(), metadata.use_date);
}

TEST(AutofillDataModelTest, SetMetadata) {
  AutofillMetadata metadata;
  metadata.use_count = 10;
  metadata.use_date = kArbitraryTime;

  TestAutofillDataModel model("guid", std::string());
  EXPECT_TRUE(model.SetMetadata(metadata));
  EXPECT_EQ(metadata.use_count, model.use_count());
  EXPECT_EQ(metadata.use_date, model.use_date());
}

TEST(AutofillDataModelTest, IsDeletable) {
  TestAutofillDataModel model("guid", std::string());
  model.set_use_date(kArbitraryTime);

  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  EXPECT_FALSE(model.IsDeletable());

  test_clock.SetNow(kArbitraryTime + kDisusedDataModelDeletionTimeDelta +
                    base::Days(1));
  EXPECT_TRUE(model.IsDeletable());
}

enum Expectation { GREATER, LESS };
struct AutofillDataModelRankingTestCase {
  const std::string guid_a;
  const int use_count_a;
  const base::Time use_date_a;
  const std::string guid_b;
  const int use_count_b;
  const base::Time use_date_b;
  Expectation expectation;
};

base::Time now = AutofillClock::Now();

class AutofillDataModelRankingTest
    : public testing::TestWithParam<AutofillDataModelRankingTestCase> {};

TEST_P(AutofillDataModelRankingTest, HasGreaterRankingThan) {
  // Enable kAutofillEnableRankingFormula so that it uses the new formula
  // instead of frecency.
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(features::kAutofillEnableRankingFormula);

  auto test_case = GetParam();
  TestAutofillDataModel model_a(test_case.guid_a, test_case.use_count_a,
                                test_case.use_date_a);
  TestAutofillDataModel model_b(test_case.guid_b, test_case.use_count_b,
                                test_case.use_date_b);

  EXPECT_EQ(test_case.expectation == GREATER,
            model_a.HasGreaterRankingThan(&model_b, now));
  EXPECT_NE(test_case.expectation == GREATER,
            model_b.HasGreaterRankingThan(&model_a, now));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataModelTest,
    AutofillDataModelRankingTest,
    testing::Values(
        // Same ranking score, model_a has a smaller GUID (tie breaker).
        AutofillDataModelRankingTestCase{"guid_a", 8, now, "guid_b", 8, now,
                                         LESS},
        // Same days since last use, model_a has a bigger use count.
        AutofillDataModelRankingTestCase{"guid_a", 10, now, "guid_b", 8, now,
                                         GREATER},
        // Same days since last use, model_a has a smaller use count.
        AutofillDataModelRankingTestCase{"guid_a", 8, now, "guid_b", 10, now,
                                         LESS},
        // Same days since last use, model_a has larger use count.
        AutofillDataModelRankingTestCase{"guid_a", 8, now, "guid_b", 8,
                                         now - base::Days(1), GREATER},
        // Same use count, model_a has smaller days since last use.
        AutofillDataModelRankingTestCase{"guid_a", 8, now - base::Days(1),
                                         "guid_b", 8, now, LESS},
        // Special case: occasional profiles. A profile with relatively low
        // usage and used recently (model_b) should not rank higher than a more
        // used profile that has been unused for a short amount of time
        // (model_a).
        AutofillDataModelRankingTestCase{"guid_a", 300, now - base::Days(5),
                                         "guid_b", 10, now - base::Days(1),
                                         GREATER},
        // Special case: moving. A new profile used frequently (model_b) should
        // rank higher than a profile with more usage that has not been used for
        // a while (model_a).
        AutofillDataModelRankingTestCase{"guid_a", 90, now - base::Days(20),
                                         "guid_b", 10, now - base::Days(5),
                                         LESS}));

class HasGreaterFrecencyThanTest
    : public testing::TestWithParam<AutofillDataModelRankingTestCase> {};

TEST_P(HasGreaterFrecencyThanTest, HasGreaterFrecencyThan) {
  auto test_case = GetParam();
  TestAutofillDataModel model_a(test_case.guid_a, test_case.use_count_a,
                                test_case.use_date_a);
  TestAutofillDataModel model_b(test_case.guid_b, test_case.use_count_b,
                                test_case.use_date_b);

  EXPECT_EQ(test_case.expectation == GREATER,
            model_a.HasGreaterRankingThan(&model_b, now));
  EXPECT_NE(test_case.expectation == GREATER,
            model_b.HasGreaterRankingThan(&model_a, now));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataModelTest,
    HasGreaterFrecencyThanTest,
    testing::Values(
        // Same ranking score, model_a has a smaller GUID (tie breaker).
        AutofillDataModelRankingTestCase{"guid_a", 8, now, "guid_b", 8, now,
                                         LESS},
        // Same days since last use, model_a has a bigger use count.
        AutofillDataModelRankingTestCase{"guid_a", 10, now, "guid_b", 8, now,
                                         GREATER},
        // Same days since last use, model_a has a smaller use count.
        AutofillDataModelRankingTestCase{"guid_a", 8, now, "guid_b", 10, now,
                                         LESS},
        // Same days since last use, model_a has larger use count.
        AutofillDataModelRankingTestCase{"guid_a", 8, now, "guid_b", 8,
                                         now - base::Days(1), GREATER},
        // Same use count, model_a has smaller days since last use.
        AutofillDataModelRankingTestCase{"guid_a", 8, now - base::Days(1),
                                         "guid_b", 8, now, LESS},
        // Special case: occasional profiles. A profile with relatively low
        // usage and used recently (model_b) should not rank higher than a more
        // used profile that has been unused for a short amount of time
        // (model_a).
        AutofillDataModelRankingTestCase{"guid_a", 300, now - base::Days(5),
                                         "guid_b", 10, now - base::Days(1),
                                         GREATER},
        // Special case: moving. A new profile used frequently (model_b) should
        // rank higher than a profile with more usage that has not been used for
        // a while (model_a).
        AutofillDataModelRankingTestCase{"guid_a", 300, now - base::Days(15),
                                         "guid_b", 10, now - base::Days(1),
                                         LESS}));

}  // namespace autofill
