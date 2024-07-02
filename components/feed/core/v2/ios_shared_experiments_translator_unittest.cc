// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_experiments_translator.h"

#include "base/test/scoped_feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

class ExperimentsTranslatorTest : public testing::Test {
 public:
  ExperimentsTranslatorTest() = default;
  ExperimentsTranslatorTest(ExperimentsTranslatorTest&) = delete;
  ExperimentsTranslatorTest& operator=(const ExperimentsTranslatorTest&) =
      delete;
  ~ExperimentsTranslatorTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExperimentsTranslatorTest, ExperimentsAreTranslatedForGroupNameOnly) {
  Experiments expected;
  std::vector<ExperimentGroup> group_list{{"Group1"}};
  expected["Trial1"] = group_list;

  feedwire::ChromeFeedResponseMetadata metadata;
  auto* exp = metadata.add_experiments();
  exp->set_trial_name("Trial1");
  exp->set_group_name("Group1");

  std::optional<Experiments> e = TranslateExperiments(metadata.experiments());
  ASSERT_TRUE(e.has_value());

  EXPECT_EQ(e, expected);
}

TEST_F(ExperimentsTranslatorTest,
       ExperimentsAreTranslatedForBothGroupNameAndExperimentId) {
  Experiments expected;
  std::vector<ExperimentGroup> group_list{{"Group1", 12345}};
  expected["Trial1"] = group_list;

  feedwire::ChromeFeedResponseMetadata metadata;
  auto* exp1 = metadata.add_experiments();
  exp1->set_trial_name("Trial1");
  exp1->set_group_name("Group1");
  exp1->set_experiment_id("12345");

  // Group name must be present.
  auto* exp2 = metadata.add_experiments();
  exp2->set_experiment_id("EXP_ID_NOT_TRANSLATED");
  auto* exp3 = metadata.add_experiments();
  exp3->set_trial_name("Trial_NOT_TRANSLATED");
  exp3->set_experiment_id("EXP_ID_NOT_TRANSLATED");

  std::optional<Experiments> e = TranslateExperiments(metadata.experiments());
  ASSERT_TRUE(e.has_value());

  EXPECT_EQ(e, expected);
}

TEST_F(ExperimentsTranslatorTest,
       ExperimentsAreNotTranslatedGroupAndIDMissing) {
  feedwire::ChromeFeedResponseMetadata metadata;
  auto* exp1 = metadata.add_experiments();
  exp1->set_trial_name("Trial1");

  std::optional<Experiments> e = TranslateExperiments(metadata.experiments());
  ASSERT_FALSE(e.has_value());
}

}  // namespace feed
