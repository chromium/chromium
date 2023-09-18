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
constexpr char kPerAppTrigger[] = "PerAppIphFeature_trigger";
constexpr char kPerAppUsed[] = "PerAppIphFeature_used";
constexpr char kLegalNoticeTrigger[] = "LegalNoticeIphFeature_trigger";
constexpr char kLegalNoticeUsed[] = "LegalNoticeIphFeature_used";
BASE_FEATURE(kToastIphFeature,
             "IPH_ToastIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSnoozeIphFeature,
             "IPH_SnoozeIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kPerAppIphFeature,
             "IPH_PerAppIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeIphFeature,
             "IPH_LegalNoticeIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
const std::initializer_list<const base::Feature*> kKnownFeatures{
    &kToastIphFeature, &kSnoozeIphFeature, &kPerAppIphFeature,
    &kLegalNoticeIphFeature};
const std::initializer_list<const base::Feature*> kKnownGroups{};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);

// Provider must be created after possibly setting v2 feature flag.
//
// Call this only when the provider is actually needed for the test, and after
// SetEnableV2 (if necessary).
std::unique_ptr<UserEducationConfigurationProvider> CreateProvider() {
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
      kPerAppIphFeature, kTestElementId, IDS_CANCEL, IDS_OK, base::DoNothing());
  spec.set_promo_subtype_for_testing(
      user_education::FeaturePromoSpecification::PromoSubtype::kPerApp);
  registry.RegisterFeature(std::move(spec));

  spec = user_education::FeaturePromoSpecification::CreateForCustomAction(
      kLegalNoticeIphFeature, kTestElementId, IDS_CLEAR, IDS_CLOSE,
      base::DoNothing());
  spec.set_promo_subtype_for_testing(
      user_education::FeaturePromoSpecification::PromoSubtype::kLegalNotice);
  registry.RegisterFeature(std::move(spec));

  return std::make_unique<UserEducationConfigurationProvider>(
      std::move(registry));
}

}  // namespace

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
    return feature_engagement::EventConfig(
        name, kLessThan3, feature_engagement::kMaxStoragePeriod,
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
  const feature_engagement::Comparator kLessThan3{feature_engagement::LESS_THAN,
                                                  3};
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

TEST_F(UserEducationConfigurationProviderTest, ProvidesPerAppConfiguration) {
  feature_engagement::FeatureConfig config;

  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kPerAppIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_TRUE(config.valid);

  EXPECT_EQ(GetDefaultUsed(kPerAppUsed), config.used);

  EXPECT_EQ(GetAnyTrigger(kPerAppTrigger), config.trigger);

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
      98, 97);
  config.trigger = trigger;
  config.used = used;
  config.valid = true;
  EXPECT_FALSE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(trigger, config.trigger);
  EXPECT_EQ(used, config.used);
}

TEST_F(UserEducationConfigurationProviderTest, v2_DoesOverwriteValid) {
  SetEnableV2(true);
  feature_engagement::FeatureConfig config;
  const feature_engagement::EventConfig trigger(
      "foo", feature_engagement::Comparator(feature_engagement::LESS_THAN, 10),
      100, 99);
  const feature_engagement::EventConfig used(
      "bar", feature_engagement::Comparator(feature_engagement::LESS_THAN, 8),
      98, 97);
  config.trigger = trigger;
  config.used = used;
  config.valid = true;
  EXPECT_TRUE(CreateProvider()->MaybeProvideFeatureConfiguration(
      kSnoozeIphFeature, config, kKnownFeatures, kKnownGroups));

  EXPECT_EQ(GetDefaultTrigger("foo"), config.trigger);
  EXPECT_EQ(GetDefaultUsed("bar"), config.used);
}
