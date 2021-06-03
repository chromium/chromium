// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types_mojom_traits.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

// Test that every field in apps::mojom::App in correctly converted.
TEST(AppServiceTypesTraitsTest, RoundTrip) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  input->app_type = apps::mojom::AppType::kWeb;
  input->app_id = "abcdefg";
  input->readiness = apps::mojom::Readiness::kReady;
  input->name = "lacros test name";
  input->short_name = "lacros test name";
  input->publisher_id = "publisher_id";
  input->description = "description";
  input->version = "version";
  input->additional_search_terms = {"1", "2"};

  auto icon_key = apps::mojom::IconKey::New();
  icon_key->timeline = 1;
  icon_key->icon_effects = 2;
  input->icon_key = std::move(icon_key);

  input->last_launch_time = base::Time() + base::TimeDelta::FromDays(1);
  input->install_time = base::Time() + base::TimeDelta::FromDays(2);

  input->install_source = apps::mojom::InstallSource::kUser;
  input->recommendable = apps::mojom::OptionalBool::kTrue;
  input->searchable = apps::mojom::OptionalBool::kTrue;
  input->paused = apps::mojom::OptionalBool::kFalse;
  input->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  input->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  input->show_in_search = apps::mojom::OptionalBool::kTrue;
  input->show_in_management = apps::mojom::OptionalBool::kTrue;
  input->has_badge = apps::mojom::OptionalBool::kUnknown;
  input->paused = apps::mojom::OptionalBool::kFalse;

  auto intent_filter = apps::mojom::IntentFilter::New();
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, "https",
      apps::mojom::PatternMatchType::kNone, intent_filter);
  intent_filter->activity_name = "activity_name";
  intent_filter->activity_label = "activity_label";
  input->intent_filters.push_back(std::move(intent_filter));

  apps::mojom::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  EXPECT_EQ(output->app_type, apps::mojom::AppType::kWeb);
  EXPECT_EQ(output->app_id, "abcdefg");
  EXPECT_EQ(output->readiness, apps::mojom::Readiness::kReady);
  EXPECT_EQ(output->name, "lacros test name");
  EXPECT_EQ(output->short_name, "lacros test name");
  EXPECT_EQ(output->publisher_id, "publisher_id");
  EXPECT_EQ(output->description, "description");
  EXPECT_EQ(output->version, "version");
  EXPECT_EQ(output->additional_search_terms, input->additional_search_terms);

  EXPECT_EQ(output->icon_key->timeline, 1U);
  EXPECT_EQ(output->icon_key->icon_effects, 2U);

  EXPECT_EQ(output->last_launch_time,
            base::Time() + base::TimeDelta::FromDays(1));
  EXPECT_EQ(output->install_time, base::Time() + base::TimeDelta::FromDays(2));

  EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kUser);
  EXPECT_EQ(output->recommendable, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->searchable, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kFalse);
  EXPECT_EQ(output->show_in_launcher, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->show_in_shelf, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->show_in_search, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->show_in_management, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->has_badge, apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kFalse);

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 1U);
  auto& condition = filter->conditions[0];
  EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kScheme);
  ASSERT_EQ(condition->condition_values.size(), 1U);
  EXPECT_EQ(condition->condition_values[0]->value, "https");
  EXPECT_EQ(condition->condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kNone);
  EXPECT_EQ(filter->activity_name, "activity_name");
  EXPECT_EQ(filter->activity_label, "activity_label");
}

// Test that serialization and deserialization works with optional fields that
// doesn't fill up.
TEST(AppServiceTypesTraitsTest, RoundTripNoOptional) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  input->app_type = apps::mojom::AppType::kWeb;
  input->app_id = "abcdefg";
  input->readiness = apps::mojom::Readiness::kReady;
  input->additional_search_terms = {"1", "2"};

  input->install_source = apps::mojom::InstallSource::kUser;
  input->recommendable = apps::mojom::OptionalBool::kTrue;
  input->searchable = apps::mojom::OptionalBool::kTrue;
  input->paused = apps::mojom::OptionalBool::kFalse;
  input->show_in_launcher = apps::mojom::OptionalBool::kTrue;
  input->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  input->show_in_search = apps::mojom::OptionalBool::kTrue;
  input->show_in_management = apps::mojom::OptionalBool::kTrue;
  input->has_badge = apps::mojom::OptionalBool::kUnknown;
  input->paused = apps::mojom::OptionalBool::kFalse;

  auto intent_filter = apps::mojom::IntentFilter::New();
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kScheme, "https",
      apps::mojom::PatternMatchType::kNone, intent_filter);
  input->intent_filters.push_back(std::move(intent_filter));

  apps::mojom::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  EXPECT_EQ(output->app_type, apps::mojom::AppType::kWeb);
  EXPECT_EQ(output->app_id, "abcdefg");
  EXPECT_EQ(output->readiness, apps::mojom::Readiness::kReady);
  EXPECT_EQ(output->additional_search_terms, input->additional_search_terms);

  EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kUser);
  EXPECT_EQ(output->recommendable, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->searchable, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kFalse);
  EXPECT_EQ(output->show_in_launcher, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->show_in_shelf, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->show_in_search, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->show_in_management, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->has_badge, apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kFalse);

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 1U);
  auto& condition = filter->conditions[0];
  EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kScheme);
  ASSERT_EQ(condition->condition_values.size(), 1U);
  EXPECT_EQ(condition->condition_values[0]->value, "https");
  EXPECT_EQ(condition->condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kNone);
}

// Test that serialization and deserialization works with updating app type.
TEST(AppServiceTypesTraitsTest, RoundTripAppType) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->app_type = apps::mojom::AppType::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::mojom::AppType::kUnknown);
  }
  {
    input->app_type = apps::mojom::AppType::kArc;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::mojom::AppType::kArc);
  }
  {
    input->app_type = apps::mojom::AppType::kWeb;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::mojom::AppType::kWeb);
  }
  {
    input->app_type = apps::mojom::AppType::kSystemWeb;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::mojom::AppType::kSystemWeb);
  }
}

// Test that serialization and deserialization works with updating readiness.
TEST(AppServiceTypesTraitsTest, RoundTripReadiness) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->readiness = apps::mojom::Readiness::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kUnknown);
  }
  {
    input->readiness = apps::mojom::Readiness::kReady;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kReady);
  }
  {
    input->readiness = apps::mojom::Readiness::kDisabledByBlocklist;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kDisabledByBlocklist);
  }
  {
    input->readiness = apps::mojom::Readiness::kDisabledByPolicy;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kDisabledByPolicy);
  }
  {
    input->readiness = apps::mojom::Readiness::kDisabledByUser;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kDisabledByUser);
  }
  {
    input->readiness = apps::mojom::Readiness::kTerminated;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kTerminated);
  }
  {
    input->readiness = apps::mojom::Readiness::kUninstalledByUser;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kUninstalledByUser);
  }
  {
    input->readiness = apps::mojom::Readiness::kRemoved;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::mojom::Readiness::kRemoved);
  }
}

// Test that serialization and deserialization works with updating install
// source.
TEST(AppServiceTypesTraitsTest, RoundTripInstallSource) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->install_source = apps::mojom::InstallSource::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kUnknown);
  }
  {
    input->install_source = apps::mojom::InstallSource::kSystem;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kSystem);
  }
  {
    input->install_source = apps::mojom::InstallSource::kPolicy;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kPolicy);
  }
  {
    input->install_source = apps::mojom::InstallSource::kOem;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kOem);
  }
  {
    input->install_source = apps::mojom::InstallSource::kDefault;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kDefault);
  }
  {
    input->install_source = apps::mojom::InstallSource::kSync;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kSync);
  }
  {
    input->install_source = apps::mojom::InstallSource::kUser;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_source, apps::mojom::InstallSource::kUser);
  }
}

// Test that serialization and deserialization works with updating
// recommendable.
TEST(AppServiceTypesTraitsTest, RoundTripRecommendable) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->recommendable = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->recommendable, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->recommendable = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->recommendable, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->recommendable = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->recommendable, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating searchable.
TEST(AppServiceTypesTraitsTest, RoundTripSearchable) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->searchable = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->searchable, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->searchable = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->searchable, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->searchable = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->searchable, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating
// show_in_launcher.
TEST(AppServiceTypesTraitsTest, RoundTripShowInLauncher) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->show_in_launcher = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_launcher, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->show_in_launcher = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_launcher, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->show_in_launcher = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_launcher, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating
// show_in_shelf.
TEST(AppServiceTypesTraitsTest, RoundTripShowInShelf) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->show_in_shelf = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_shelf, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->show_in_shelf = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_shelf, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->show_in_shelf = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_shelf, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating
// show_in_search.
TEST(AppServiceTypesTraitsTest, RoundTripShowInSearch) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->show_in_search = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_search, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->show_in_search = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_search, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->show_in_search = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_search, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating
// show_in_management.
TEST(AppServiceTypesTraitsTest, RoundTripShowInManagement) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->show_in_management = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_management, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->show_in_management = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_management, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->show_in_management = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->show_in_management, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating has_badge.
TEST(AppServiceTypesTraitsTest, RoundTripHasBadge) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->has_badge = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->has_badge, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->has_badge = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->has_badge, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->has_badge = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->has_badge, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating paused.
TEST(AppServiceTypesTraitsTest, RoundTripPaused) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->paused = apps::mojom::OptionalBool::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kUnknown);
  }
  {
    input->paused = apps::mojom::OptionalBool::kFalse;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kFalse);
  }
  {
    input->paused = apps::mojom::OptionalBool::kTrue;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->paused, apps::mojom::OptionalBool::kTrue);
  }
}

// Test that serialization and deserialization works with updating
// intent_filters.
TEST(AppServiceTypesTraitsTest, RoundTripIntentFilters) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  auto intent_filter = apps::mojom::IntentFilter::New();
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kScheme, "1",
                                     apps::mojom::PatternMatchType::kNone,
                                     intent_filter);
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kHost, "2",
                                     apps::mojom::PatternMatchType::kLiteral,
                                     intent_filter);
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kPattern, "3",
                                     apps::mojom::PatternMatchType::kPrefix,
                                     intent_filter);
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kAction, "4",
                                     apps::mojom::PatternMatchType::kGlob,
                                     intent_filter);
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kMimeType, "5",
                                     apps::mojom::PatternMatchType::kMimeType,
                                     intent_filter);
  input->intent_filters.push_back(std::move(intent_filter));

  apps::mojom::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 5U);
  {
    auto& condition = filter->conditions[0];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kScheme);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kNone);
    EXPECT_EQ(condition->condition_values[0]->value, "1");
  }
  {
    auto& condition = filter->conditions[1];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kHost);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kLiteral);
    EXPECT_EQ(condition->condition_values[0]->value, "2");
  }
  {
    auto& condition = filter->conditions[2];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kPattern);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kPrefix);
    EXPECT_EQ(condition->condition_values[0]->value, "3");
  }
  {
    auto& condition = filter->conditions[3];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kAction);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kGlob);
    EXPECT_EQ(condition->condition_values[0]->value, "4");
  }
  {
    auto& condition = filter->conditions[4];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kMimeType);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[0]->value, "5");
  }
}

// Test that serialization and deserialization works with uninstall source.
TEST(AppServiceTypesTraitsTest, RoundTripUninstallSource) {
  apps::mojom::UninstallSource input;
  {
    input = apps::mojom::UninstallSource::kUnknown;
    apps::mojom::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::mojom::UninstallSource::kUnknown);
  }
  {
    input = apps::mojom::UninstallSource::kAppList;
    apps::mojom::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::mojom::UninstallSource::kAppList);
  }
  {
    input = apps::mojom::UninstallSource::kAppManagement;
    apps::mojom::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::mojom::UninstallSource::kAppManagement);
  }
  {
    input = apps::mojom::UninstallSource::kShelf;
    apps::mojom::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::mojom::UninstallSource::kShelf);
  }
  {
    input = apps::mojom::UninstallSource::kMigration;
    apps::mojom::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::mojom::UninstallSource::kMigration);
  }
}

// Test that serialization and deserialization works with icon type.
TEST(AppServiceTypesTraitsTest, RoundTripIconType) {
  apps::mojom::IconType input;
  {
    input = apps::mojom::IconType::kUnknown;
    apps::mojom::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::mojom::IconType::kUnknown);
  }
  {
    input = apps::mojom::IconType::kUncompressed;
    apps::mojom::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::mojom::IconType::kUncompressed);
  }
  {
    input = apps::mojom::IconType::kCompressed;
    apps::mojom::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::mojom::IconType::kCompressed);
  }
  {
    input = apps::mojom::IconType::kStandard;
    apps::mojom::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::mojom::IconType::kStandard);
  }
}

// Test that serialization and deserialization works with icon value.
TEST(AppServiceTypesTraitsTest, RoundTripIconValue) {
  {
    auto input = apps::mojom::IconValue::New();
    input->icon_type = apps::mojom::IconType::kUnknown;

    std::vector<float> scales;
    scales.push_back(1.0f);
    gfx::ImageSkia::SetSupportedScales(scales);

    gfx::ImageSkia image = gfx::test::CreateImageSkia(1, 2);
    input->uncompressed = image;

    input->compressed = {1u, 2u};
    input->is_placeholder_icon = true;

    apps::mojom::IconValuePtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconValue>(
        input, output));

    EXPECT_EQ(output->icon_type, apps::mojom::IconType::kUnknown);
    EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(output->uncompressed),
                                          gfx::Image(image)));
    EXPECT_EQ(output->compressed, std::vector<uint8_t>({1u, 2u}));
    EXPECT_TRUE(output->is_placeholder_icon);
  }
  {
    auto input = apps::mojom::IconValue::New();
    input->icon_type = apps::mojom::IconType::kUncompressed;

    std::vector<float> scales;
    scales.push_back(1.0f);
    gfx::ImageSkia::SetSupportedScales(scales);

    gfx::ImageSkia image = gfx::test::CreateImageSkia(3, 4);
    input->uncompressed = image;
    input->is_placeholder_icon = false;

    apps::mojom::IconValuePtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconValue>(
        input, output));

    EXPECT_EQ(output->icon_type, apps::mojom::IconType::kUncompressed);
    EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(output->uncompressed),
                                          gfx::Image(image)));
    EXPECT_FALSE(output->is_placeholder_icon);
  }
  {
    auto input = apps::mojom::IconValue::New();
    input->icon_type = apps::mojom::IconType::kCompressed;

    input->compressed = {3u, 4u};
    input->is_placeholder_icon = true;

    apps::mojom::IconValuePtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconValue>(
        input, output));

    EXPECT_EQ(output->icon_type, apps::mojom::IconType::kCompressed);
    EXPECT_EQ(output->compressed, std::vector<uint8_t>({3u, 4u}));
    EXPECT_TRUE(output->is_placeholder_icon);
  }
}
