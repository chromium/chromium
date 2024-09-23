// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_configuration_provider.h"

#include <initializer_list>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/group_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/user_education_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace {

constexpr char kToastTrigger[] = "ToastIphFeature_trigger";
constexpr char kToastUsed[] = "ToastIphFeature_used";
constexpr char kSnoozeTrigger[] = "SnoozeIphFeature_trigger";
constexpr char kSnoozeUsed[] = "SnoozeIphFeature_used";
constexpr uint32_t kAdditionalConditionDelayDays = 6;
constexpr uint32_t kAdditionalConditionUsedLimit = 2;
constexpr char kAdditionalConditionName[] = "AdditionalCondition";
constexpr char kAdditionalCondition2Name[] = "AdditionalCondition2";
constexpr uint32_t kAdditionalConditionCount = 3;
constexpr uint32_t kAdditionalConditionDays = 14;
constexpr char kKeyedNoticeTrigger[] = "KeyedNoticeIphFeature_trigger";
constexpr char kKeyedNoticeUsed[] = "KeyedNoticeIphFeature_used";
constexpr char kLegalNoticeTrigger[] = "LegalNoticeIphFeature_trigger";
constexpr char kLegalNoticeUsed[] = "LegalNoticeIphFeature_used";
constexpr char kActionableAlertTrigger[] = "ActionableAlertFeature_trigger";
constexpr char kActionableAlertUsed[] = "ActionableAlertFeature_used";
BASE_FEATURE(kToastIphFeature,
             "IPH_ToastIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSnoozeIphFeature,
             "IPH_SnoozeIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIphWithConditionFeature,
             "IPH_kIphWithConditionFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kKeyedNoticeIphFeature,
             "IPH_KeyedNoticeIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeIphFeature,
             "IPH_LegalNoticeIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActionableAlertFeature,
             "IPH_ActionableAlertFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
const std::initializer_list<const base::Feature*> kKnownFeatures{
    &kToastIphFeature,         &kSnoozeIphFeature,
    &kIphWithConditionFeature, &kKeyedNoticeIphFeature,
    &kActionableAlertFeature,  &kLegalNoticeIphFeature};
const std::initializer_list<const base::Feature*> kKnownGroups{};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);

// Provider must be created after possibly setting v2 feature flag.
//
// Call this only when the provider is actually needed for the test, and after
// SetEnableV2 (if necessary).
template <typename... Args>
std::unique_ptr<UserEducationConfigurationProvider> CreateProvider(
    Args... additional_specifications) {
  user_education::FeaturePromoRegistry registry;

  registry.RegisterFeature(
      user_education::FeaturePromoSpecification::CreateForToastPromo(
          kToastIphFeature, kTestElementId, IDS_OK, IDS_CANCEL,
          user_education::FeaturePromoSpecification::AcceleratorInfo(
              IDC_HOME)));

  registry.RegisterFeature(
      user_education::FeaturePromoSpecification::CreateForSnoozePromo(
          kSnoozeIphFeature, kTestElementId, IDS_CLOSE));

  auto spec = user_education::FeaturePromoSpecification::CreateForCustomAction(
      kKeyedNoticeIphFeature, kTestElementId, IDS_CANCEL, IDS_OK,
      base::DoNothing());
  spec.set_promo_subtype_for_testing(
      user_education::FeaturePromoSpecification::PromoSubtype::kKeyedNotice);
  registry.RegisterFeature(std::move(spec));

  spec = user_education::FeaturePromoSpecification::CreateForCustomAction(
      kLegalNoticeIphFeature, kTestElementId, IDS_CLEAR, IDS_CLOSE,
      base::DoNothing());
  spec.set_promo_subtype_for_testing(
      user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice);
  registry.RegisterFeature(std::move(spec));

  spec = user_education::FeaturePromoSpecification::CreateForCustomAction(
      kActionableAlertFeature, kTestElementId, IDS_CLEAR, IDS_CLOSE,
      base::DoNothing());
  spec.set_promo_subtype_for_testing(user_education::FeaturePromoSpecification::
                                         PromoSubtype::kActionableAlert);
  registry.RegisterFeature(std::move(spec));

  (registry.RegisterFeature(std::move(additional_specifications)), ...);

  return std::make_unique<UserEducationConfigurationProvider>(
      std::move(registry));
}

}  // namespace

using AdditionalConditions =
    user_education::FeaturePromoSpecification::AdditionalConditions;

class UserEducationConfigurationProviderTest : public testing::Test {
 public:
  UserEducationConfigurationProviderTest() {
    kSessionRateImpactNone.type =
        feature_engagement::SessionRateImpact::Type::NONE;
  }
  ~UserEducationConfigurationProviderTest() override = default;

  void SetEnableV2(bool enable_v2) {
    if (enable_v2) {
      feature_list_.InitAndEnableFeature(
          user_education::features::kUserEducationExperienceVersion2);
    } else {
      feature_list_.InitAndDisableFeature(
          user_education::features::kUserEducationExperienceVersion2);
    }
  }

  auto GetDefaultTrigger(const char* name) {
    const auto trigger =
        base::FeatureList::IsEnabled(
            user_education::features::kUserEducationExperienceVersion2)
            ? kAny
            : kLessThan5;
    return feature_engagement::EventConfig(
        name, trigger, feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
  }

  auto GetAnyTrigger(const char* name) {
    return feature_engagement::EventConfig(
        name, kAny, feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
  }

  auto GetDefaultUsed(const char* name) {
    return feature_engagement::EventConfig(
        name, kEqualsZero, feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
  }

 protected:
  const feature_engagement::Comparator kAny;
  const feature_engagement::Blocking kBlockingAll;
  const feature_engagement::BlockedBy kBlockedByAll;
  const feature_engagement::Comparator kEqualsZero{feature_engagement::EQUAL,
                                                   0};
  const feature_engagement::Comparator kLessThan5{feature_engagement::LESS_THAN,
                                                  5};
  const feature_engagement::Comparator kAtLeast7{
      feature_engagement::GREATER_THAN_OR_EQUAL, 7};
  feature_engagement::SessionRateImpact kSessionRateImpactNone;
  const feature_engagement::SessionRateImpact kSessionRateImpactAll;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(UserEducationConfigurationProviderTest, ProvidesToastConfiguration) {
  feature_engagement::FeatureConfig config;

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kToastIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(GetDefaultUsed(kToastUsed), config.used);

  EXPECT_EQ(GetDefaultTrigger(kToastTrigger), config.trigger);

  EXPECT_TRUE(config.event_configs.empty());

  EXPECT_EQ(kAny, config.session_rate);

  EXPECT_EQ(kSessionRateImpactNone, config.session_rate_impact);

  EXPECT_EQ(feature_engagement::BlockedBy(), config.blocked_by);

  EXPECT_EQ(feature_engagement::Blocking(), config.blocking);

  EXPECT_EQ(kAny, config.availability);

  EXPECT_FALSE(config.tracking_only);

  EXPECT_EQ(feature_engagement::SnoozeParams(), config.snooze_params);

  EXPECT_TRUE(config.groups.empty());
}

TEST_F(UserEducationConfigurationProviderTest, ProvidesSnoozeConfiguration) {
  feature_engagement::FeatureConfig config;

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(GetDefaultUsed(kSnoozeUsed), config.used);

  EXPECT_EQ(GetDefaultTrigger(kSnoozeTrigger), config.trigger);

  EXPECT_TRUE(config.event_configs.empty());

  EXPECT_EQ(kEqualsZero, config.session_rate);

  EXPECT_EQ(kSessionRateImpactAll, config.session_rate_impact);

  EXPECT_EQ(feature_engagement::BlockedBy(), config.blocked_by);

  EXPECT_EQ(feature_engagement::Blocking(), config.blocking);

  EXPECT_EQ(kAny, config.availability);

  EXPECT_FALSE(config.tracking_only);

  EXPECT_EQ(feature_engagement::SnoozeParams(), config.snooze_params);

  EXPECT_TRUE(config.groups.empty());
}

TEST_F(UserEducationConfigurationProviderTest,
       ProvidesKeyedNoticeConfiguration) {
  feature_engagement::FeatureConfig config;

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kKeyedNoticeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(GetDefaultUsed(kKeyedNoticeUsed), config.used);

  EXPECT_EQ(GetAnyTrigger(kKeyedNoticeTrigger), config.trigger);

  EXPECT_TRUE(config.event_configs.empty());

  EXPECT_EQ(kAny, config.session_rate);

  EXPECT_EQ(kSessionRateImpactAll, config.session_rate_impact);

  EXPECT_EQ(feature_engagement::BlockedBy(), config.blocked_by);

  EXPECT_EQ(feature_engagement::Blocking(), config.blocking);

  EXPECT_EQ(kAny, config.availability);

  EXPECT_FALSE(config.tracking_only);

  EXPECT_EQ(feature_engagement::SnoozeParams(), config.snooze_params);

  EXPECT_TRUE(config.groups.empty());
}

TEST_F(UserEducationConfigurationProviderTest,
       ProvidesLegalNoticeConfiguration) {
  feature_engagement::FeatureConfig config;

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kLegalNoticeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(GetDefaultUsed(kLegalNoticeUsed), config.used);

  EXPECT_EQ(GetAnyTrigger(kLegalNoticeTrigger), config.trigger);

  EXPECT_TRUE(config.event_configs.empty());

  EXPECT_EQ(kAny, config.session_rate);

  EXPECT_EQ(kSessionRateImpactAll, config.session_rate_impact);

  EXPECT_EQ(feature_engagement::BlockedBy(), config.blocked_by);

  EXPECT_EQ(feature_engagement::Blocking(), config.blocking);

  EXPECT_EQ(kAny, config.availability);

  EXPECT_FALSE(config.tracking_only);

  EXPECT_EQ(feature_engagement::SnoozeParams(), config.snooze_params);

  EXPECT_TRUE(config.groups.empty());
}

TEST_F(UserEducationConfigurationProviderTest,
       ProvidesActionableAlertConfiguration) {
  feature_engagement::FeatureConfig config;

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kActionableAlertFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(GetDefaultUsed(kActionableAlertUsed), config.used);

  EXPECT_EQ(GetAnyTrigger(kActionableAlertTrigger), config.trigger);

  EXPECT_TRUE(config.event_configs.empty());

  EXPECT_EQ(kAny, config.session_rate);

  EXPECT_EQ(kSessionRateImpactAll, config.session_rate_impact);

  EXPECT_EQ(feature_engagement::BlockedBy(), config.blocked_by);

  EXPECT_EQ(feature_engagement::Blocking(), config.blocking);

  EXPECT_EQ(kAny, config.availability);

  EXPECT_FALSE(config.tracking_only);

  EXPECT_EQ(feature_engagement::SnoozeParams(), config.snooze_params);

  EXPECT_TRUE(config.groups.empty());
}

TEST_F(UserEducationConfigurationProviderTest, HandlesEventConfigs) {
  feature_engagement::EventConfig event("other_event", kEqualsZero, 100, 100);

  feature_engagement::FeatureConfig config;
  config.event_configs.insert(event);

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(kAtLeast7, config.availability);

  EXPECT_THAT(config.event_configs, testing::ElementsAre(event));
}

TEST_F(UserEducationConfigurationProviderTest, DoesntOverwriteNames) {
  feature_engagement::FeatureConfig config;
  config.trigger.name = "foo";
  config.used.name = "bar";

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(GetDefaultTrigger("foo"), config.trigger);
  EXPECT_EQ(GetDefaultUsed("bar"), config.used);
}

TEST_F(UserEducationConfigurationProviderTest, v1_DoesntOverwriteValid) {
  SetEnableV2(false);
  feature_engagement::FeatureConfig config;
  const feature_engagement::EventConfig trigger(
      "foo", feature_engagement::Comparator(feature_engagement::LESS_THAN, 10),
      100, 99);
  const feature_engagement::EventConfig used(
      "bar", feature_engagement::Comparator(feature_engagement::LESS_THAN, 8),
      97, 98);
  config.trigger = trigger;
  config.used = used;
  config.valid = true;
  EXPECT_FALSE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(trigger, config.trigger);
  EXPECT_EQ(used, config.used);
}

TEST_F(UserEducationConfigurationProviderTest,
       v2_DoesOverwriteValid_SpecifiesUsed) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  const feature_engagement::EventConfig trigger(
      "foo", feature_engagement::Comparator(feature_engagement::LESS_THAN, 10),
      100, 99);
  const feature_engagement::EventConfig used(
      "bar", feature_engagement::Comparator(feature_engagement::LESS_THAN, 8),
      97, 98);
  config.trigger = trigger;
  config.used = used;
  config.valid = true;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(GetDefaultTrigger("foo"), config.trigger);
  EXPECT_EQ(used, config.used);
}

TEST_F(UserEducationConfigurationProviderTest,
       v2_DoesOverwriteValid_NoUsedEvent) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  const feature_engagement::EventConfig trigger(
      "foo", feature_engagement::Comparator(feature_engagement::LESS_THAN, 10),
      100, 99);
  config.trigger = trigger;
  config.valid = true;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(GetDefaultTrigger("foo"), config.trigger);
  EXPECT_EQ(GetDefaultUsed(kSnoozeUsed), config.used);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_OriginalAvailabilityPreserved) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  const feature_engagement::Comparator availability = {
      feature_engagement::GREATER_THAN, 4};
  config.availability = availability;
  config.valid = true;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(availability, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_DefaultAvailability) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(kAny, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_AvailabilityFromSpec) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.set_initial_delay_days(kAdditionalConditionDelayDays);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  const feature_engagement::Comparator availability = {
      feature_engagement::GREATER_THAN_OR_EQUAL, kAdditionalConditionDelayDays};
  EXPECT_EQ(availability, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_UsedLimitDefaultAvailability) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.set_used_limit(kAdditionalConditionUsedLimit);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  const feature_engagement::Comparator comparator{
      feature_engagement::LESS_THAN_OR_EQUAL, kAdditionalConditionUsedLimit};
  EXPECT_EQ(comparator, config.used.comparator);
  EXPECT_EQ(kAtLeast7, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_UsedLimitCustomAvailability) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.set_used_limit(kAdditionalConditionUsedLimit);
  additional_conditions.set_initial_delay_days(kAdditionalConditionDelayDays);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  const feature_engagement::Comparator expected_used{
      feature_engagement::LESS_THAN_OR_EQUAL, kAdditionalConditionUsedLimit};
  const feature_engagement::Comparator expected_availability{
      feature_engagement::GREATER_THAN_OR_EQUAL, kAdditionalConditionDelayDays};
  EXPECT_EQ(expected_used, config.used.comparator);
  EXPECT_EQ(expected_availability, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_AddedConditionDefaultAvailability) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.AddAdditionalCondition(
      kAdditionalConditionName, AdditionalConditions::Constraint::kAtLeast,
      kAdditionalConditionCount, kAdditionalConditionDays);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  EXPECT_EQ(1U, config.event_configs.size());
  auto& event_config = *config.event_configs.begin();
  EXPECT_EQ(kAdditionalConditionName, event_config.name);
  EXPECT_EQ(kAdditionalConditionCount, event_config.comparator.value);
  EXPECT_EQ(feature_engagement::GREATER_THAN_OR_EQUAL,
            event_config.comparator.type);
  EXPECT_EQ(kAdditionalConditionDays, event_config.window);
  EXPECT_LE(kAdditionalConditionDays, event_config.storage);
  EXPECT_EQ(kAtLeast7, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_AddedConditionCustomAvailability) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.set_initial_delay_days(kAdditionalConditionDelayDays);
  additional_conditions.AddAdditionalCondition(
      kAdditionalConditionName, AdditionalConditions::Constraint::kAtLeast,
      kAdditionalConditionCount, kAdditionalConditionDays);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  EXPECT_EQ(1U, config.event_configs.size());
  auto& event_config = *config.event_configs.begin();
  EXPECT_EQ(kAdditionalConditionName, event_config.name);
  EXPECT_EQ(kAdditionalConditionCount, event_config.comparator.value);
  EXPECT_EQ(feature_engagement::GREATER_THAN_OR_EQUAL,
            event_config.comparator.type);
  EXPECT_EQ(kAdditionalConditionDays, event_config.window);
  EXPECT_LE(kAdditionalConditionDays, event_config.storage);
  const feature_engagement::Comparator expected_availability{
      feature_engagement::GREATER_THAN_OR_EQUAL, kAdditionalConditionDelayDays};
  EXPECT_EQ(expected_availability, config.availability);
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_AddedConditionInAddition) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.AddAdditionalCondition(
      kAdditionalConditionName, AdditionalConditions::Constraint::kAtLeast,
      kAdditionalConditionCount, kAdditionalConditionDays);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  feature_engagement::EventConfig event_config;
  event_config.name = kAdditionalCondition2Name;
  event_config.comparator = {feature_engagement::LESS_THAN_OR_EQUAL, 1};
  config.event_configs.insert(event_config);
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  EXPECT_EQ(2U, config.event_configs.size());
  for (const auto& cur_config : config.event_configs) {
    if (cur_config.name == event_config.name) {
      EXPECT_EQ(event_config.comparator, cur_config.comparator);
    } else {
      EXPECT_EQ(kAdditionalConditionName, cur_config.name);
      EXPECT_EQ(kAdditionalConditionCount, cur_config.comparator.value);
      EXPECT_EQ(feature_engagement::GREATER_THAN_OR_EQUAL,
                cur_config.comparator.type);
      EXPECT_EQ(kAdditionalConditionDays, cur_config.window);
      EXPECT_LE(kAdditionalConditionDays, cur_config.storage);
    }
  }
}

TEST_F(UserEducationConfigurationProviderTest,
       AdditionalConditions_AddedConditionNotOverridden) {
  SetEnableV2(true);
  auto spec = user_education::FeaturePromoSpecification::CreateForToastPromo(
      kIphWithConditionFeature, kTestElementId, IDS_CLOSE, IDS_CANCEL, {});
  AdditionalConditions additional_conditions;
  additional_conditions.AddAdditionalCondition(
      kAdditionalConditionName, AdditionalConditions::Constraint::kAtLeast,
      kAdditionalConditionCount, kAdditionalConditionDays);
  spec.SetAdditionalConditions(std::move(additional_conditions));

  feature_engagement::FeatureConfig config;
  feature_engagement::EventConfig event_config;
  event_config.name = kAdditionalConditionName;
  event_config.comparator = {feature_engagement::LESS_THAN_OR_EQUAL, 1};
  event_config.window = 4;
  event_config.storage = 8;
  config.event_configs.insert(event_config);
  EXPECT_TRUE(CreateProvider(std::move(spec))
                  ->MaybeProvideFeatureConfiguration(kIphWithConditionFeature,
                                                     config, kKnownFeatures,
                                                     kKnownGroups));

  EXPECT_EQ(1U, config.event_configs.size());
  const auto& new_config = *config.event_configs.begin();
  EXPECT_EQ(event_config.name, kAdditionalConditionName);
  EXPECT_EQ(event_config.comparator, new_config.comparator);
  EXPECT_EQ(event_config.window, new_config.window);
  EXPECT_LE(event_config.storage, new_config.storage);
}

TEST_F(UserEducationConfigurationProviderTest, v1_SessionRate) {
  SetEnableV2(false);
  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(feature_engagement::EQUAL, config.session_rate.type);
  EXPECT_EQ(0U, config.session_rate.value);
  EXPECT_EQ(feature_engagement::SessionRateImpact::Type::ALL,
            config.session_rate_impact.type);
}

TEST_F(UserEducationConfigurationProviderTest, v2_SessionRate) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(feature_engagement::ANY, config.session_rate.type);
  EXPECT_EQ(0U, config.session_rate.value);
  EXPECT_EQ(feature_engagement::SessionRateImpact::Type::ALL,
            config.session_rate_impact.type);
}

TEST_F(UserEducationConfigurationProviderTest, V1AllowsDuplicateTrigger) {
  SetEnableV2(false);
  feature_engagement::FeatureConfig config;
  config.trigger.name = kToastTrigger;
  config.used.name = kToastUsed;
  config.event_configs.emplace(kToastTrigger, kLessThan5, 10, 10);
  config.event_configs.emplace(kToastUsed, kAtLeast7, 10, 10);
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kToastIphFeature, config, kKnownFeatures, kKnownGroups));
  EXPECT_EQ(2U, config.event_configs.size());
}

TEST_F(UserEducationConfigurationProviderTest, V2RemovesDuplicateTrigger) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  config.trigger.name = kToastTrigger;
  config.used.name = kToastUsed;
  config.event_configs.emplace(kToastTrigger, kLessThan5, 10, 10);
  config.event_configs.emplace(kToastUsed, kAtLeast7, 10, 10);
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kToastIphFeature, config, kKnownFeatures, kKnownGroups));
  EXPECT_EQ(1U, config.event_configs.size());
  EXPECT_EQ(kToastUsed, config.event_configs.begin()->name);
}
