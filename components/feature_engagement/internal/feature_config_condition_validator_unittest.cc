// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/feature_config_condition_validator.h"

#include <map>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/internal/availability_model.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/noop_display_lock_controller.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const base::Feature kFeatureConfigTestFeatureFoo{
    "test_foo", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFeatureConfigTestFeatureBar{
    "test_bar", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFeatureConfigTestFeatureQux{
    "test_qux", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFeatureConfigTestFeatureXyz{
    "test_xyz", base::FEATURE_DISABLED_BY_DEFAULT};

FeatureConfig GetValidFeatureConfig() {
  FeatureConfig config;
  config.valid = true;
  return config;
}

FeatureConfig GetAcceptingFeatureConfig() {
  FeatureConfig config;
  config.valid = true;
  config.used = EventConfig("used", Comparator(ANY, 0), 0, 0);
  config.trigger = EventConfig("trigger", Comparator(ANY, 0), 0, 0);
  config.session_rate = Comparator(ANY, 0);
  config.availability = Comparator(ANY, 0);
  return config;
}

SessionRateImpact CreateSessionRateImpactTypeExplicit(
    std::vector<std::string> affected_features) {
  SessionRateImpact impact;
  impact.type = SessionRateImpact::Type::EXPLICIT;
  impact.affected_features = affected_features;
  return impact;
}

class TestEventModel : public EventModel {
 public:
  TestEventModel() : ready_(true) {}

  void Initialize(const OnModelInitializationFinished& callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  const Event* GetEvent(const std::string& event_name) const override {
    auto search = events_.find(event_name);
    if (search == events_.end())
      return nullptr;

    return &search->second;
  }

  void SetEvent(const Event& event) { events_[event.name()] = event; }

  void IncrementEvent(const std::string& event_name, uint32_t day) override {}

 private:
  std::map<std::string, Event> events_;
  bool ready_;
};

class TestAvailabilityModel : public AvailabilityModel {
 public:
  TestAvailabilityModel() : ready_(true) {}
  ~TestAvailabilityModel() override = default;

  void Initialize(AvailabilityModel::OnInitializedCallback callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  base::Optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override {
    auto search = availabilities_.find(feature.name);
    if (search == availabilities_.end())
      return base::nullopt;

    return search->second;
  }

  void SetAvailability(const base::Feature* feature,
                       base::Optional<uint32_t> availability) {
    availabilities_[feature->name] = availability;
  }

 private:
  bool ready_;

  std::map<std::string, base::Optional<uint32_t>> availabilities_;

  DISALLOW_COPY_AND_ASSIGN(TestAvailabilityModel);
};

class TestDisplayLockController : public DisplayLockController {
 public:
  TestDisplayLockController() = default;
  ~TestDisplayLockController() override = default;

  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override {
    return nullptr;
  }

  bool IsDisplayLocked() const override { return next_display_locked_result_; }

  void SetNextIsDisplayLockedResult(bool result) {
    next_display_locked_result_ = result;
  }

 private:
  // The next result to return from IsDisplayLocked().
  bool next_display_locked_result_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayLockController);
};

class FeatureConfigConditionValidatorTest : public ::testing::Test {
 public:
  FeatureConfigConditionValidatorTest() = default;

 protected:
  ConditionValidator::Result GetResultForDayAndEventWindow(
      Comparator comparator,
      uint32_t window,
      uint32_t current_day) {
    FeatureConfig config = GetAcceptingFeatureConfig();
    config.event_configs.insert(EventConfig("event1", comparator, window, 0));
    return validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  }

  ConditionValidator::Result GetResultForDay(const FeatureConfig& config,
                                             uint32_t current_day) {
    return validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  }

  ConditionValidator::Result GetResultForDayZero(const FeatureConfig& config) {
    return validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, 0);
  }

  ConditionValidator::Result GetResultForDayZeroForFeature(
      const base::Feature& feature,
      const FeatureConfig& config) {
    return validator_.MeetsConditions(feature, config, event_model_,
                                      availability_model_,
                                      display_lock_controller_, 0);
  }

  TestEventModel event_model_;
  TestAvailabilityModel availability_model_;
  TestDisplayLockController display_lock_controller_;
  FeatureConfigConditionValidator validator_;
  uint32_t current_day_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeatureConfigConditionValidatorTest);
};

}  // namespace

TEST_F(FeatureConfigConditionValidatorTest, ModelNotReadyShouldFail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  event_model_.SetIsReady(false);

  ConditionValidator::Result result =
      GetResultForDayZero(GetValidFeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.event_model_ready_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, ConfigInvalidShouldFail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  ConditionValidator::Result result = GetResultForDayZero(FeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.config_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, MultipleErrorsShouldBeSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  event_model_.SetIsReady(false);

  ConditionValidator::Result result = GetResultForDayZero(FeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.event_model_ready_ok);
  EXPECT_FALSE(result.config_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, ReadyModelEmptyConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  EXPECT_TRUE(GetResultForDayZero(GetValidFeatureConfig()).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, ReadyModelAcceptingConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  EXPECT_TRUE(GetResultForDayZero(GetAcceptingFeatureConfig()).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, CurrentlyShowing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});

  validator_.NotifyIsShowing(
      kFeatureConfigTestFeatureBar, FeatureConfig(),
      {kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name});
  ConditionValidator::Result result =
      GetResultForDayZero(GetAcceptingFeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.currently_showing_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, Used) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.used = EventConfig("used", Comparator(LESS_THAN, 0), 0, 0);

  ConditionValidator::Result result = GetResultForDayZero(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.used_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, Trigger) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.trigger = EventConfig("trigger", Comparator(LESS_THAN, 0), 0, 0);

  ConditionValidator::Result result = GetResultForDayZero(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.trigger_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, SingleOKPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResultForDayZero(config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, MultipleOKPreconditions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));
  config.event_configs.insert(EventConfig("event2", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResultForDayZero(config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, OneOKThenOneFailingPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result = GetResultForDayZero(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, OneFailingThenOneOKPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result = GetResultForDayZero(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, TwoFailingPreconditions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(LESS_THAN, 0), 0, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result = GetResultForDayZero(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, SessionRate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  EXPECT_TRUE(GetResultForDayZero(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultForDayZero(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result = GetResultForDayZero(foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  result = GetResultForDayZero(foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, SessionRateImpactAffectsNone) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig affects_none_config = GetAcceptingFeatureConfig();
  affects_none_config.session_rate_impact = SessionRateImpact();
  affects_none_config.session_rate_impact.type = SessionRateImpact::Type::NONE;

  EXPECT_TRUE(GetResultForDayZero(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, affects_none_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultForDayZero(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, affects_none_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultForDayZero(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, affects_none_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultForDayZero(foo_config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, SessionRateImpactAffectsExplicit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar,
       kFeatureConfigTestFeatureQux},
      {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name,
      kFeatureConfigTestFeatureQux.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig bar_config = GetAcceptingFeatureConfig();
  bar_config.session_rate = Comparator(LESS_THAN, 2u);

  FeatureConfig affects_only_foo_config = GetAcceptingFeatureConfig();
  affects_only_foo_config.session_rate_impact =
      CreateSessionRateImpactTypeExplicit({kFeatureConfigTestFeatureFoo.name});

  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  ConditionValidator::Result result =
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, SessionRateImpactAffectsSelf) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar,
       kFeatureConfigTestFeatureQux},
      {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig bar_config = GetAcceptingFeatureConfig();
  bar_config.session_rate = Comparator(LESS_THAN, 2u);

  FeatureConfig affects_only_foo_config = GetAcceptingFeatureConfig();
  affects_only_foo_config.session_rate_impact =
      CreateSessionRateImpactTypeExplicit({kFeatureConfigTestFeatureFoo.name});

  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureFoo,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureFoo);
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureFoo,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureFoo);
  ConditionValidator::Result result =
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest,
       SessionRateImpactAffectsExplicitMultipleFeatures) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar,
       kFeatureConfigTestFeatureQux, kFeatureConfigTestFeatureXyz},
      {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name,
      kFeatureConfigTestFeatureQux.name, kFeatureConfigTestFeatureXyz.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig bar_config = GetAcceptingFeatureConfig();
  bar_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig xyz_config = GetAcceptingFeatureConfig();
  xyz_config.session_rate = Comparator(LESS_THAN, 2u);

  FeatureConfig affects_foo_and_bar_config = GetAcceptingFeatureConfig();
  affects_foo_and_bar_config.session_rate_impact =
      CreateSessionRateImpactTypeExplicit({kFeatureConfigTestFeatureFoo.name,
                                           kFeatureConfigTestFeatureBar.name});

  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureXyz, xyz_config)
          .NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_foo_and_bar_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureBar, bar_config)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureXyz, xyz_config)
          .NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_foo_and_bar_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  ConditionValidator::Result foo_result =
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, foo_config);
  EXPECT_FALSE(foo_result.NoErrors());
  EXPECT_FALSE(foo_result.session_rate_ok);
  ConditionValidator::Result bar_result =
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureFoo, bar_config);
  EXPECT_FALSE(bar_result.NoErrors());
  EXPECT_FALSE(bar_result.session_rate_ok);
  EXPECT_TRUE(
      GetResultForDayZeroForFeature(kFeatureConfigTestFeatureXyz, xyz_config)
          .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, Availability) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  EXPECT_TRUE(GetResultForDayZero(config).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 100u).NoErrors());

  // When the AvailabilityModel is not ready, it should fail.
  availability_model_.SetIsReady(false);
  ConditionValidator::Result result = GetResultForDayZero(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_model_ready_ok);
  result = GetResultForDay(config, 100u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_model_ready_ok);

  // Reset state back to ready.
  availability_model_.SetIsReady(true);

  // For a feature that became available on day 2 that has to have been
  // available for at least 1 day, it should start being accepted on day 3.
  availability_model_.SetAvailability(&kFeatureConfigTestFeatureFoo, 2u);
  config.availability = Comparator(GREATER_THAN_OR_EQUAL, 1u);
  result = GetResultForDay(config, 1u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  result = GetResultForDay(config, 2u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  EXPECT_TRUE(GetResultForDay(config, 3u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 4u).NoErrors());

  // For a feature that became available on day 10 that has to have been
  // available for at least 3 days, it should start being accepted on day 13.
  availability_model_.SetAvailability(&kFeatureConfigTestFeatureFoo, 10u);
  config.availability = Comparator(GREATER_THAN_OR_EQUAL, 3u);
  result = GetResultForDay(config, 11u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  result = GetResultForDay(config, 12u);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  EXPECT_TRUE(GetResultForDay(config, 13u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 14u).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, SingleEventChangingComparator) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  uint32_t current_day = 102u;
  uint32_t window = 10u;

  // Create event with 10 events per day for three days.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 100u, 10u);
  test::SetEventCountForDay(&event1, 101u, 10u);
  test::SetEventCountForDay(&event1, 102u, 10u);
  event_model_.SetEvent(event1);

  EXPECT_TRUE(GetResultForDayAndEventWindow(Comparator(LESS_THAN, 50u), window,
                                            current_day)
                  .NoErrors());
  EXPECT_TRUE(
      GetResultForDayAndEventWindow(Comparator(EQUAL, 30u), window, current_day)
          .NoErrors());
  EXPECT_FALSE(GetResultForDayAndEventWindow(Comparator(LESS_THAN, 30u), window,
                                             current_day)
                   .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, SingleEventChangingWindow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 100u, 10u);
  test::SetEventCountForDay(&event1, 101u, 10u);
  test::SetEventCountForDay(&event1, 102u, 10u);
  test::SetEventCountForDay(&event1, 103u, 10u);
  test::SetEventCountForDay(&event1, 104u, 10u);
  event_model_.SetEvent(event1);

  uint32_t current_day = 104u;

  EXPECT_FALSE(GetResultForDayAndEventWindow(Comparator(GREATER_THAN, 30u), 0,
                                             current_day)
                   .NoErrors());
  EXPECT_FALSE(GetResultForDayAndEventWindow(Comparator(GREATER_THAN, 30u), 1u,
                                             current_day)
                   .NoErrors());
  EXPECT_FALSE(GetResultForDayAndEventWindow(Comparator(GREATER_THAN, 30u), 2u,
                                             current_day)
                   .NoErrors());
  EXPECT_FALSE(GetResultForDayAndEventWindow(Comparator(GREATER_THAN, 30u), 3u,
                                             current_day)
                   .NoErrors());
  EXPECT_TRUE(GetResultForDayAndEventWindow(Comparator(GREATER_THAN, 30u), 4u,
                                            current_day)
                  .NoErrors());
  EXPECT_TRUE(GetResultForDayAndEventWindow(Comparator(GREATER_THAN, 30u), 5u,
                                            current_day)
                  .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, CapEarliestAcceptedDayAtEpoch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 0, 10u);
  test::SetEventCountForDay(&event1, 1u, 10u);
  test::SetEventCountForDay(&event1, 2u, 10u);
  event_model_.SetEvent(event1);

  uint32_t current_day = 100u;

  EXPECT_TRUE(
      GetResultForDayAndEventWindow(Comparator(EQUAL, 10u), 99u, current_day)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayAndEventWindow(Comparator(EQUAL, 20u), 100u, current_day)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayAndEventWindow(Comparator(EQUAL, 30u), 101u, current_day)
          .NoErrors());
  EXPECT_TRUE(
      GetResultForDayAndEventWindow(Comparator(EQUAL, 30u), 1000u, current_day)
          .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, TestMultipleEvents) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 0, 10u);
  test::SetEventCountForDay(&event1, 1u, 10u);
  test::SetEventCountForDay(&event1, 2u, 10u);
  event_model_.SetEvent(event1);

  Event event2;
  event2.set_name("event2");
  test::SetEventCountForDay(&event2, 0, 5u);
  test::SetEventCountForDay(&event2, 1u, 5u);
  test::SetEventCountForDay(&event2, 2u, 5u);
  event_model_.SetEvent(event2);

  uint32_t current_day = 100u;

  // Verify validator counts correctly for two events last 99 days.
  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 10u), 99u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 5u), 99u, 0));
  ConditionValidator::Result result = validator_.MeetsConditions(
      kFeatureConfigTestFeatureFoo, config, event_model_, availability_model_,
      display_lock_controller_, current_day);
  EXPECT_TRUE(result.NoErrors());

  // Verify validator counts correctly for two events last 100 days.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 20u), 100u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 10u), 100u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  EXPECT_TRUE(result.NoErrors());

  // Verify validator counts correctly for two events last 101 days.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 30u), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 15u), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  EXPECT_TRUE(result.NoErrors());

  // Verify validator counts correctly for two events last 101 days, and returns
  // error when first event fails.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 0), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 15u), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);

  // Verify validator counts correctly for two events last 101 days, and returns
  // error when second event fails.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 30u), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 0), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);

  // Verify validator counts correctly for two events last 101 days, and returns
  // error when both events fail.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 0), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 0), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config,
                                      event_model_, availability_model_,
                                      display_lock_controller_, current_day);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, TestStaggeredTriggering) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  // Trigger maximum 2 times, and only 1 time last 2 days (today + yesterday).
  FeatureConfig config;
  config.valid = true;
  config.used = EventConfig("used", Comparator(ANY, 0), 0, 0);
  config.trigger = EventConfig("trigger", Comparator(LESS_THAN, 2), 100u, 100u);
  config.session_rate = Comparator(ANY, 0);
  config.availability = Comparator(ANY, 0);
  config.event_configs.insert(
      EventConfig("trigger", Comparator(LESS_THAN, 1u), 2u, 100u));

  // Should be OK to trigger initially on day 0.
  EXPECT_TRUE(GetResultForDay(config, 0u).NoErrors());

  // Set that we triggered on day 0. We should then only trigger on day 2+.
  Event trigger_event;
  trigger_event.set_name("trigger");
  test::SetEventCountForDay(&trigger_event, 0u, 1u);
  event_model_.SetEvent(trigger_event);
  EXPECT_FALSE(GetResultForDay(config, 0u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 1u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 2u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 3u).NoErrors());

  // Set that we triggered again on day 2. We should then not trigger again
  // until max storage time has passed (100 days), which would expire the
  // trigger from day 0.
  test::SetEventCountForDay(&trigger_event, 2u, 1u);
  event_model_.SetEvent(trigger_event);
  EXPECT_FALSE(GetResultForDay(config, 2u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 3u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 4u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 5u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 99u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 100u).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, TestMultipleEventsWithSameName) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  // Trigger maximum 2 times, and only 1 time last 2 days (today + yesterday).
  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(LESS_THAN, 1u), 2u, 100u));
  config.event_configs.insert(
      EventConfig("event1", Comparator(LESS_THAN, 2u), 100u, 100u));

  // Should be OK to trigger initially on day 0.
  EXPECT_TRUE(GetResultForDay(config, 0u).NoErrors());

  // Set that we had event1 on day 0. We should then only trigger on day 2+.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 0u, 1u);
  event_model_.SetEvent(event1);
  EXPECT_FALSE(GetResultForDay(config, 0u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 1u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 2u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 3u).NoErrors());

  // Set that we had event1 again on day 2. We should then not trigger again
  // until max storage time has passed (100 days), which would expire the
  // trigger from day 0.
  test::SetEventCountForDay(&event1, 2u, 1u);
  event_model_.SetEvent(event1);
  EXPECT_FALSE(GetResultForDay(config, 2u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 3u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 4u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 5u).NoErrors());
  EXPECT_FALSE(GetResultForDay(config, 99u).NoErrors());
  EXPECT_TRUE(GetResultForDay(config, 100u).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, DisplayLockedStatus) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  // When the display is locked, the result should be negative.
  display_lock_controller_.SetNextIsDisplayLockedResult(true);

  ConditionValidator::Result result =
      GetResultForDayZero(GetAcceptingFeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.display_lock_ok);

  // Setting the display to unlocked should make the result positive.
  display_lock_controller_.SetNextIsDisplayLockedResult(false);

  EXPECT_TRUE(GetResultForDayZero(GetAcceptingFeatureConfig()).NoErrors());
}

}  // namespace feature_engagement
