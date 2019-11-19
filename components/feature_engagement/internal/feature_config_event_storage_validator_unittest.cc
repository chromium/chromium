// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/feature_config_event_storage_validator.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/public/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

const base::Feature kEventStorageTestFeatureFoo{
    "test_foo", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEventStorageTestFeatureBar{
    "test_bar", base::FEATURE_DISABLED_BY_DEFAULT};

FeatureConfig kNeverStored;
FeatureConfig kStoredInUsed1Day;
FeatureConfig kStoredInUsed2Days;
FeatureConfig kStoredInUsed10Days;
FeatureConfig kStoredInTrigger1Day;
FeatureConfig kStoredInTrigger2Days;
FeatureConfig kStoredInTrigger10Days;
FeatureConfig kStoredInEventConfigs1Day;
FeatureConfig kStoredInEventConfigs2Days;
FeatureConfig kStoredInEventConfigs10Days;

void InitializeStorageFeatureConfigs() {
  FeatureConfig default_config;
  default_config.valid = true;
  default_config.used = EventConfig("myevent", Comparator(ANY, 0), 0, 0);
  default_config.trigger = EventConfig("myevent", Comparator(ANY, 0), 0, 0);
  default_config.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 0));
  default_config.event_configs.insert(
      EventConfig("unrelated_event", Comparator(ANY, 0), 0, 100));
  default_config.session_rate = Comparator(ANY, 0);
  default_config.availability = Comparator(ANY, 0);

  kNeverStored = default_config;

  kStoredInUsed1Day = default_config;
  kStoredInUsed1Day.used = EventConfig("myevent", Comparator(ANY, 0), 0, 1);

  kStoredInUsed2Days = default_config;
  kStoredInUsed2Days.used = EventConfig("myevent", Comparator(ANY, 0), 0, 2);

  kStoredInUsed10Days = default_config;
  kStoredInUsed10Days.used = EventConfig("myevent", Comparator(ANY, 0), 0, 10);

  kStoredInTrigger1Day = default_config;
  kStoredInTrigger1Day.trigger =
      EventConfig("myevent", Comparator(ANY, 0), 0, 1);

  kStoredInTrigger2Days = default_config;
  kStoredInTrigger2Days.trigger =
      EventConfig("myevent", Comparator(ANY, 0), 0, 2);

  kStoredInTrigger10Days = default_config;
  kStoredInTrigger10Days.trigger =
      EventConfig("myevent", Comparator(ANY, 0), 0, 10);

  kStoredInEventConfigs1Day = default_config;
  kStoredInEventConfigs1Day.event_configs.clear();
  kStoredInEventConfigs1Day.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 0));
  kStoredInEventConfigs1Day.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 1));
  kStoredInEventConfigs1Day.event_configs.insert(
      EventConfig("unrelated_event", Comparator(ANY, 0), 0, 100));

  kStoredInEventConfigs2Days = default_config;
  kStoredInEventConfigs2Days.event_configs.clear();
  kStoredInEventConfigs2Days.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 0));
  kStoredInEventConfigs2Days.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 2));
  kStoredInEventConfigs2Days.event_configs.insert(
      EventConfig("unrelated_event", Comparator(ANY, 0), 0, 100));

  kStoredInEventConfigs10Days = default_config;
  kStoredInEventConfigs10Days.event_configs.clear();
  kStoredInEventConfigs10Days.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 0));
  kStoredInEventConfigs10Days.event_configs.insert(
      EventConfig("myevent", Comparator(ANY, 0), 0, 10));
  kStoredInEventConfigs10Days.event_configs.insert(
      EventConfig("unrelated_event", Comparator(ANY, 0), 0, 100));
}

class FeatureConfigEventStorageValidatorTest : public ::testing::Test {
 public:
  FeatureConfigEventStorageValidatorTest() : current_day_(100) {
    InitializeStorageFeatureConfigs();
  }

  void UseConfig(const FeatureConfig& foo_config) {
    FeatureVector features = {&kEventStorageTestFeatureFoo};

    validator_.ClearForTesting();
    EditableConfiguration configuration;
    configuration.SetConfiguration(&kEventStorageTestFeatureFoo, foo_config);
    validator_.InitializeFeatures(features, configuration);
  }

  void UseConfigs(const FeatureConfig& foo_config,
                  const FeatureConfig& bar_config) {
    FeatureVector features = {&kEventStorageTestFeatureFoo,
                              &kEventStorageTestFeatureBar};

    validator_.ClearForTesting();
    EditableConfiguration configuration;
    configuration.SetConfiguration(&kEventStorageTestFeatureFoo, foo_config);
    configuration.SetConfiguration(&kEventStorageTestFeatureBar, bar_config);
    validator_.InitializeFeatures(features, configuration);
  }

  void VerifyNeverKeep() {
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 89, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 90, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 91, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 98, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 99, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 100, current_day_));
    // This is trying to store data in the future, which should never happen.
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 101, current_day_));
  }

  void VerifyKeep1Day() {
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 89, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 90, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 91, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 98, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 99, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 100, current_day_));
    // This is trying to store data in the future, which should never happen.
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 101, current_day_));
  }

  void VerifyKeep2Days() {
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 89, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 90, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 91, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 98, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 99, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 100, current_day_));
    // This is trying to store data in the future, which should never happen.
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 101, current_day_));
  }

  void VerifyKeep10Days() {
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 89, current_day_));
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 90, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 91, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 98, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 99, current_day_));
    EXPECT_TRUE(validator_.ShouldKeep("myevent", 100, current_day_));
    // This is trying to store data in the future, which should never happen.
    EXPECT_FALSE(validator_.ShouldKeep("myevent", 101, current_day_));
  }

 protected:
  FeatureConfigEventStorageValidator validator_;
  uint32_t current_day_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeatureConfigEventStorageValidatorTest);
};

}  // namespace

TEST_F(FeatureConfigEventStorageValidatorTest,
       ShouldOnlyUseConfigFromEnabledFeatures) {
  scoped_feature_list_.InitWithFeatures({kEventStorageTestFeatureFoo},
                                        {kEventStorageTestFeatureBar});

  FeatureConfig foo_config = kNeverStored;
  foo_config.used = EventConfig("fooevent", Comparator(ANY, 0), 0, 1);
  FeatureConfig bar_config = kNeverStored;
  bar_config.used = EventConfig("barevent", Comparator(ANY, 0), 0, 1);
  UseConfigs(foo_config, bar_config);

  EXPECT_FALSE(validator_.ShouldStore("myevent"));
  EXPECT_TRUE(validator_.ShouldStore("fooevent"));
  EXPECT_FALSE(validator_.ShouldStore("barevent"));
}

TEST_F(FeatureConfigEventStorageValidatorTest,
       ShouldStoreIfSingleConfigHasMinimum1DayStorage) {
  scoped_feature_list_.InitWithFeatures({kEventStorageTestFeatureFoo}, {});

  UseConfig(kNeverStored);
  EXPECT_FALSE(validator_.ShouldStore("myevent"));

  const FeatureConfig* should_store_configs[] = {
      &kStoredInUsed1Day,          &kStoredInUsed2Days,
      &kStoredInUsed10Days,        &kStoredInTrigger1Day,
      &kStoredInTrigger2Days,      &kStoredInTrigger10Days,
      &kStoredInEventConfigs1Day,  &kStoredInEventConfigs2Days,
      &kStoredInEventConfigs10Days};
  for (const FeatureConfig* config : should_store_configs) {
    UseConfig(*config);
    EXPECT_TRUE(validator_.ShouldStore("myevent"));
  }
}

TEST_F(FeatureConfigEventStorageValidatorTest,
       ShouldStoreIfAnyConfigHasMinimum1DayStorage) {
  scoped_feature_list_.InitWithFeatures(
      {kEventStorageTestFeatureFoo, kEventStorageTestFeatureBar}, {});

  UseConfigs(kNeverStored, kNeverStored);
  EXPECT_FALSE(validator_.ShouldStore("myevent"));

  const FeatureConfig* should_store_configs[] = {
      &kStoredInUsed1Day,          &kStoredInUsed2Days,
      &kStoredInUsed10Days,        &kStoredInTrigger1Day,
      &kStoredInTrigger2Days,      &kStoredInTrigger10Days,
      &kStoredInEventConfigs1Day,  &kStoredInEventConfigs2Days,
      &kStoredInEventConfigs10Days};
  for (const FeatureConfig* config : should_store_configs) {
    UseConfigs(kNeverStored, *config);
    EXPECT_TRUE(validator_.ShouldStore("myevent"));
  }
}

TEST_F(FeatureConfigEventStorageValidatorTest,
       ShouldKeepIfSingleConfigMeetsEventAge) {
  scoped_feature_list_.InitWithFeatures({kEventStorageTestFeatureFoo}, {});

  UseConfig(kNeverStored);
  VerifyNeverKeep();

  const FeatureConfig* one_day_storage_configs[] = {
      &kStoredInUsed1Day, &kStoredInTrigger1Day, &kStoredInEventConfigs1Day};
  for (const FeatureConfig* config : one_day_storage_configs) {
    UseConfig(*config);
    VerifyKeep1Day();
  }

  const FeatureConfig* two_days_storage_configs[] = {
      &kStoredInUsed2Days, &kStoredInTrigger2Days, &kStoredInEventConfigs2Days};
  for (const FeatureConfig* config : two_days_storage_configs) {
    UseConfig(*config);
    VerifyKeep2Days();
  }

  const FeatureConfig* ten_days_storage_configs[] = {
      &kStoredInUsed10Days, &kStoredInTrigger10Days,
      &kStoredInEventConfigs10Days};
  for (const FeatureConfig* config : ten_days_storage_configs) {
    UseConfig(*config);
    VerifyKeep10Days();
  }
}

TEST_F(FeatureConfigEventStorageValidatorTest,
       ShouldKeepIfAnyConfigMeetsEventAge) {
  scoped_feature_list_.InitWithFeatures(
      {kEventStorageTestFeatureFoo, kEventStorageTestFeatureBar}, {});

  UseConfigs(kNeverStored, kNeverStored);
  VerifyNeverKeep();

  const FeatureConfig* one_day_storage_configs[] = {
      &kStoredInUsed1Day, &kStoredInTrigger1Day, &kStoredInEventConfigs1Day};
  for (const FeatureConfig* config : one_day_storage_configs) {
    UseConfigs(kNeverStored, *config);
    VerifyKeep1Day();
  }

  const FeatureConfig* two_days_storage_configs[] = {
      &kStoredInUsed2Days, &kStoredInTrigger2Days, &kStoredInEventConfigs2Days};
  for (const FeatureConfig* config : two_days_storage_configs) {
    UseConfigs(kNeverStored, *config);
    VerifyKeep2Days();
  }

  const FeatureConfig* ten_days_storage_configs[] = {
      &kStoredInUsed10Days, &kStoredInTrigger10Days,
      &kStoredInEventConfigs10Days};
  for (const FeatureConfig* config : ten_days_storage_configs) {
    UseConfigs(kNeverStored, *config);
    VerifyKeep10Days();
  }
}

}  // namespace feature_engagement
