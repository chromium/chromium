// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/chrome_variations_configuration.h"

#include <map>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/configuration_provider.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/feature_engagement/public/stats.h"
#include "components/feature_engagement/public/tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kChromeTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeTestFeatureQux,
             "test_qux",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeTestGroupOne,
             "test_group_one",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeTestGroupTwo,
             "test_group_two",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kChromeTestGroupThree,
             "test_group_three",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kFooTrialName[] = "FooTrial";
const char kBarTrialName[] = "BarTrial";
const char kQuxTrialName[] = "QuxTrial";
const char kGroupOneTrialName[] = "GroupOneTrial";
const char kGroupTwoTrialName[] = "GroupTwoTrial";
const char kGroupThreeTrialName[] = "GroupThreeTrial";
const char kTrialGroupName[] = "FieldTrialGroup1";
const char kConfigParseEventName[] = "InProductHelp.Config.ParsingEvent";

SessionRateImpact CreateSessionRateImpactExplicit(
    std::vector<std::string> affected_features) {
  SessionRateImpact impact;
  impact.type = SessionRateImpact::Type::EXPLICIT;
  impact.affected_features = affected_features;
  return impact;
}

BlockedBy CreateBlockedByExplicit(std::vector<std::string> affected_features) {
  BlockedBy blocked_by;
  blocked_by.type = BlockedBy::Type::EXPLICIT;
  blocked_by.affected_features = affected_features;
  return blocked_by;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TestConfigurationProvider : public ConfigurationProvider {
 public:
  TestConfigurationProvider() = default;
  ~TestConfigurationProvider() override = default;

  // ConfigurationProvider:
  bool MaybeProvideFeatureConfiguration(
      const base::Feature& feature,
      feature_engagement::FeatureConfig& config,
      const feature_engagement::FeatureVector& known_features,
      const feature_engagement::GroupVector& known_groups) const override {
    config = config_;
    return true;
  }

  const char* GetConfigurationSourceDescription() const override {
    return "Test Configuration Provider";
  }

  std::set<std::string> MaybeProvideAllowedEventPrefixes(
      const base::Feature& feature) const override {
    return {"TestEventPrefix"};
  }

  void SetConfig(const FeatureConfig& config) { config_ = config; }

 private:
  FeatureConfig config_;
};
#endif

class ChromeVariationsConfigurationTest : public ::testing::Test {
 public:
  ChromeVariationsConfigurationTest() {
    base::FieldTrial* foo_trial =
        base::FieldTrialList::CreateFieldTrial(kFooTrialName, kTrialGroupName);
    base::FieldTrial* bar_trial =
        base::FieldTrialList::CreateFieldTrial(kBarTrialName, kTrialGroupName);
    base::FieldTrial* qux_trial =
        base::FieldTrialList::CreateFieldTrial(kQuxTrialName, kTrialGroupName);
    base::FieldTrial* group_one_trial = base::FieldTrialList::CreateFieldTrial(
        kGroupOneTrialName, kTrialGroupName);
    base::FieldTrial* group_two_trial = base::FieldTrialList::CreateFieldTrial(
        kGroupTwoTrialName, kTrialGroupName);
    base::FieldTrial* group_three_trial =
        base::FieldTrialList::CreateFieldTrial(kGroupThreeTrialName,
                                               kTrialGroupName);
    trials_[kChromeTestFeatureFoo.name] = foo_trial;
    trials_[kChromeTestFeatureBar.name] = bar_trial;
    trials_[kChromeTestFeatureQux.name] = qux_trial;
    trials_[kChromeTestGroupOne.name] = group_one_trial;
    trials_[kChromeTestGroupTwo.name] = group_two_trial;
    trials_[kChromeTestGroupThree.name] = group_three_trial;

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        kChromeTestFeatureFoo.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        foo_trial);
    feature_list->RegisterFieldTrialOverride(
        kChromeTestFeatureBar.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        bar_trial);
    feature_list->RegisterFieldTrialOverride(
        kChromeTestFeatureQux.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        qux_trial);
    feature_list->RegisterFieldTrialOverride(
        kChromeTestGroupOne.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        group_one_trial);
    feature_list->RegisterFieldTrialOverride(
        kChromeTestGroupTwo.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        group_two_trial);
    feature_list->RegisterFieldTrialOverride(
        kChromeTestGroupThree.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        group_three_trial);

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
    EXPECT_EQ(foo_trial,
              base::FeatureList::GetFieldTrial(kChromeTestFeatureFoo));
    EXPECT_EQ(bar_trial,
              base::FeatureList::GetFieldTrial(kChromeTestFeatureBar));
    EXPECT_EQ(qux_trial,
              base::FeatureList::GetFieldTrial(kChromeTestFeatureQux));
    EXPECT_EQ(group_one_trial,
              base::FeatureList::GetFieldTrial(kChromeTestGroupOne));
    EXPECT_EQ(group_two_trial,
              base::FeatureList::GetFieldTrial(kChromeTestGroupTwo));
    EXPECT_EQ(group_three_trial,
              base::FeatureList::GetFieldTrial(kChromeTestGroupThree));
  }

  ChromeVariationsConfigurationTest(const ChromeVariationsConfigurationTest&) =
      delete;
  ChromeVariationsConfigurationTest& operator=(
      const ChromeVariationsConfigurationTest&) = delete;

  void TearDown() override {
    // This is required to ensure each test can define its own params.
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  void SetFeatureParams(const base::Feature& feature,
                        std::map<std::string, std::string> params) {
    ASSERT_TRUE(
        base::FieldTrialParamAssociator::GetInstance()
            ->AssociateFieldTrialParams(trials_[feature.name]->trial_name(),
                                        kTrialGroupName, params));

    std::map<std::string, std::string> actualParams;
    EXPECT_TRUE(base::GetFieldTrialParamsByFeature(feature, &actualParams));
    EXPECT_EQ(params, actualParams);
  }

  void LoadConfigs(const FeatureVector& features, const GroupVector& groups) {
    configuration_.LoadConfigs(Tracker::GetDefaultConfigurationProviders(),
                               features, groups);
  }

 protected:
  void VerifyInvalid(const std::string& event_config) {
    std::map<std::string, std::string> foo_params;
    foo_params["event_used"] = "name:u;comparator:any;window:0;storage:1";
    foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
    foo_params["event_0"] = event_config;
    SetFeatureParams(kChromeTestFeatureFoo, foo_params);

    std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
    GroupVector groups;
    LoadConfigs(features, groups);

    FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
    EXPECT_FALSE(foo.valid);
  }

  ChromeVariationsConfiguration configuration_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<std::string, base::FieldTrial*> trials_;
};

}  // namespace

TEST_F(ChromeVariationsConfigurationTest,
       DisabledFeatureShouldHaveInvalidConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {kChromeTestFeatureFoo});

  FeatureVector features;
  features.push_back(&kChromeTestFeatureFoo);
  GroupVector groups;
  base::HistogramTester histogram_tester;

  LoadConfigs(features, groups);

  FeatureConfig foo_config =
      configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo_config.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, ParseSingleFeature) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] =
      "name:page_download_started;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] =
      "name:opened_chrome_home;comparator:any;window:0;storage:360";
  foo_params["event_1"] =
      "name:user_has_seen_dino;comparator:>=1;window:120;storage:180";
  foo_params["event_2"] =
      "name:user_opened_app_menu;comparator:<=0;window:120;storage:180";
  foo_params["event_3"] =
      "name:user_opened_downloads_home;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used =
      EventConfig("page_download_started", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger =
      EventConfig("opened_chrome_home", Comparator(ANY, 0), 0, 360);
  expected_foo.event_configs.insert(EventConfig(
      "user_has_seen_dino", Comparator(GREATER_THAN_OR_EQUAL, 1), 120, 180));
  expected_foo.event_configs.insert(EventConfig(
      "user_opened_app_menu", Comparator(LESS_THAN_OR_EQUAL, 0), 120, 180));
  expected_foo.event_configs.insert(
      EventConfig("user_opened_downloads_home", Comparator(ANY, 0), 0, 360));
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest, ParsePrefixedParamNames) {
  std::map<std::string, std::string> foo_params;
  // Prefixed param names.
  foo_params["test_foo_session_rate"] = "!=6";
  foo_params["test_foo_availability"] = ">=1";
  foo_params["test_foo_event_used"] =
      "name:page_download_started;comparator:any;window:0;storage:360";
  foo_params["test_foo_event_trigger"] =
      "name:opened_chrome_home;comparator:any;window:0;storage:360";
  foo_params["test_foo_event_1"] =
      "name:user_has_seen_dino;comparator:>=1;window:120;storage:180";
  foo_params["event_2"] =
      "name:user_opened_app_menu;comparator:<=0;window:120;storage:180";
  foo_params["event_3"] =
      "name:user_opened_downloads_home;comparator:any;window:0;storage:360";

  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used =
      EventConfig("page_download_started", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger =
      EventConfig("opened_chrome_home", Comparator(ANY, 0), 0, 360);
  expected_foo.event_configs.insert(EventConfig(
      "user_has_seen_dino", Comparator(GREATER_THAN_OR_EQUAL, 1), 120, 180));
  expected_foo.event_configs.insert(EventConfig(
      "user_opened_app_menu", Comparator(LESS_THAN_OR_EQUAL, 0), 120, 180));
  expected_foo.event_configs.insert(
      EventConfig("user_opened_downloads_home", Comparator(ANY, 0), 0, 360));
  expected_foo.session_rate = Comparator(NOT_EQUAL, 6);
  expected_foo.availability = Comparator(GREATER_THAN_OR_EQUAL, 1);
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest,
       MultipleFeaturesWithPrefixedAndUnprefixedParams) {
  std::map<std::string, std::string> foo_params;
  foo_params["test_foo_session_rate"] = "!=6";
  foo_params["test_foo_availability"] = ">=1";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["test_bar_session_rate"] = "!=7";
  bar_params["test_bar_availability"] = ">=2";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  std::map<std::string, std::string> qux_params;
  qux_params["session_rate"] = "!=3";
  qux_params["availability"] = ">=5";
  SetFeatureParams(kChromeTestFeatureQux, qux_params);

  std::vector<const base::Feature*> features = {
      &kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  FeatureConfig bar = configuration_.GetFeatureConfig(kChromeTestFeatureBar);
  FeatureConfig qux = configuration_.GetFeatureConfig(kChromeTestFeatureQux);

  FeatureConfig expected_foo;
  expected_foo.session_rate = Comparator(NOT_EQUAL, 6);
  expected_foo.availability = Comparator(GREATER_THAN_OR_EQUAL, 1);
  EXPECT_EQ(expected_foo, foo);

  FeatureConfig expected_bar;
  expected_bar.session_rate = Comparator(NOT_EQUAL, 7);
  expected_bar.availability = Comparator(GREATER_THAN_OR_EQUAL, 2);
  EXPECT_EQ(expected_bar, bar);

  FeatureConfig expected_qux;
  expected_qux.session_rate = Comparator(NOT_EQUAL, 3);
  expected_qux.availability = Comparator(GREATER_THAN_OR_EQUAL, 5);
  EXPECT_EQ(expected_qux, qux);
}

TEST_F(ChromeVariationsConfigurationTest, MissingUsedIsInvalid) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_USED_EVENT_MISSING),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

TEST_F(ChromeVariationsConfigurationTest, MissingTriggerIsInvalid) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(
          stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

TEST_F(ChromeVariationsConfigurationTest, OnlyTriggerAndUsedIsValid) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  EXPECT_EQ(expected_foo, foo);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

void RunBlockedByTest(ChromeVariationsConfigurationTest* test,
                      ChromeVariationsConfiguration* configuration,
                      std::vector<const base::Feature*> features,
                      std::string blocked_by_param_value,
                      BlockedBy expected_blocked_by,
                      bool is_valid) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["blocked_by"] = blocked_by_param_value;
  test->SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  GroupVector groups;
  configuration->LoadConfigs(Tracker::GetDefaultConfigurationProviders(),
                             features, groups);
  FeatureConfig foo = configuration->GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(is_valid, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = is_valid;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.blocked_by = expected_blocked_by;
  EXPECT_EQ(expected_foo, foo);
  if (is_valid) {
    histogram_tester.ExpectBucketCount(
        kConfigParseEventName,
        static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  } else {
    histogram_tester.ExpectBucketCount(
        kConfigParseEventName,
        static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  }
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockingNone) {
  std::map<std::string, std::string> foo_params;
  foo_params["blocking"] = "none";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.blocking.type, Blocking::Type::NONE);
  EXPECT_TRUE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockingAll) {
  std::map<std::string, std::string> foo_params;
  foo_params["blocking"] = "all";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.blocking.type, Blocking::Type::ALL);
  EXPECT_TRUE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, ParseInvalidBlockingParams) {
  std::map<std::string, std::string> foo_params;
  // Anything other than all/none should be invalid.
  foo_params["blocking"] = "123";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.blocking.type, Blocking::Type::ALL);
  EXPECT_FALSE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByAll) {
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo}, "all",
                   BlockedBy(), true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByNone) {
  BlockedBy blocked_by;
  blocked_by.type = BlockedBy::Type::NONE;
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo}, "none",
                   blocked_by, true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByExplicitSelf) {
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo},
                   kChromeTestFeatureFoo.name,
                   CreateBlockedByExplicit({kChromeTestFeatureFoo.name}),
                   true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByExplicitOther) {
  RunBlockedByTest(this, &configuration_,
                   {&kChromeTestFeatureFoo, &kChromeTestFeatureBar},
                   kChromeTestFeatureBar.name,
                   CreateBlockedByExplicit({kChromeTestFeatureBar.name}),
                   true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByExplicitMultiple) {
  RunBlockedByTest(
      this, &configuration_,
      {&kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux},
      "test_bar,test_qux",
      CreateBlockedByExplicit(
          {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name}),
      true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByExplicitEmpty) {
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo}, "",
                   BlockedBy(), false /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByExplicitOnlySeparator) {
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo}, ",",
                   BlockedBy(), false /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseBlockedByExplicitUnknownFeature) {
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo}, "random",
                   BlockedBy(), false /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseBlockedByExplicitAll) {
  RunBlockedByTest(this, &configuration_, {&kChromeTestFeatureFoo},
                   "all,test_foo", BlockedBy(), false /* is_valid */);
}

void RunSessionRateImpactTest(ChromeVariationsConfigurationTest* test,
                              ChromeVariationsConfiguration* configuration,
                              std::vector<const base::Feature*> features,
                              std::string session_rate_impact_param_value,
                              SessionRateImpact expected_impact,
                              bool is_valid) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["session_rate_impact"] = session_rate_impact_param_value;
  test->SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  GroupVector groups;
  configuration->LoadConfigs(Tracker::GetDefaultConfigurationProviders(),
                             features, groups);
  FeatureConfig foo = configuration->GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(is_valid, foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = is_valid;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.session_rate_impact =
      is_valid ? expected_impact : SessionRateImpact();
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest, SessionRateImpactAll) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(this, &configuration_, {&kChromeTestFeatureFoo},
                           "all", SessionRateImpact(), true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, SessionRateImpactNone) {
  base::HistogramTester histogram_tester;
  SessionRateImpact impact;
  impact.type = SessionRateImpact::Type::NONE;
  RunSessionRateImpactTest(this, &configuration_, {&kChromeTestFeatureFoo},
                           "none", impact, true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, SessionRateImpactExplicitSelf) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(
      this, &configuration_, {&kChromeTestFeatureFoo}, "test_foo",
      CreateSessionRateImpactExplicit({kChromeTestFeatureFoo.name}),
      true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, SessionRateImpactExplicitOther) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(
      this, &configuration_, {&kChromeTestFeatureFoo, &kChromeTestFeatureBar},
      "test_bar", CreateSessionRateImpactExplicit({kChromeTestFeatureBar.name}),
      true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  // bar has no configuration.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

TEST_F(ChromeVariationsConfigurationTest, SessionRateImpactExplicitMultiple) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(
      this, &configuration_,
      {&kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux},
      "test_bar,test_qux",
      CreateSessionRateImpactExplicit(
          {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name}),
      true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  // bar and qux have no configuration.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 2);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 3);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitAtLeastOneValidFeature) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(
      this, &configuration_,
      {&kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux},
      "test_foo,no_feature",
      CreateSessionRateImpactExplicit(
          {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name}),
      true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::
                           FAILURE_SESSION_RATE_IMPACT_UNKNOWN_FEATURE),
      1);
  // bar and qux have no configuration.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 2);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 4);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitAtLeastOneValidFeaturePlusEmpty) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(
      this, &configuration_,
      {&kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux},
      "test_foo, ",
      CreateSessionRateImpactExplicit(
          {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name}),
      true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  // bar and qux have no configuration.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 2);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 3);
}

void TestInvalidSessionRateImpactParamValue(
    ChromeVariationsConfigurationTest* test,
    ChromeVariationsConfiguration* configuration,
    std::string session_rate_impact_param_value,
    uint32_t parse_errors,
    uint32_t unknown_features) {
  base::HistogramTester histogram_tester;
  RunSessionRateImpactTest(
      test, configuration,
      {&kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux},
      session_rate_impact_param_value, SessionRateImpact(),
      false /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  if (parse_errors > 0) {
    histogram_tester.ExpectBucketCount(
        kConfigParseEventName,
        static_cast<int>(
            stats::ConfigParsingEvent::FAILURE_SESSION_RATE_IMPACT_PARSE),
        parse_errors);
  }
  if (unknown_features > 0) {
    histogram_tester.ExpectBucketCount(
        kConfigParseEventName,
        static_cast<int>(stats::ConfigParsingEvent::
                             FAILURE_SESSION_RATE_IMPACT_UNKNOWN_FEATURE),
        unknown_features);
  }
  // bar and qux has no configuration.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 2);
  histogram_tester.ExpectTotalCount(kConfigParseEventName,
                                    3 + parse_errors + unknown_features);
}

TEST_F(ChromeVariationsConfigurationTest, SessionRateImpactExplicitEmpty) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "", 1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitEmptyWhitespace) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "   ", 1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitOnlySeparator) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, ",", 1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitOnlySeparatorWhitespace) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, " ,  ", 1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitOnlyUnknownFeature) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "not_feature",
                                         1, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitOnlyMultipleUnknownFeatures) {
  TestInvalidSessionRateImpactParamValue(
      this, &configuration_, "not_feature,another_not_feature", 1, 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitOnlyUnknownFeaturePlusEmpty) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "not_feature, ",
                                         1, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameAllFirst) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "all,test_foo",
                                         1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameAllLast) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "test_foo,all",
                                         1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameAllLastInvalidFirst) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_,
                                         "not_feature,all", 1, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameNoneFirst) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "none,test_foo",
                                         1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameNoneLast) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_, "test_foo,none",
                                         1, 0);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameNoneLastInvalidFirst) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_,
                                         "not_feature,none", 1, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       SessionRateImpactExplicitInvalidFeatureNameFromCodeReview) {
  TestInvalidSessionRateImpactParamValue(this, &configuration_,
                                         "test_foo,all,none,mwahahaha", 1, 0);
}

void RunGroupsTest(ChromeVariationsConfigurationTest* test,
                   ChromeVariationsConfiguration* configuration,
                   FeatureVector features,
                   GroupVector groups,
                   std::string groups_param_value,
                   std::vector<std::string> expected_groups,
                   bool is_valid) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["groups"] = groups_param_value;
  test->SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  configuration->LoadConfigs(Tracker::GetDefaultConfigurationProviders(),
                             features, groups);
  FeatureConfig foo = configuration->GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(is_valid, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = is_valid;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.groups = expected_groups;
  EXPECT_EQ(expected_foo, foo);
  if (is_valid) {
    histogram_tester.ExpectBucketCount(
        kConfigParseEventName,
        static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  } else {
    histogram_tester.ExpectBucketCount(
        kConfigParseEventName,
        static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  }
}

TEST_F(ChromeVariationsConfigurationTest, ParseGroupsItem) {
  RunGroupsTest(this, &configuration_, {&kChromeTestFeatureFoo},
                {&kChromeTestGroupOne}, kChromeTestGroupOne.name,
                {kChromeTestGroupOne.name}, true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseGroupsMultiple) {
  RunGroupsTest(this, &configuration_, {&kChromeTestFeatureFoo},
                {&kChromeTestGroupOne, &kChromeTestGroupTwo},
                "test_group_one,test_group_two",
                {kChromeTestGroupOne.name, kChromeTestGroupTwo.name},
                true /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseGroupsEmpty) {
  RunGroupsTest(this, &configuration_, {&kChromeTestFeatureFoo},
                {&kChromeTestGroupOne}, "", std::vector<std::string>(),
                false /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseGroupsOnlySeparator) {
  RunGroupsTest(this, &configuration_, {&kChromeTestFeatureFoo},
                {&kChromeTestGroupOne}, ",", std::vector<std::string>(),
                false /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest, ParseGroupsUnknownGroup) {
  RunGroupsTest(this, &configuration_, {&kChromeTestFeatureFoo},
                {&kChromeTestGroupOne}, "random", std::vector<std::string>(),
                false /* is_valid */);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseGroupsCombinationKnownAndUnknownGroups) {
  base::HistogramTester histogram_tester;
  RunGroupsTest(this, &configuration_, {&kChromeTestFeatureFoo},
                {&kChromeTestGroupOne}, "test_group_one,random",
                {kChromeTestGroupOne.name}, true /* is_valid */);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_GROUPS_UNKNOWN_GROUP),
      1);
}

TEST_F(ChromeVariationsConfigurationTest, WhitespaceIsValid) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] =
      " name : eu ; comparator : any ; window : 1 ; storage : 320 ";
  foo_params["event_trigger"] =
      " name:et;comparator : any ;window: 2;storage:330 ";
  foo_params["event_0"] = "name:e0;comparator: <1 ;window:3\n;storage:340";
  foo_params["event_1"] = "name:e1;comparator:  > 2 ;window:4;\rstorage:350";
  foo_params["event_2"] = "name:e2;comparator: <= 3 ;window:5;\tstorage:360";
  foo_params["event_3"] = "name:e3;comparator: <\n4 ;window:6;storage:370";
  foo_params["event_4"] = "name:e4;comparator:  >\t5 ;window:7;storage:380";
  foo_params["event_5"] = "name:e5;comparator: <=\r6 ;window:8;storage:390";
  foo_params["event_6"] = "name:e6;comparator:\n<=7;window:9;storage:400";
  foo_params["event_7"] = "name:e7;comparator:<=8\n;window:10;storage:410";
  foo_params["session_rate_impact"] = " test_bar, test_qux ";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {
      &kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 1, 320);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 2, 330);
  expected_foo.event_configs.insert(
      EventConfig("e0", Comparator(LESS_THAN, 1), 3, 340));
  expected_foo.event_configs.insert(
      EventConfig("e1", Comparator(GREATER_THAN, 2), 4, 350));
  expected_foo.event_configs.insert(
      EventConfig("e2", Comparator(LESS_THAN_OR_EQUAL, 3), 5, 360));
  expected_foo.event_configs.insert(
      EventConfig("e3", Comparator(LESS_THAN, 4), 6, 370));
  expected_foo.event_configs.insert(
      EventConfig("e4", Comparator(GREATER_THAN, 5), 7, 380));
  expected_foo.event_configs.insert(
      EventConfig("e5", Comparator(LESS_THAN_OR_EQUAL, 6), 8, 390));
  expected_foo.event_configs.insert(
      EventConfig("e6", Comparator(LESS_THAN_OR_EQUAL, 7), 9, 400));
  expected_foo.event_configs.insert(
      EventConfig("e7", Comparator(LESS_THAN_OR_EQUAL, 8), 10, 410));
  expected_foo.session_rate_impact = CreateSessionRateImpactExplicit(
      {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name});
  EXPECT_EQ(expected_foo, foo);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  // bar and qux have no configuration.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 2);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 3);
}

TEST_F(ChromeVariationsConfigurationTest, IgnoresInvalidConfigKeys) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["not_there_yet"] = "bogus value";                // Unrecognized.
  foo_params["still_not_there"] = "another bogus value";      // Unrecognized.
  foo_params["x_this_is_ignored"] = "this value is ignored";  // Ignored.
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  EXPECT_EQ(expected_foo, foo);

  // Exactly 2 keys should be unrecognized and not ignored.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_UNKNOWN_KEY), 2);
}

TEST_F(ChromeVariationsConfigurationTest, IgnoresInvalidEventConfigTokens) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] =
      "name:eu;comparator:any;window:0;storage:360;somethingelse:1";
  foo_params["event_trigger"] =
      "yesway:0;noway:1;name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest,
       MissingAllEventConfigParamsIsInvalid) {
  VerifyInvalid("a:1;b:2;c:3;d:4");
}

TEST_F(ChromeVariationsConfigurationTest,
       MissingEventEventConfigParamIsInvalid) {
  VerifyInvalid("foobar:eu;comparator:any;window:1;storage:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       MissingComparatorEventConfigParamIsInvalid) {
  VerifyInvalid("name:eu;foobar:any;window:1;storage:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       MissingWindowEventConfigParamIsInvalid) {
  VerifyInvalid("name:eu;comparator:any;foobar:1;storage:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       MissingStorageEventConfigParamIsInvalid) {
  VerifyInvalid("name:eu;comparator:any;window:1;foobar:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       EventSpecifiedMultipleTimesIsInvalid) {
  VerifyInvalid("name:eu;name:eu;comparator:any;window:1;storage:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       ComparatorSpecifiedMultipleTimesIsInvalid) {
  VerifyInvalid("name:eu;comparator:any;comparator:any;window:1;storage:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       WindowSpecifiedMultipleTimesIsInvalid) {
  VerifyInvalid("name:eu;comparator:any;window:1;window:2;storage:360");
}

TEST_F(ChromeVariationsConfigurationTest,
       StorageSpecifiedMultipleTimesIsInvalid) {
  VerifyInvalid("name:eu;comparator:any;window:1;storage:360;storage:10");
}

TEST_F(ChromeVariationsConfigurationTest,
       InvalidSessionRateCausesInvalidConfig) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:1;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["session_rate"] = "bogus value";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_SESSION_RATE_PARSE),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       InvalidAvailabilityCausesInvalidConfig) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["availability"] = "bogus value";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_AVAILABILITY_PARSE),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

TEST_F(ChromeVariationsConfigurationTest, InvalidUsedCausesInvalidConfig) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "bogus value";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_USED_EVENT_PARSE), 1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_USED_EVENT_MISSING),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 3);
}

TEST_F(ChromeVariationsConfigurationTest, InvalidTriggerCausesInvalidConfig) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "bogus value";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_PARSE),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(
          stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 3);
}

TEST_F(ChromeVariationsConfigurationTest,
       InvalidEventConfigCausesInvalidConfig) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["event_used_0"] = "bogus value";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest,
       InvalidTrackingOnlyCausesInvalidConfig) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["tracking_only"] = "bogus value";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  base::HistogramTester histogram_tester;
  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_FALSE(foo.valid);
  EXPECT_FALSE(foo.tracking_only);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_TRACKING_ONLY_PARSE),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

void RunTrackingOnlyTest(ChromeVariationsConfigurationTest* test,
                         ChromeVariationsConfiguration* configuration,
                         std::vector<const base::Feature*> features,
                         std::string tracking_only_definition,
                         bool expected_value,
                         bool is_valid) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["tracking_only"] = tracking_only_definition;
  test->SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  GroupVector groups;
  configuration->LoadConfigs(Tracker::GetDefaultConfigurationProviders(),
                             features, groups);
  FeatureConfig foo = configuration->GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(is_valid, foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = is_valid;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.tracking_only = expected_value;
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest, TrackingOnlyReturnsTrue) {
  base::HistogramTester histogram_tester;
  RunTrackingOnlyTest(this, &configuration_, {&kChromeTestFeatureFoo}, "true",
                      true /* expected_value */, true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       TrackingOnlyReturnsTrueCaseInsensitive) {
  base::HistogramTester histogram_tester;
  RunTrackingOnlyTest(this, &configuration_, {&kChromeTestFeatureFoo}, "tRUe",
                      true /* expected_value */, true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, TrackingOnlyReturnsFalse) {
  base::HistogramTester histogram_tester;
  RunTrackingOnlyTest(this, &configuration_, {&kChromeTestFeatureFoo}, "false",
                      false /* expected_value */, true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       TrackingOnlyReturnsFalseCaseInsensitive) {
  base::HistogramTester histogram_tester;
  RunTrackingOnlyTest(this, &configuration_, {&kChromeTestFeatureFoo}, "fALSe",
                      false /* expected_value */, true /* is_valid */);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, AllComparatorTypesWork) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:e0;comparator:any;window:20;storage:30";
  foo_params["event_trigger"] = "name:e1;comparator:<1;window:21;storage:31";
  foo_params["event_2"] = "name:e2;comparator:>2;window:22;storage:32";
  foo_params["event_3"] = "name:e3;comparator:<=3;window:23;storage:33";
  foo_params["event_4"] = "name:e4;comparator:>=4;window:24;storage:34";
  foo_params["event_5"] = "name:e5;comparator:==5;window:25;storage:35";
  foo_params["event_6"] = "name:e6;comparator:!=6;window:26;storage:36";
  foo_params["session_rate"] = "!=6";
  foo_params["availability"] = ">=1";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("e0", Comparator(ANY, 0), 20, 30);
  expected_foo.trigger = EventConfig("e1", Comparator(LESS_THAN, 1), 21, 31);
  expected_foo.event_configs.insert(
      EventConfig("e2", Comparator(GREATER_THAN, 2), 22, 32));
  expected_foo.event_configs.insert(
      EventConfig("e3", Comparator(LESS_THAN_OR_EQUAL, 3), 23, 33));
  expected_foo.event_configs.insert(
      EventConfig("e4", Comparator(GREATER_THAN_OR_EQUAL, 4), 24, 34));
  expected_foo.event_configs.insert(
      EventConfig("e5", Comparator(EQUAL, 5), 25, 35));
  expected_foo.event_configs.insert(
      EventConfig("e6", Comparator(NOT_EQUAL, 6), 26, 36));
  expected_foo.session_rate = Comparator(NOT_EQUAL, 6);
  expected_foo.availability = Comparator(GREATER_THAN_OR_EQUAL, 1);
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest, MultipleEventsWithSameName) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:foo;comparator:any;window:20;storage:30";
  foo_params["event_trigger"] = "name:foo;comparator:<1;window:21;storage:31";
  foo_params["event_2"] = "name:foo;comparator:>2;window:22;storage:32";
  foo_params["event_3"] = "name:foo;comparator:<=3;window:23;storage:33";
  foo_params["session_rate"] = "any";
  foo_params["availability"] = ">1";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);

  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("foo", Comparator(ANY, 0), 20, 30);
  expected_foo.trigger = EventConfig("foo", Comparator(LESS_THAN, 1), 21, 31);
  expected_foo.event_configs.insert(
      EventConfig("foo", Comparator(GREATER_THAN, 2), 22, 32));
  expected_foo.event_configs.insert(
      EventConfig("foo", Comparator(LESS_THAN_OR_EQUAL, 3), 23, 33));
  expected_foo.session_rate = Comparator(ANY, 0);
  expected_foo.availability = Comparator(GREATER_THAN, 1);
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest, ParseMultipleFeatures) {
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] =
      "name:foo_used;comparator:any;window:20;storage:30";
  foo_params["event_trigger"] =
      "name:foo_trigger;comparator:<1;window:21;storage:31";
  foo_params["session_rate"] = "==10";
  foo_params["availability"] = "<0";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["event_used"] = "name:bar_used;comparator:ANY;window:0;storage:0";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  std::map<std::string, std::string> qux_params;
  qux_params["event_used"] =
      "name:qux_used;comparator:>=2;window:99;storage:42";
  qux_params["event_trigger"] =
      "name:qux_trigger;comparator:ANY;window:103;storage:104";
  qux_params["event_0"] = "name:q0;comparator:<10;window:20;storage:30";
  qux_params["event_1"] = "name:q1;comparator:>11;window:21;storage:31";
  qux_params["event_2"] = "name:q2;comparator:<=12;window:22;storage:32";
  qux_params["event_3"] = "name:q3;comparator:>=13;window:23;storage:33";
  qux_params["event_4"] = "name:q4;comparator:==14;window:24;storage:34";
  qux_params["event_5"] = "name:q5;comparator:!=15;window:25;storage:35";
  qux_params["session_rate"] = "!=13";
  qux_params["availability"] = "==0";
  SetFeatureParams(kChromeTestFeatureQux, qux_params);

  std::vector<const base::Feature*> features = {
      &kChromeTestFeatureFoo, &kChromeTestFeatureBar, &kChromeTestFeatureQux};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_TRUE(foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("foo_used", Comparator(ANY, 0), 20, 30);
  expected_foo.trigger =
      EventConfig("foo_trigger", Comparator(LESS_THAN, 1), 21, 31);
  expected_foo.session_rate = Comparator(EQUAL, 10);
  expected_foo.availability = Comparator(LESS_THAN, 0);
  EXPECT_EQ(expected_foo, foo);

  FeatureConfig bar = configuration_.GetFeatureConfig(kChromeTestFeatureBar);
  EXPECT_FALSE(bar.valid);

  FeatureConfig qux = configuration_.GetFeatureConfig(kChromeTestFeatureQux);
  EXPECT_TRUE(qux.valid);
  FeatureConfig expected_qux;
  expected_qux.valid = true;
  expected_qux.used =
      EventConfig("qux_used", Comparator(GREATER_THAN_OR_EQUAL, 2), 99, 42);
  expected_qux.trigger =
      EventConfig("qux_trigger", Comparator(ANY, 0), 103, 104);
  expected_qux.event_configs.insert(
      EventConfig("q0", Comparator(LESS_THAN, 10), 20, 30));
  expected_qux.event_configs.insert(
      EventConfig("q1", Comparator(GREATER_THAN, 11), 21, 31));
  expected_qux.event_configs.insert(
      EventConfig("q2", Comparator(LESS_THAN_OR_EQUAL, 12), 22, 32));
  expected_qux.event_configs.insert(
      EventConfig("q3", Comparator(GREATER_THAN_OR_EQUAL, 13), 23, 33));
  expected_qux.event_configs.insert(
      EventConfig("q4", Comparator(EQUAL, 14), 24, 34));
  expected_qux.event_configs.insert(
      EventConfig("q5", Comparator(NOT_EQUAL, 15), 25, 35));
  expected_qux.session_rate = Comparator(NOT_EQUAL, 13);
  expected_qux.availability = Comparator(EQUAL, 0);
  EXPECT_EQ(expected_qux, qux);
}

TEST_F(ChromeVariationsConfigurationTest, CheckedInConfigIsReadable) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatureState(kIPHDummyFeature, true);
  base::HistogramTester histogram_tester;
  FeatureVector features;
  features.push_back(&kIPHDummyFeature);
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig expected;
  expected.valid = true;
  expected.availability = Comparator(LESS_THAN, 0);
  expected.session_rate = Comparator(LESS_THAN, 0);
  expected.trigger =
      EventConfig("dummy_feature_iph_trigger", Comparator(LESS_THAN, 0), 1, 1);
  expected.used =
      EventConfig("dummy_feature_action", Comparator(LESS_THAN, 0), 1, 1);

  const FeatureConfig& config =
      configuration_.GetFeatureConfig(kIPHDummyFeature);
  EXPECT_EQ(expected, config);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS_FROM_SOURCE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       CheckedInConfigButDisabledFeatureIsInvalid) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatureState(kIPHDummyFeature, false);
  base::HistogramTester histogram_tester;
  FeatureVector features;
  features.push_back(&kIPHDummyFeature);
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig expected;
  expected.valid = false;

  const FeatureConfig& config =
      configuration_.GetFeatureConfig(kIPHDummyFeature);
  EXPECT_EQ(expected, config);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, NonExistingConfigIsInvalid) {
  base::HistogramTester histogram_tester;
  FeatureVector features;
  features.push_back(&kChromeTestFeatureFoo);
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig expected;
  expected.valid = false;

  const FeatureConfig& config =
      configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(expected, config);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, ParseValidSnoozeParams) {
  std::map<std::string, std::string> foo_params;
  foo_params["snooze_params"] = "max_limit:5, snooze_interval:3";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.snooze_params.max_limit, 5u);
  EXPECT_EQ(foo.snooze_params.snooze_interval, 3u);
  EXPECT_TRUE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, UnorderedSnoozeParams) {
  std::map<std::string, std::string> foo_params;
  foo_params["snooze_params"] = "snooze_interval:3, max_limit:3";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.snooze_params.max_limit, 3u);
  EXPECT_EQ(foo.snooze_params.snooze_interval, 3u);
  EXPECT_TRUE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, InvalidMaxLimitSnoozeParams) {
  std::map<std::string, std::string> foo_params;
  foo_params["snooze_params"] = "max_limit:e, snooze_interval:3";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.snooze_params.max_limit, 0u);
  EXPECT_EQ(foo.snooze_params.snooze_interval, 0u);
  EXPECT_FALSE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, InvalidIntervalSnoozeParams) {
  std::map<std::string, std::string> foo_params;
  foo_params["snooze_params"] = "max_limit:3, snooze_interval:e";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.snooze_params.max_limit, 0u);
  EXPECT_EQ(foo.snooze_params.snooze_interval, 0u);
  EXPECT_FALSE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest, IncompleteSnoozeParams) {
  std::map<std::string, std::string> foo_params;
  foo_params["snooze_params"] = "max_limit:3";
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::vector<const base::Feature*> features = {&kChromeTestFeatureFoo};
  GroupVector groups;
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(foo.snooze_params.max_limit, 0u);
  EXPECT_EQ(foo.snooze_params.snooze_interval, 0u);
  EXPECT_FALSE(foo.valid);
}

TEST_F(ChromeVariationsConfigurationTest,
       DisabledGroupShouldHaveInvalidConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled_features */, {kChromeTestGroupOne} /* disabled_features */);

  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one_config =
      configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_FALSE(group_one_config.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, ParseSingleGroup) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_trigger"] =
      "name:opened_chrome_home;comparator:any;window:0;storage:360";
  group_one_params["event_1"] =
      "name:user_has_seen_dino;comparator:>=1;window:120;storage:180";
  group_one_params["event_2"] =
      "name:user_opened_app_menu;comparator:<=0;window:120;storage:180";
  group_one_params["event_3"] =
      "name:user_opened_downloads_home;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_TRUE(group_one.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);

  GroupConfig expected_group_one;
  expected_group_one.valid = true;
  expected_group_one.trigger =
      EventConfig("opened_chrome_home", Comparator(ANY, 0), 0, 360);
  expected_group_one.event_configs.insert(EventConfig(
      "user_has_seen_dino", Comparator(GREATER_THAN_OR_EQUAL, 1), 120, 180));
  expected_group_one.event_configs.insert(EventConfig(
      "user_opened_app_menu", Comparator(LESS_THAN_OR_EQUAL, 0), 120, 180));
  expected_group_one.event_configs.insert(
      EventConfig("user_opened_downloads_home", Comparator(ANY, 0), 0, 360));
  EXPECT_EQ(expected_group_one, group_one);
}

TEST_F(ChromeVariationsConfigurationTest, EmptyGroupIsInvalid) {
  std::map<std::string, std::string> group_one_params;
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_FALSE(group_one.valid);

  GroupConfig expected_group_one;
  expected_group_one.valid = false;
  EXPECT_EQ(expected_group_one, group_one);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, GroupOnlyTriggerIsValid) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_trigger"] =
      "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_TRUE(group_one.valid);

  GroupConfig expected_group_one;
  expected_group_one.valid = true;
  expected_group_one.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  EXPECT_EQ(expected_group_one, group_one);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, GroupIgnoresInvalidConfigKeys) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_trigger"] =
      "name:et;comparator:any;window:0;storage:360";
  group_one_params["not_there_yet"] = "bogus value";            // Unrecognized.
  group_one_params["still_not_there"] = "another bogus value";  // Unrecognized.
  group_one_params["x_this_is_ignored"] = "this value is ignored";  // Ignored.
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_TRUE(group_one.valid);

  GroupConfig expected_group_one;
  expected_group_one.valid = true;
  expected_group_one.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  EXPECT_EQ(expected_group_one, group_one);

  // Exactly 2 keys should be unrecognized and not ignored.
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_UNKNOWN_KEY), 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       GroupInvalidSessionRateCausesInvalidConfig) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_used"] =
      "name:eu;comparator:any;window:1;storage:360";
  group_one_params["event_trigger"] =
      "name:et;comparator:any;window:0;storage:360";
  group_one_params["session_rate"] = "bogus value";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_FALSE(group_one.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_SESSION_RATE_PARSE),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       GroupInvalidTriggerCausesInvalidConfig) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_1"] = "name:eu;comparator:any;window:0;storage:360";
  group_one_params["event_trigger"] = "bogus value";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_FALSE(group_one.valid);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_PARSE),
      1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE), 1);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(
          stats::ConfigParsingEvent::FAILURE_TRIGGER_EVENT_MISSING),
      1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 3);
}

TEST_F(ChromeVariationsConfigurationTest,
       GroupInvalidEventConfigCausesInvalidConfig) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_used"] =
      "name:eu;comparator:any;window:0;storage:360";
  group_one_params["event_trigger"] =
      "name:et;comparator:any;window:0;storage:360";
  group_one_params["event_used_0"] = "bogus value";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_FALSE(group_one.valid);
}

TEST_F(ChromeVariationsConfigurationTest, GroupMultipleEventsWithSameName) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_trigger"] =
      "name:foo;comparator:<1;window:21;storage:31";
  group_one_params["event_2"] = "name:foo;comparator:>2;window:22;storage:32";
  group_one_params["event_3"] = "name:foo;comparator:<=3;window:23;storage:33";
  group_one_params["session_rate"] = "any";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_TRUE(group_one.valid);

  GroupConfig expected_group_one;
  expected_group_one.valid = true;
  expected_group_one.trigger =
      EventConfig("foo", Comparator(LESS_THAN, 1), 21, 31);
  expected_group_one.event_configs.insert(
      EventConfig("foo", Comparator(GREATER_THAN, 2), 22, 32));
  expected_group_one.event_configs.insert(
      EventConfig("foo", Comparator(LESS_THAN_OR_EQUAL, 3), 23, 33));
  expected_group_one.session_rate = Comparator(ANY, 0);
  EXPECT_EQ(expected_group_one, group_one);
}

TEST_F(ChromeVariationsConfigurationTest, ParseMultipleGroups) {
  std::map<std::string, std::string> group_one_params;
  group_one_params["event_trigger"] =
      "name:foo_trigger;comparator:<1;window:21;storage:31";
  group_one_params["session_rate"] = "==10";
  SetFeatureParams(kChromeTestGroupOne, group_one_params);

  std::map<std::string, std::string> group_two_params;
  group_two_params["event_trigger"] = "bogus value";
  SetFeatureParams(kChromeTestGroupTwo, group_two_params);

  std::map<std::string, std::string> group_three_params;
  group_three_params["event_trigger"] =
      "name:qux_trigger;comparator:ANY;window:103;storage:104";
  group_three_params["event_0"] = "name:q0;comparator:<10;window:20;storage:30";
  group_three_params["event_1"] = "name:q1;comparator:>11;window:21;storage:31";
  group_three_params["event_2"] =
      "name:q2;comparator:<=12;window:22;storage:32";
  group_three_params["event_3"] =
      "name:q3;comparator:>=13;window:23;storage:33";
  group_three_params["event_4"] =
      "name:q4;comparator:==14;window:24;storage:34";
  group_three_params["event_5"] =
      "name:q5;comparator:!=15;window:25;storage:35";
  group_three_params["session_rate"] = "!=13";
  SetFeatureParams(kChromeTestGroupThree, group_three_params);

  FeatureVector features;
  GroupVector groups = {&kChromeTestGroupOne, &kChromeTestGroupTwo,
                        &kChromeTestGroupThree};
  LoadConfigs(features, groups);

  GroupConfig group_one = configuration_.GetGroupConfig(kChromeTestGroupOne);
  EXPECT_TRUE(group_one.valid);
  GroupConfig expected_group_one;
  expected_group_one.valid = true;
  expected_group_one.trigger =
      EventConfig("foo_trigger", Comparator(LESS_THAN, 1), 21, 31);
  expected_group_one.session_rate = Comparator(EQUAL, 10);
  EXPECT_EQ(expected_group_one, group_one);

  GroupConfig group_two = configuration_.GetGroupConfig(kChromeTestGroupTwo);
  EXPECT_FALSE(group_two.valid);

  GroupConfig group_three =
      configuration_.GetGroupConfig(kChromeTestGroupThree);
  EXPECT_TRUE(group_three.valid);
  GroupConfig expected_group_three;
  expected_group_three.valid = true;
  expected_group_three.trigger =
      EventConfig("qux_trigger", Comparator(ANY, 0), 103, 104);
  expected_group_three.event_configs.insert(
      EventConfig("q0", Comparator(LESS_THAN, 10), 20, 30));
  expected_group_three.event_configs.insert(
      EventConfig("q1", Comparator(GREATER_THAN, 11), 21, 31));
  expected_group_three.event_configs.insert(
      EventConfig("q2", Comparator(LESS_THAN_OR_EQUAL, 12), 22, 32));
  expected_group_three.event_configs.insert(
      EventConfig("q3", Comparator(GREATER_THAN_OR_EQUAL, 13), 23, 33));
  expected_group_three.event_configs.insert(
      EventConfig("q4", Comparator(EQUAL, 14), 24, 34));
  expected_group_three.event_configs.insert(
      EventConfig("q5", Comparator(NOT_EQUAL, 15), 25, 35));
  expected_group_three.session_rate = Comparator(NOT_EQUAL, 13);
  EXPECT_EQ(expected_group_three, group_three);
}

TEST_F(ChromeVariationsConfigurationTest, CheckedInGroupConfigIsReadable) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitWithFeatureState(kIPHDummyGroup, true);
  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kIPHDummyGroup};
  LoadConfigs(features, groups);

  GroupConfig expected;
  expected.valid = true;
  expected.session_rate = Comparator(LESS_THAN, 0);
  expected.trigger =
      EventConfig("dummy_group_iph_trigger", Comparator(LESS_THAN, 0), 1, 1);

  const GroupConfig& config = configuration_.GetGroupConfig(kIPHDummyGroup);
  EXPECT_EQ(expected, config);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS_FROM_SOURCE), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       CheckedInGroupConfigButDisabledGroupIsInvalid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled_features */, {kIPHDummyGroup} /* disabled_features */);
  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kIPHDummyGroup};
  LoadConfigs(features, groups);

  GroupConfig expected;
  expected.valid = false;

  const GroupConfig& config = configuration_.GetGroupConfig(kIPHDummyGroup);
  EXPECT_EQ(expected, config);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest, NonExistingGroupConfigIsInvalid) {
  base::HistogramTester histogram_tester;
  FeatureVector features;
  GroupVector groups = {&kIPHDummyGroup};
  LoadConfigs(features, groups);

  GroupConfig expected;
  expected.valid = false;

  const GroupConfig& config = configuration_.GetGroupConfig(kIPHDummyGroup);
  EXPECT_EQ(expected, config);
  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::FAILURE_NO_FIELD_TRIAL), 1);
  histogram_tester.ExpectTotalCount(kConfigParseEventName, 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseExpandsBlockedByGroupNameIntoFeatures) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["blocked_by"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  bar_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  bar_params["groups"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  FeatureVector features = {&kChromeTestFeatureFoo, &kChromeTestFeatureBar};
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.blocked_by =
      CreateBlockedByExplicit({kChromeTestFeatureBar.name});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseExpandsBlockedByEmptyGroupIntoEmptyList) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["blocked_by"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  FeatureVector features = {&kChromeTestFeatureFoo};
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.blocked_by = CreateBlockedByExplicit({});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseExpandsBlockedByGroupAndFeature) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["blocked_by"] = "test_group_one,test_qux";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  bar_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  bar_params["groups"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  std::map<std::string, std::string> qux_params;
  qux_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  qux_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  SetFeatureParams(kChromeTestFeatureQux, qux_params);

  FeatureVector features = {&kChromeTestFeatureFoo, &kChromeTestFeatureBar,
                            &kChromeTestFeatureQux};
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.blocked_by = CreateBlockedByExplicit(
      {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 3);
}

TEST_F(ChromeVariationsConfigurationTest, ParseExpandsBlockedByMultipleGroups) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["blocked_by"] = "test_group_one,test_group_two";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  bar_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  bar_params["groups"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  std::map<std::string, std::string> qux_params;
  qux_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  qux_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  qux_params["groups"] = "test_group_two";
  SetFeatureParams(kChromeTestFeatureQux, qux_params);

  FeatureVector features = {&kChromeTestFeatureFoo, &kChromeTestFeatureBar,
                            &kChromeTestFeatureQux};
  GroupVector groups = {&kChromeTestGroupOne, &kChromeTestGroupTwo};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.blocked_by = CreateBlockedByExplicit(
      {kChromeTestFeatureBar.name, kChromeTestFeatureQux.name});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 3);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseExpandsBlockedByFeatureAppearsMultipleTimes) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["blocked_by"] = "test_bar,test_group_one,test_group_two";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  bar_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  bar_params["groups"] = "test_group_one,test_group_two";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  FeatureVector features = {&kChromeTestFeatureFoo, &kChromeTestFeatureBar};
  GroupVector groups = {&kChromeTestGroupOne, &kChromeTestGroupTwo};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  // Even though test_bar is in the original foo blocked_by and is in group one
  // and group two, it should only show up in the flattened list once.
  EXPECT_EQ(1u, foo.blocked_by.affected_features->size());
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.blocked_by =
      CreateBlockedByExplicit({kChromeTestFeatureBar.name});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseExpandsSessionRateImpactGroupNameIntoFeatures) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["session_rate_impact"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  std::map<std::string, std::string> bar_params;
  bar_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  bar_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  bar_params["groups"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureBar, bar_params);

  FeatureVector features = {&kChromeTestFeatureFoo, &kChromeTestFeatureBar};
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.session_rate_impact =
      CreateSessionRateImpactExplicit({kChromeTestFeatureBar.name});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 2);
}

TEST_F(ChromeVariationsConfigurationTest,
       ParseExpandsSessionRateEmptyGroupIntoEmptyList) {
  base::HistogramTester histogram_tester;
  std::map<std::string, std::string> foo_params;
  foo_params["event_used"] = "name:eu;comparator:any;window:0;storage:360";
  foo_params["event_trigger"] = "name:et;comparator:any;window:0;storage:360";
  foo_params["session_rate_impact"] = "test_group_one";
  SetFeatureParams(kChromeTestFeatureFoo, foo_params);

  FeatureVector features = {&kChromeTestFeatureFoo};
  GroupVector groups = {&kChromeTestGroupOne};
  LoadConfigs(features, groups);

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);

  EXPECT_EQ(true, foo.valid);
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used = EventConfig("eu", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger = EventConfig("et", Comparator(ANY, 0), 0, 360);
  expected_foo.session_rate_impact = CreateSessionRateImpactExplicit({});
  EXPECT_EQ(expected_foo, foo);

  histogram_tester.ExpectBucketCount(
      kConfigParseEventName,
      static_cast<int>(stats::ConfigParsingEvent::SUCCESS), 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeVariationsConfigurationTest, UpdateConfigs) {
  FeatureConfig expected_foo;
  expected_foo.valid = true;
  expected_foo.used =
      EventConfig("page_download_started", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger =
      EventConfig("opened_chrome_home", Comparator(ANY, 0), 0, 360);
  expected_foo.event_configs.insert(EventConfig(
      "user_has_seen_dino", Comparator(GREATER_THAN_OR_EQUAL, 1), 120, 180));
  expected_foo.event_configs.insert(EventConfig(
      "user_opened_app_menu", Comparator(LESS_THAN_OR_EQUAL, 0), 120, 180));
  expected_foo.event_configs.insert(
      EventConfig("user_opened_downloads_home", Comparator(ANY, 0), 0, 360));

  auto provider = std::make_unique<TestConfigurationProvider>();
  provider->SetConfig(expected_foo);
  configuration_.UpdateConfig(kChromeTestFeatureFoo, provider.get());

  FeatureConfig foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(expected_foo, foo);

  // Update config, removing `event_1` and `event_3`.
  expected_foo = FeatureConfig();
  expected_foo.valid = true;
  expected_foo.used =
      EventConfig("page_download_started", Comparator(ANY, 0), 0, 360);
  expected_foo.trigger =
      EventConfig("opened_chrome_home", Comparator(ANY, 0), 0, 360);
  expected_foo.event_configs.insert(EventConfig(
      "user_opened_app_menu", Comparator(LESS_THAN_OR_EQUAL, 0), 120, 180));
  EXPECT_NE(expected_foo, foo);

  provider->SetConfig(expected_foo);
  configuration_.UpdateConfig(kChromeTestFeatureFoo, provider.get());

  foo = configuration_.GetFeatureConfig(kChromeTestFeatureFoo);
  EXPECT_EQ(expected_foo, foo);
}

TEST_F(ChromeVariationsConfigurationTest, GetEventPrefixes) {
  ConfigurationProviderList providers;
  providers.emplace_back(std::make_unique<TestConfigurationProvider>());
  configuration_.LoadConfigs(providers, /*features=*/{&kChromeTestFeatureFoo},
                             /*groups=*/{});

  const auto& prefixes = configuration_.GetRegisteredAllowedEventPrefixes();
  EXPECT_EQ(1u, prefixes.size());
  EXPECT_TRUE(prefixes.contains("TestEventPrefix"));
}
#endif

}  // namespace feature_engagement
