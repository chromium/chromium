// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_data_model.h"

#include <stddef.h>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/test_autofill_data_model.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Tests that when recording a use, the last, second to last, etc. use dates are
// updated correctly.
TEST(AutofillDataModelTest, RecordUseDate) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature{
      features::kAutofillTrackMultipleUseDates};

  // Data model creation counts as a use.
  TestAutofillDataModel model(/*usage_history_size=*/3);
  EXPECT_EQ(model.use_date(1), base::Time::Now());
  EXPECT_FALSE(model.use_date(2).has_value());
  EXPECT_FALSE(model.use_date(3).has_value());

  // Recording a use should propagate the change.
  task_environment.FastForwardBy(base::Seconds(123));
  base::Time old_last_use_date = model.use_date();
  model.RecordUseDate(base::Time::Now());
  EXPECT_EQ(model.use_date(1), base::Time::Now());
  EXPECT_EQ(model.use_date(2), old_last_use_date);
  EXPECT_FALSE(model.use_date(3).has_value());

  // Record a second use.
  task_environment.FastForwardBy(base::Seconds(234));
  old_last_use_date = model.use_date();
  base::Time old_second_last_use_date = *model.use_date(2);
  model.RecordUseDate(base::Time::Now());
  EXPECT_EQ(model.use_date(1), base::Time::Now());
  EXPECT_EQ(model.use_date(2), old_last_use_date);
  EXPECT_EQ(model.use_date(3), old_second_last_use_date);
}

enum Expectation { GREATER, LESS };
struct AutofillDataModelRankingTestCase {
  const int use_count_a;
  const base::Time use_date_a;
  const int use_count_b;
  const base::Time use_date_b;
  Expectation expectation;
};

base::Time now = AutofillClock::Now();

class HasGreaterFrecencyThanTest
    : public testing::TestWithParam<AutofillDataModelRankingTestCase> {};

TEST_P(HasGreaterFrecencyThanTest, HasGreaterFrecencyThan) {
  auto test_case = GetParam();
  TestAutofillDataModel model_a(test_case.use_count_a, test_case.use_date_a);
  TestAutofillDataModel model_b(test_case.use_count_b, test_case.use_date_b);

  EXPECT_EQ(test_case.expectation == GREATER,
            model_a.HasGreaterRankingThan(&model_b, now));
  EXPECT_NE(test_case.expectation == GREATER,
            model_b.HasGreaterRankingThan(&model_a, now));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataModelTest,
    HasGreaterFrecencyThanTest,
    testing::Values(
        // Same days since last use, model_a has a bigger use count.
        AutofillDataModelRankingTestCase{10, now, 8, now, GREATER},
        // Same days since last use, model_a has a smaller use count.
        AutofillDataModelRankingTestCase{8, now, 10, now, LESS},
        // Same days since last use, model_a has larger use count.
        AutofillDataModelRankingTestCase{8, now, 8, now - base::Days(1),
                                         GREATER},
        // Same use count, model_a has smaller days since last use.
        AutofillDataModelRankingTestCase{8, now - base::Days(1), 8, now, LESS},
        // Special case: occasional profiles. A profile with relatively low
        // usage and used recently (model_b) should not rank higher than a more
        // used profile that has been unused for a short amount of time
        // (model_a).
        AutofillDataModelRankingTestCase{300, now - base::Days(5), 10,
                                         now - base::Days(1), GREATER},
        // Special case: moving. A new profile used frequently (model_b) should
        // rank higher than a profile with more usage that has not been used for
        // a while (model_a).
        AutofillDataModelRankingTestCase{300, now - base::Days(15), 10,
                                         now - base::Days(1), LESS}));

// Tests that when merging two models, the use dates are merged correctly.
struct UseDateMergeTestCase {
  // Describes how long ago the last, second last and third last uses occurred,
  // respectively. Nullopt indicate that the model hasn't been used this often.
  using UsageHistory = std::array<std::optional<base::TimeDelta>, 3>;
  const UsageHistory usage_history_a;
  const UsageHistory usage_history_b;
  const UsageHistory expected_merged_use_history;
};
class UseDateMergeTest : public testing::TestWithParam<UseDateMergeTestCase> {
 public:
  TestAutofillDataModel ModelFromUsageHistory(
      const UseDateMergeTestCase::UsageHistory& usage_history) {
    TestAutofillDataModel model(/*usage_history_size=*/3);
    for (int i = 0; i < 3; i++) {
      if (usage_history[i]) {
        model.set_use_date(base::Time::Now() - *usage_history[i], i + 1);
      }
    }
    return model;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_{
      features::kAutofillTrackMultipleUseDates};
};

TEST_P(UseDateMergeTest, MergeUseDates) {
  const UseDateMergeTestCase& test = GetParam();
  TestAutofillDataModel a = ModelFromUsageHistory(test.usage_history_a);
  const TestAutofillDataModel b = ModelFromUsageHistory(test.usage_history_b);
  const TestAutofillDataModel expected_merged_profile =
      ModelFromUsageHistory(test.expected_merged_use_history);

  ASSERT_EQ(a.usage_history_size(), 3u);
  a.MergeUseDates(b);
  EXPECT_EQ(a.use_date(1), expected_merged_profile.use_date(1));
  EXPECT_EQ(a.use_date(2), expected_merged_profile.use_date(2));
  EXPECT_EQ(a.use_date(3), expected_merged_profile.use_date(3));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillDataModelTest,
    UseDateMergeTest,
    testing::Values(
        // No second/third use date set: Expect that the more recent use date
        // becomes the use date and the other use date the second use date.
        UseDateMergeTestCase{{base::Days(1)},
                             {base::Days(2)},
                             {base::Days(1), base::Days(2)}},
        UseDateMergeTestCase{{base::Days(2)},
                             {base::Days(1)},
                             {base::Days(1), base::Days(2)}},
        // At least three use dates are set: Expect that the second and third
        // use date of the merged profile are populated.
        UseDateMergeTestCase{{base::Days(1), base::Days(3), base::Days(5)},
                             {base::Days(2), base::Days(4), base::Days(6)},
                             {base::Days(1), base::Days(2), base::Days(3)}}));

}  // namespace

}  // namespace autofill
