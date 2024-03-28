// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/feature_config_condition_validator.h"

#include <map>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/internal/availability_model.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/noop_display_lock_controller.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "components/feature_engagement/internal/test/test_time_provider.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

BASE_FEATURE(kFeatureConfigTestFeatureFoo,
             "test_foo",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureConfigTestFeatureBar,
             "test_bar",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureConfigTestFeatureQux,
             "test_qux",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeatureConfigTestFeatureXyz,
             "test_xyz",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

FeatureConfig GetNonBlockingFeatureConfig() {
  FeatureConfig config;
  config.valid = true;
  config.blocked_by.type = BlockedBy::Type::NONE;
  config.blocking.type = Blocking::Type::NONE;
  return config;
}

GroupConfig GetValidGroupConfig() {
  GroupConfig config;
  config.valid = true;
  return config;
}

GroupConfig GetAcceptingGroupConfig() {
  GroupConfig config;
  config.valid = true;
  config.trigger = EventConfig("trigger", Comparator(ANY, 0), 0, 0);
  return config;
}

SessionRateImpact CreateSessionRateImpactTypeExplicit(
    std::vector<std::string> affected_features) {
  SessionRateImpact impact;
  impact.type = SessionRateImpact::Type::EXPLICIT;
  impact.affected_features = affected_features;
  return impact;
}

class TestConfiguration : public Configuration {
 public:
  TestConfiguration() {
    config_ = GetValidFeatureConfig();
    group_config_ = GetValidGroupConfig();
  }
  ~TestConfiguration() override = default;

  // Configuration implementation.
  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override {
    return config_;
  }
  const FeatureConfig& GetFeatureConfigByName(
      const std::string& feature_name) const override {
    return config_;
  }
  const Configuration::ConfigMap& GetRegisteredFeatureConfigs() const override {
    return map_;
  }
  const std::vector<std::string> GetRegisteredFeatures() const override {
    return std::vector<std::string>();
  }
  const GroupConfig& GetGroupConfig(const base::Feature& group) const override {
    return group_config_;
  }
  const GroupConfig& GetGroupConfigByName(
      const std::string& group_name) const override {
    return group_config_;
  }
  const Configuration::GroupConfigMap& GetRegisteredGroupConfigs()
      const override {
    return group_map_;
  }
  const std::vector<std::string> GetRegisteredGroups() const override {
    return std::vector<std::string>();
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void UpdateConfig(const base::Feature& feature,
                    const ConfigurationProvider* provider) override {}
  const EventPrefixSet& GetRegisteredAllowedEventPrefixes() const override {
    return event_prefixes_;
  }
#endif

 private:
  FeatureConfig config_;
  GroupConfig group_config_;
  Configuration::ConfigMap map_;
  Configuration::GroupConfigMap group_map_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Configuration::EventPrefixSet event_prefixes_;
#endif
};

class TestEventModel : public EventModel {
 public:
  TestEventModel() = default;

  void Initialize(OnModelInitializationFinished callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  const Event* GetEvent(const std::string& event_name) const override {
    auto search = events_.find(event_name);
    if (search == events_.end())
      return nullptr;

    return &search->second;
  }

  uint32_t GetEventCount(const std::string& event_name,
                         uint32_t current_day,
                         uint32_t window_size) const override {
    // A same implementation for EventModelImpl.
    const Event* event = GetEvent(event_name);
    if (event == nullptr || window_size == 0u)
      return 0;

    DCHECK(window_size >= 0);

    uint32_t oldest_accepted_day = current_day - window_size + 1;
    if (window_size > current_day)
      oldest_accepted_day = 0u;

    // Calculate the number of events within the window.
    uint32_t event_count = 0;
    for (const auto& event_day : event->events()) {
      if (event_day.day() < oldest_accepted_day)
        continue;

      event_count += event_day.count();
    }

    return event_count;
  }

  void SetEvent(const Event& event) { events_[event.name()] = event; }

  void IncrementEvent(const std::string& event_name, uint32_t day) override {}

  void ClearEvent(const std::string& event_name) override {}

  void IncrementSnooze(const std::string& event_name,
                       uint32_t day,
                       base::Time time) override {
    last_snooze_time_us_ = time;
  }

  void DismissSnooze(const std::string& event_name) override {
    snooze_dismissed_ = true;
  }

  base::Time GetLastSnoozeTimestamp(
      const std::string& event_name) const override {
    return last_snooze_time_us_;
  }

  uint32_t GetSnoozeCount(const std::string& event_name,
                          uint32_t window,
                          uint32_t current_day) const override {
    const Event* event = GetEvent(event_name);
    if (!event || window == 0u)
      return 0;

    DCHECK(window >= 0);

    uint32_t oldest_accepted_day = current_day - window + 1;
    if (window > current_day)
      oldest_accepted_day = 0u;

    // Calculate the number of snooze within the window.
    uint32_t count = 0;
    for (const auto& event_day : event->events()) {
      if (event_day.day() < oldest_accepted_day ||
          event_day.day() > current_day)
        continue;

      count += event_day.snooze_count();
    }

    return count;
  }

  bool IsSnoozeDismissed(const std::string& event_name) const override {
    return snooze_dismissed_;
  }

 private:
  std::map<std::string, Event> events_;
  base::Time last_snooze_time_us_;
  bool ready_ = true;
  bool snooze_dismissed_ = false;
};

class TestAvailabilityModel : public AvailabilityModel {
 public:
  TestAvailabilityModel() : ready_(true) {}

  TestAvailabilityModel(const TestAvailabilityModel&) = delete;
  TestAvailabilityModel& operator=(const TestAvailabilityModel&) = delete;

  ~TestAvailabilityModel() override = default;

  void Initialize(AvailabilityModel::OnInitializedCallback callback,
                  uint32_t current_day) override {}

  bool IsReady() const override { return ready_; }

  void SetIsReady(bool ready) { ready_ = ready; }

  std::optional<uint32_t> GetAvailability(
      const base::Feature& feature) const override {
    auto search = availabilities_.find(feature.name);
    if (search == availabilities_.end())
      return std::nullopt;

    return search->second;
  }

  void SetAvailability(const base::Feature* feature,
                       std::optional<uint32_t> availability) {
    availabilities_[feature->name] = availability;
  }

 private:
  bool ready_;

  std::map<std::string, std::optional<uint32_t>> availabilities_;
};

class TestDisplayLockController : public DisplayLockController {
 public:
  TestDisplayLockController() = default;

  TestDisplayLockController(const TestDisplayLockController&) = delete;
  TestDisplayLockController& operator=(const TestDisplayLockController&) =
      delete;

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
};

class FeatureConfigConditionValidatorTest : public ::testing::Test {
 public:
  FeatureConfigConditionValidatorTest() = default;

  FeatureConfigConditionValidatorTest(
      const FeatureConfigConditionValidatorTest&) = delete;
  FeatureConfigConditionValidatorTest& operator=(
      const FeatureConfigConditionValidatorTest&) = delete;

 protected:
  ConditionValidator::Result GetResultForEventWindow(Comparator comparator,
                                                     uint32_t window) {
    FeatureConfig config = GetAcceptingFeatureConfig();
    config.event_configs.insert(EventConfig("event1", comparator, window, 0));
    return validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config, {},
                                      event_model_, availability_model_,
                                      display_lock_controller_, &configuration_,
                                      time_provider_);
  }

  ConditionValidator::Result GetResult(const FeatureConfig& config) {
    return GetResultWithGroups(config, {});
  }

  ConditionValidator::Result GetResultForFeature(const base::Feature& feature,
                                                 const FeatureConfig& config) {
    return validator_.MeetsConditions(
        feature, config, {}, event_model_, availability_model_,
        display_lock_controller_, &configuration_, time_provider_);
  }

  ConditionValidator::Result GetResultWithGroups(
      const FeatureConfig& config,
      std::vector<GroupConfig> group_configs) {
    return validator_.MeetsConditions(
        kFeatureConfigTestFeatureFoo, config, group_configs, event_model_,
        availability_model_, display_lock_controller_, &configuration_,
        time_provider_);
  }

  TestEventModel event_model_;
  TestAvailabilityModel availability_model_;
  TestDisplayLockController display_lock_controller_;
  FeatureConfigConditionValidator validator_;
  TestConfiguration configuration_;
  TestTimeProvider time_provider_;
};

}  // namespace

TEST_F(FeatureConfigConditionValidatorTest, ModelNotReadyShouldFail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  event_model_.SetIsReady(false);

  ConditionValidator::Result result = GetResult(GetValidFeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.event_model_ready_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, ConfigInvalidShouldFail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  ConditionValidator::Result result = GetResult(FeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.config_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, MultipleErrorsShouldBeSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  event_model_.SetIsReady(false);

  ConditionValidator::Result result = GetResult(FeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.event_model_ready_ok);
  EXPECT_FALSE(result.config_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, ReadyModelEmptyConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  EXPECT_TRUE(GetResult(GetValidFeatureConfig()).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, ReadyModelAcceptingConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  EXPECT_TRUE(GetResult(GetAcceptingFeatureConfig()).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, Used) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.used = EventConfig("used", Comparator(LESS_THAN, 0), 0, 0);

  ConditionValidator::Result result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.used_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, Trigger) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.trigger = EventConfig("trigger", Comparator(LESS_THAN, 0), 0, 0);

  ConditionValidator::Result result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.trigger_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, SingleOKPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResult(config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, MultipleOKPreconditions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));
  config.event_configs.insert(EventConfig("event2", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResult(config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, OneOKThenOneFailingPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result = GetResult(config);
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

  ConditionValidator::Result result = GetResult(config);
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

  ConditionValidator::Result result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, PriorityNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());

  validator_.SetPriorityNotification(kFeatureConfigTestFeatureFoo.name);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  ConditionValidator::Result result =
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.priority_notification_ok);

  validator_.SetPriorityNotification(std::nullopt);
  validator_.SetPriorityNotification(kFeatureConfigTestFeatureBar.name);
  EXPECT_FALSE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());

  validator_.SetPriorityNotification(std::nullopt);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());
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

  EXPECT_TRUE(GetResult(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResult(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result = GetResult(foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  result = GetResult(foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
}

// Tests that the session rate is zero after resetting the session.
TEST_F(FeatureConfigConditionValidatorTest, SessionRateIsZeroAfterReset) {
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
  FeatureConfig qux_config = GetAcceptingFeatureConfig();
  qux_config.session_rate = Comparator(EQUAL, 0u);

  // Current session has 2, making the `foo_config` fail the check.
  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result = GetResult(foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);

  validator_.ResetSession();

  // After resetting, current session has 0, making the `qux_config` pass the
  // check.
  result = GetResult(qux_config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.session_rate_ok);
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

  EXPECT_TRUE(GetResult(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, affects_none_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResult(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, affects_none_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResult(foo_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, affects_none_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResult(foo_config).NoErrors());
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
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  ConditionValidator::Result result =
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());
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
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureFoo,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureFoo);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureFoo,
                             affects_only_foo_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureFoo);
  ConditionValidator::Result result =
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());
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
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureXyz, xyz_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_foo_and_bar_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureBar, bar_config).NoErrors());
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureXyz, xyz_config).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureQux,
                             affects_foo_and_bar_config, all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureQux);
  ConditionValidator::Result foo_result =
      GetResultForFeature(kFeatureConfigTestFeatureFoo, foo_config);
  EXPECT_FALSE(foo_result.NoErrors());
  EXPECT_FALSE(foo_result.session_rate_ok);
  ConditionValidator::Result bar_result =
      GetResultForFeature(kFeatureConfigTestFeatureFoo, bar_config);
  EXPECT_FALSE(bar_result.NoErrors());
  EXPECT_FALSE(bar_result.session_rate_ok);
  EXPECT_TRUE(
      GetResultForFeature(kFeatureConfigTestFeatureXyz, xyz_config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, Availability) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  EXPECT_TRUE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(100u);
  EXPECT_TRUE(GetResult(config).NoErrors());

  // When the AvailabilityModel is not ready, it should fail.
  availability_model_.SetIsReady(false);
  time_provider_.SetCurrentDay(0u);
  ConditionValidator::Result result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_model_ready_ok);
  time_provider_.SetCurrentDay(100u);
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_model_ready_ok);

  // Reset state back to ready.
  availability_model_.SetIsReady(true);

  // For a feature that became available on day 2 that has to have been
  // available for at least 1 day, it should start being accepted on day 3.
  availability_model_.SetAvailability(&kFeatureConfigTestFeatureFoo, 2u);
  config.availability = Comparator(GREATER_THAN_OR_EQUAL, 1u);
  time_provider_.SetCurrentDay(1u);
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  time_provider_.SetCurrentDay(2u);
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  time_provider_.SetCurrentDay(3u);
  EXPECT_TRUE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(4u);
  EXPECT_TRUE(GetResult(config).NoErrors());

  // For a feature that became available on day 10 that has to have been
  // available for at least 3 days, it should start being accepted on day 13.
  availability_model_.SetAvailability(&kFeatureConfigTestFeatureFoo, 10u);
  config.availability = Comparator(GREATER_THAN_OR_EQUAL, 3u);
  time_provider_.SetCurrentDay(11u);
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  time_provider_.SetCurrentDay(12u);
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.availability_ok);
  time_provider_.SetCurrentDay(13u);
  EXPECT_TRUE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(14u);
  EXPECT_TRUE(GetResult(config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, SnoozeExpiration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});
  FeatureConfig config = GetAcceptingFeatureConfig();
  base::Time baseline = base::Time::Now();

  // Set up snooze params.
  SnoozeParams snooze_params;
  snooze_params.max_limit = 5;
  snooze_params.snooze_interval = 3;
  config.snooze_params = snooze_params;
  config.trigger.window = 5;

  ConditionValidator::Result result = GetResult(config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.snooze_expiration_ok);
  EXPECT_TRUE(result.should_show_snooze);

  // Adding snooze count for |event|.
  Event event;
  event.set_name(config.trigger.name);
  test::SetSnoozeCountForDay(&event, 1u, 1u);
  test::SetSnoozeCountForDay(&event, 3u, 2u);
  test::SetSnoozeCountForDay(&event, 5u, 2u);
  event_model_.SetEvent(event);

  // Updating last snooze timestamp.
  event_model_.IncrementSnooze(config.trigger.name, 1u,
                               baseline - base::Days(4));

  // Verify that snooze conditions are met at day 3.
  time_provider_.SetCurrentDay(3u);
  result = GetResult(config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.snooze_expiration_ok);
  EXPECT_TRUE(result.should_show_snooze);

  // When last snooze timestamp is too recent.
  event_model_.IncrementSnooze(config.trigger.name, 1u,
                               baseline - base::Days(2));
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.snooze_expiration_ok);
  EXPECT_FALSE(result.should_show_snooze);

  // Reset the last snooze timestamp.
  event_model_.IncrementSnooze(config.trigger.name, 1u,
                               baseline - base::Days(4));
  result = GetResult(config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.snooze_expiration_ok);
  EXPECT_TRUE(result.should_show_snooze);

  // When snooze is dismissed.
  event_model_.DismissSnooze(config.trigger.name);
  result = GetResult(config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.snooze_expiration_ok);
  EXPECT_FALSE(result.should_show_snooze);
}

TEST_F(FeatureConfigConditionValidatorTest, ShouldShowSnooze) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});
  FeatureConfig config = GetAcceptingFeatureConfig();

  // Set up snooze params.
  SnoozeParams snooze_params;
  snooze_params.max_limit = 5;
  snooze_params.snooze_interval = 3;
  config.snooze_params = snooze_params;
  config.trigger.window = 5;

  ConditionValidator::Result result = GetResult(config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.should_show_snooze);

  // Adding snooze count for |event|.
  Event event;
  event.set_name(config.trigger.name);
  test::SetSnoozeCountForDay(&event, 1u, 10u);
  event_model_.SetEvent(event);

  // When snooze count exceeds the maximum limit.
  time_provider_.SetCurrentDay(5u);
  result = GetResult(config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_FALSE(result.should_show_snooze);
}

TEST_F(FeatureConfigConditionValidatorTest, SingleEventChangingComparator) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  time_provider_.SetCurrentDay(102u);
  uint32_t window = 10u;

  // Create event with 10 events per day for three days.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 100u, 10u);
  test::SetEventCountForDay(&event1, 101u, 10u);
  test::SetEventCountForDay(&event1, 102u, 10u);
  event_model_.SetEvent(event1);

  EXPECT_TRUE(
      GetResultForEventWindow(Comparator(LESS_THAN, 50u), window).NoErrors());
  EXPECT_TRUE(
      GetResultForEventWindow(Comparator(EQUAL, 30u), window).NoErrors());
  EXPECT_FALSE(
      GetResultForEventWindow(Comparator(LESS_THAN, 30u), window).NoErrors());
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

  time_provider_.SetCurrentDay(104u);

  EXPECT_FALSE(
      GetResultForEventWindow(Comparator(GREATER_THAN, 30u), 0).NoErrors());
  EXPECT_FALSE(
      GetResultForEventWindow(Comparator(GREATER_THAN, 30u), 1u).NoErrors());
  EXPECT_FALSE(
      GetResultForEventWindow(Comparator(GREATER_THAN, 30u), 2u).NoErrors());
  EXPECT_FALSE(
      GetResultForEventWindow(Comparator(GREATER_THAN, 30u), 3u).NoErrors());
  EXPECT_TRUE(
      GetResultForEventWindow(Comparator(GREATER_THAN, 30u), 4u).NoErrors());
  EXPECT_TRUE(
      GetResultForEventWindow(Comparator(GREATER_THAN, 30u), 5u).NoErrors());
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

  time_provider_.SetCurrentDay(100u);

  EXPECT_TRUE(GetResultForEventWindow(Comparator(EQUAL, 10u), 99u).NoErrors());
  EXPECT_TRUE(GetResultForEventWindow(Comparator(EQUAL, 20u), 100u).NoErrors());
  EXPECT_TRUE(GetResultForEventWindow(Comparator(EQUAL, 30u), 101u).NoErrors());
  EXPECT_TRUE(
      GetResultForEventWindow(Comparator(EQUAL, 30u), 1000u).NoErrors());
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

  time_provider_.SetCurrentDay(100u);

  // Verify validator counts correctly for two events last 99 days.
  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 10u), 99u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 5u), 99u, 0));
  ConditionValidator::Result result = validator_.MeetsConditions(
      kFeatureConfigTestFeatureFoo, config, {}, event_model_,
      availability_model_, display_lock_controller_, &configuration_,
      time_provider_);
  EXPECT_TRUE(result.NoErrors());

  // Verify validator counts correctly for two events last 100 days.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 20u), 100u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 10u), 100u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config, {},
                                      event_model_, availability_model_,
                                      display_lock_controller_, &configuration_,
                                      time_provider_);
  EXPECT_TRUE(result.NoErrors());

  // Verify validator counts correctly for two events last 101 days.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 30u), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 15u), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config, {},
                                      event_model_, availability_model_,
                                      display_lock_controller_, &configuration_,
                                      time_provider_);
  EXPECT_TRUE(result.NoErrors());

  // Verify validator counts correctly for two events last 101 days, and returns
  // error when first event fails.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 0), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 15u), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config, {},
                                      event_model_, availability_model_,
                                      display_lock_controller_, &configuration_,
                                      time_provider_);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);

  // Verify validator counts correctly for two events last 101 days, and returns
  // error when second event fails.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 30u), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 0), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config, {},
                                      event_model_, availability_model_,
                                      display_lock_controller_, &configuration_,
                                      time_provider_);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);

  // Verify validator counts correctly for two events last 101 days, and returns
  // error when both events fail.
  config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(EQUAL, 0), 101u, 0));
  config.event_configs.insert(
      EventConfig("event2", Comparator(EQUAL, 0), 101u, 0));
  result = validator_.MeetsConditions(kFeatureConfigTestFeatureFoo, config, {},
                                      event_model_, availability_model_,
                                      display_lock_controller_, &configuration_,
                                      time_provider_);
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
  EXPECT_TRUE(GetResult(config).NoErrors());

  // Set that we triggered on day 0. We should then only trigger on day 2+.
  Event trigger_event;
  trigger_event.set_name("trigger");
  test::SetEventCountForDay(&trigger_event, 0u, 1u);
  event_model_.SetEvent(trigger_event);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(1u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(2u);
  EXPECT_TRUE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(3u);
  EXPECT_TRUE(GetResult(config).NoErrors());

  // Set that we triggered again on day 2. We should then not trigger again
  // until max storage time has passed (100 days), which would expire the
  // trigger from day 0.
  test::SetEventCountForDay(&trigger_event, 2u, 1u);
  event_model_.SetEvent(trigger_event);
  time_provider_.SetCurrentDay(2u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(3u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(4u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(5u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(99u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(100u);
  EXPECT_TRUE(GetResult(config).NoErrors());
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
  EXPECT_TRUE(GetResult(config).NoErrors());

  // Set that we had event1 on day 0. We should then only trigger on day 2+.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 0u, 1u);
  event_model_.SetEvent(event1);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(1u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(2u);
  EXPECT_TRUE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(3u);
  EXPECT_TRUE(GetResult(config).NoErrors());

  // Set that we had event1 again on day 2. We should then not trigger again
  // until max storage time has passed (100 days), which would expire the
  // trigger from day 0.
  test::SetEventCountForDay(&event1, 2u, 1u);
  event_model_.SetEvent(event1);
  time_provider_.SetCurrentDay(2u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(3u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(4u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(5u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(99u);
  EXPECT_FALSE(GetResult(config).NoErrors());
  time_provider_.SetCurrentDay(100u);
  EXPECT_TRUE(GetResult(config).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, DisplayLockedStatus) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  // When the display is locked, the result should be negative.
  display_lock_controller_.SetNextIsDisplayLockedResult(true);

  ConditionValidator::Result result = GetResult(GetAcceptingFeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.display_lock_ok);

  // Setting the display to unlocked should make the result positive.
  display_lock_controller_.SetNextIsDisplayLockedResult(false);

  EXPECT_TRUE(GetResult(GetAcceptingFeatureConfig()).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, TestConcurrentPromosBlockingAll) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  validator_.NotifyIsShowing(
      kFeatureConfigTestFeatureBar, FeatureConfig(),
      {kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name});
  ConditionValidator::Result result = GetResult(GetValidFeatureConfig());
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.currently_showing_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, TestConcurrentPromosBlockingNone) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});

  FeatureConfig non_blocking_config = GetNonBlockingFeatureConfig();
  validator_.NotifyIsShowing(
      kFeatureConfigTestFeatureBar, FeatureConfig(),
      {kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name});
  ConditionValidator::Result result = GetResult(non_blocking_config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.currently_showing_ok);
}

TEST_F(FeatureConfigConditionValidatorTest,
       TestConcurrentPromosBlockingExplicitBlocked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});

  FeatureConfig non_blocking_config = GetNonBlockingFeatureConfig();
  non_blocking_config.blocked_by.type = BlockedBy::Type::EXPLICIT;
  non_blocking_config.blocked_by.affected_features = {
      kFeatureConfigTestFeatureBar.name};
  validator_.NotifyIsShowing(
      kFeatureConfigTestFeatureBar, FeatureConfig(),
      {kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name});
  ConditionValidator::Result result = GetResult(non_blocking_config);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.currently_showing_ok);
}

TEST_F(FeatureConfigConditionValidatorTest,
       TestConcurrentPromosBlockingExplicitNotBlocked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar,
       kFeatureConfigTestFeatureQux},
      {});

  FeatureConfig non_blocking_config = GetNonBlockingFeatureConfig();
  non_blocking_config.blocked_by.type = BlockedBy::Type::EXPLICIT;
  non_blocking_config.blocked_by.affected_features = {
      kFeatureConfigTestFeatureBar.name};
  validator_.NotifyIsShowing(
      kFeatureConfigTestFeatureQux, FeatureConfig(),
      {kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name,
       kFeatureConfigTestFeatureQux.name});
  ConditionValidator::Result result = GetResult(non_blocking_config);
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.currently_showing_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, GroupConfigInvalidShouldFail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetValidFeatureConfig();
  GroupConfig group_config = GroupConfig();

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.config_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, ReadyModelEmptyGroupConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetValidFeatureConfig();
  GroupConfig group_config = GetValidGroupConfig();

  EXPECT_TRUE(GetResultWithGroups(config, {group_config}).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, ReadyModelAcceptingGroupConfig) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_config = GetAcceptingGroupConfig();

  EXPECT_TRUE(GetResultWithGroups(config, {group_config}).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, GroupTrigger) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.trigger = EventConfig("trigger", Comparator(LESS_THAN, 0), 0, 0);

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.trigger_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, GroupSingleOKPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.event_configs.insert(
      EventConfig("event1", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResultWithGroups(config, {group_config}).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest, GroupMultipleOKPreconditions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.event_configs.insert(
      EventConfig("event1", Comparator(ANY, 0), 0, 0));
  group_config.event_configs.insert(
      EventConfig("event2", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResultWithGroups(config, {group_config}).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest,
       GroupOneOKThenOneFailingPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.event_configs.insert(
      EventConfig("event1", Comparator(ANY, 0), 0, 0));
  group_config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, MultipleGroupOKPreconditions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_one_config = GetAcceptingGroupConfig();
  group_one_config.event_configs.insert(
      EventConfig("event1", Comparator(ANY, 0), 0, 0));
  GroupConfig group_two_config = GetAcceptingGroupConfig();
  group_two_config.event_configs.insert(
      EventConfig("event2", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResultWithGroups(config, {group_one_config, group_two_config})
                  .NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest,
       MultipleGroupOneOKThenOneFailingPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_one_config = GetAcceptingGroupConfig();
  group_one_config.event_configs.insert(
      EventConfig("event1", Comparator(ANY, 0), 0, 0));
  GroupConfig group_two_config = GetAcceptingGroupConfig();
  group_two_config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_one_config, group_two_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest,
       MultipleGroupOneFailingThenOneOKPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  GroupConfig group_one_config = GetAcceptingGroupConfig();
  group_one_config.event_configs.insert(
      EventConfig("event1", Comparator(LESS_THAN, 0), 0, 0));
  GroupConfig group_two_config = GetAcceptingGroupConfig();
  group_two_config.event_configs.insert(
      EventConfig("event2", Comparator(ANY, 0), 0, 0));

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_one_config, group_two_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, FeatureAndGroupOKPreconditions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));

  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.event_configs.insert(
      EventConfig("event2", Comparator(ANY, 0), 0, 0));

  EXPECT_TRUE(GetResultWithGroups(config, {group_config}).NoErrors());
}

TEST_F(FeatureConfigConditionValidatorTest,
       FeatureOKThenGroupFailingPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(EventConfig("event1", Comparator(ANY, 0), 0, 0));

  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.event_configs.insert(
      EventConfig("event2", Comparator(LESS_THAN, 0), 0, 0));

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest,
       FeatureFailingThenGroupOKPrecondition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFeatureConfigTestFeatureFoo}, {});

  FeatureConfig config = GetAcceptingFeatureConfig();
  config.event_configs.insert(
      EventConfig("event1", Comparator(LESS_THAN, 0), 0, 0));

  GroupConfig group_one_config = GetAcceptingGroupConfig();
  group_one_config.event_configs.insert(
      EventConfig("event2", Comparator(ANY, 0), 0, 0));

  ConditionValidator::Result result =
      GetResultWithGroups(config, {group_one_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.preconditions_ok);
  EXPECT_TRUE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, GroupSessionRate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.session_rate = Comparator(LESS_THAN, 2u);

  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result =
      GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  result = GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);
}

// Tests that when group session rate is zero, it passes the test after session
// is reset.
TEST_F(FeatureConfigConditionValidatorTest, GroupSessionRateIsZeroAfterReset) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.session_rate = Comparator(EQUAL, 0u);

  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  // Current session rate is 1, the `group_config` will fail the check.
  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result =
      GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);

  validator_.ResetSession();

  // Current session rate is 0, the `group_config` will pass the check.
  result = GetResultWithGroups(foo_config, {group_config});
  EXPECT_TRUE(result.NoErrors());
  EXPECT_TRUE(result.session_rate_ok);
  EXPECT_TRUE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest,
       GroupAndFeatureSessionRateGroupLower) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 3u);
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.session_rate = Comparator(LESS_THAN, 2u);

  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result =
      GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  result = GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest,
       GroupAndFeatureSessionRateGroupHigher) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  foo_config.session_rate = Comparator(LESS_THAN, 2u);
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  GroupConfig group_config = GetAcceptingGroupConfig();
  group_config.session_rate = Comparator(LESS_THAN, 3u);

  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultWithGroups(foo_config, {group_config}).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result =
      GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_TRUE(result.groups_ok);

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  result = GetResultWithGroups(foo_config, {group_config});
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);
}

TEST_F(FeatureConfigConditionValidatorTest, TwoGroupsSessionRate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kFeatureConfigTestFeatureFoo, kFeatureConfigTestFeatureBar}, {});
  std::vector<std::string> all_feature_names = {
      kFeatureConfigTestFeatureFoo.name, kFeatureConfigTestFeatureBar.name};

  FeatureConfig foo_config = GetAcceptingFeatureConfig();
  FeatureConfig bar_config = GetAcceptingFeatureConfig();

  GroupConfig group_one_config = GetAcceptingGroupConfig();
  group_one_config.session_rate = Comparator(LESS_THAN, 3u);
  GroupConfig group_two_config = GetAcceptingGroupConfig();
  group_two_config.session_rate = Comparator(LESS_THAN, 2u);

  std::vector<GroupConfig> group_configs = {group_one_config, group_two_config};

  EXPECT_TRUE(GetResultWithGroups(foo_config, group_configs).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  EXPECT_TRUE(GetResultWithGroups(foo_config, group_configs).NoErrors());

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  ConditionValidator::Result result =
      GetResultWithGroups(foo_config, group_configs);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);

  validator_.NotifyIsShowing(kFeatureConfigTestFeatureBar, bar_config,
                             all_feature_names);
  validator_.NotifyDismissed(kFeatureConfigTestFeatureBar);
  result = GetResultWithGroups(foo_config, group_configs);
  EXPECT_FALSE(result.NoErrors());
  EXPECT_FALSE(result.session_rate_ok);
  EXPECT_FALSE(result.groups_ok);
}

}  // namespace feature_engagement
