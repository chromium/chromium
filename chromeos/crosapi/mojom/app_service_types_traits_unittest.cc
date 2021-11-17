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

  input->last_launch_time = base::Time() + base::Days(1);
  input->install_time = base::Time() + base::Days(2);

  input->install_reason = apps::mojom::InstallReason::kUser;
  input->policy_id = "https://app.site/alpha";
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

  input->window_mode = apps::mojom::WindowMode::kWindow;

  auto permission = apps::mojom::Permission::New();
  permission->permission_type = apps::mojom::PermissionType::kCamera;
  permission->value = apps::mojom::PermissionValue::New();
  permission->value->set_bool_value(true);
  permission->is_managed = true;
  input->permissions.push_back(std::move(permission));

  input->allow_uninstall = apps::mojom::OptionalBool::kTrue;
  input->handles_intents = apps::mojom::OptionalBool::kTrue;

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

  EXPECT_EQ(output->last_launch_time, base::Time() + base::Days(1));
  EXPECT_EQ(output->install_time, base::Time() + base::Days(2));

  EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kUser);
  EXPECT_EQ(output->policy_id, "https://app.site/alpha");
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

  EXPECT_EQ(output->window_mode, apps::mojom::WindowMode::kWindow);

  ASSERT_EQ(output->permissions.size(), 1U);
  auto& out_permission = output->permissions[0];
  EXPECT_EQ(out_permission->permission_type,
            apps::mojom::PermissionType::kCamera);
  ASSERT_TRUE(out_permission->value->is_bool_value());
  EXPECT_TRUE(out_permission->value->get_bool_value());
  EXPECT_TRUE(out_permission->is_managed);

  EXPECT_EQ(output->allow_uninstall, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->handles_intents, apps::mojom::OptionalBool::kTrue);
}

// Test that serialization and deserialization works with optional fields that
// doesn't fill up.
TEST(AppServiceTypesTraitsTest, RoundTripNoOptional) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  input->app_type = apps::mojom::AppType::kWeb;
  input->app_id = "abcdefg";
  input->readiness = apps::mojom::Readiness::kReady;
  input->additional_search_terms = {"1", "2"};

  input->install_reason = apps::mojom::InstallReason::kUser;
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
  input->window_mode = apps::mojom::WindowMode::kBrowser;
  input->allow_uninstall = apps::mojom::OptionalBool::kTrue;
  input->handles_intents = apps::mojom::OptionalBool::kTrue;

  apps::mojom::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  EXPECT_EQ(output->app_type, apps::mojom::AppType::kWeb);
  EXPECT_EQ(output->app_id, "abcdefg");
  EXPECT_EQ(output->readiness, apps::mojom::Readiness::kReady);
  EXPECT_EQ(output->additional_search_terms, input->additional_search_terms);

  EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kUser);
  EXPECT_FALSE(output->policy_id.has_value());
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

  EXPECT_EQ(output->window_mode, apps::mojom::WindowMode::kBrowser);
  EXPECT_EQ(output->allow_uninstall, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(output->handles_intents, apps::mojom::OptionalBool::kTrue);
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
// reason.
TEST(AppServiceTypesTraitsTest, RoundTripInstallReason) {
  apps::mojom::AppPtr input = apps::mojom::App::New();
  {
    input->install_reason = apps::mojom::InstallReason::kUnknown;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kUnknown);
  }
  {
    input->install_reason = apps::mojom::InstallReason::kSystem;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kSystem);
  }
  {
    input->install_reason = apps::mojom::InstallReason::kPolicy;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kPolicy);
  }
  {
    input->install_reason = apps::mojom::InstallReason::kOem;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kOem);
  }
  {
    input->install_reason = apps::mojom::InstallReason::kDefault;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kDefault);
  }
  {
    input->install_reason = apps::mojom::InstallReason::kSync;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kSync);
  }
  {
    input->install_reason = apps::mojom::InstallReason::kUser;
    apps::mojom::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::mojom::InstallReason::kUser);
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
  apps_util::AddSingleValueCondition(apps::mojom::ConditionType::kFile, "6",
                                     apps::mojom::PatternMatchType::kMimeType,
                                     intent_filter);
  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kFile, "7",
      apps::mojom::PatternMatchType::kFileExtension, intent_filter);
  input->intent_filters.push_back(std::move(intent_filter));

  apps::mojom::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 7U);
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
  {
    auto& condition = filter->conditions[5];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[0]->value, "6");
  }
  {
    auto& condition = filter->conditions[6];
    EXPECT_EQ(condition->condition_type, apps::mojom::ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::mojom::PatternMatchType::kFileExtension);
    EXPECT_EQ(condition->condition_values[0]->value, "7");
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
  apps::IconType input;
  {
    input = apps::IconType::kUnknown;
    apps::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::IconType::kUnknown);
  }
  {
    input = apps::IconType::kUncompressed;
    apps::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::IconType::kUncompressed);
  }
  {
    input = apps::IconType::kCompressed;
    apps::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::IconType::kCompressed);
  }
  {
    input = apps::IconType::kStandard;
    apps::IconType output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconType>(
        input, output));
    EXPECT_EQ(output, apps::IconType::kStandard);
  }
}

// Test that serialization and deserialization works with icon value.
TEST(AppServiceTypesTraitsTest, RoundTripIconValue) {
  {
    auto input = std::make_unique<apps::IconValue>();
    input->icon_type = apps::IconType::kUnknown;

    std::vector<float> scales;
    scales.push_back(1.0f);
    gfx::ImageSkia::SetSupportedScales(scales);

    gfx::ImageSkia image = gfx::test::CreateImageSkia(1, 2);
    input->uncompressed = image;

    input->compressed = {1u, 2u};
    input->is_placeholder_icon = true;

    auto output = std::make_unique<apps::IconValue>();
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconValue>(
        input, output));

    EXPECT_EQ(output->icon_type, apps::IconType::kUnknown);
    EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(output->uncompressed),
                                          gfx::Image(image)));
    EXPECT_EQ(output->compressed, std::vector<uint8_t>({1u, 2u}));
    EXPECT_TRUE(output->is_placeholder_icon);
  }
  {
    auto input = std::make_unique<apps::IconValue>();
    input->icon_type = apps::IconType::kUncompressed;

    std::vector<float> scales;
    scales.push_back(1.0f);
    gfx::ImageSkia::SetSupportedScales(scales);

    gfx::ImageSkia image = gfx::test::CreateImageSkia(3, 4);
    input->uncompressed = image;
    input->is_placeholder_icon = false;

    auto output = std::make_unique<apps::IconValue>();
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconValue>(
        input, output));

    EXPECT_EQ(output->icon_type, apps::IconType::kUncompressed);
    EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(output->uncompressed),
                                          gfx::Image(image)));
    EXPECT_FALSE(output->is_placeholder_icon);
  }
  {
    auto input = std::make_unique<apps::IconValue>();
    input->icon_type = apps::IconType::kCompressed;

    input->compressed = {3u, 4u};
    input->is_placeholder_icon = true;

    auto output = std::make_unique<apps::IconValue>();
    ;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconValue>(
        input, output));

    EXPECT_EQ(output->icon_type, apps::IconType::kCompressed);
    EXPECT_EQ(output->compressed, std::vector<uint8_t>({3u, 4u}));
    EXPECT_TRUE(output->is_placeholder_icon);
  }
}

// Test that serialization and deserialization works with window mode.
TEST(AppServiceTypesTraitsTest, RoundTripWindowMode) {
  apps::mojom::WindowMode input;
  {
    input = apps::mojom::WindowMode::kUnknown;
    apps::mojom::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::mojom::WindowMode::kUnknown);
  }
  {
    input = apps::mojom::WindowMode::kWindow;
    apps::mojom::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::mojom::WindowMode::kWindow);
  }
  {
    input = apps::mojom::WindowMode::kBrowser;
    apps::mojom::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::mojom::WindowMode::kBrowser);
  }
  {
    input = apps::mojom::WindowMode::kTabbedWindow;
    apps::mojom::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::mojom::WindowMode::kTabbedWindow);
  }
}

// Test that serialization and deserialization works with launch source.
TEST(AppServiceTypesTraitsTest, RoundTripLaunchSource) {
  apps::mojom::LaunchSource input;
  {
    input = apps::mojom::LaunchSource::kUnknown;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromAppListGrid;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromAppListGridContextMenu;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromAppListQuery;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromAppListQueryContextMenu;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromAppListRecommendation;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromParentalControls;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromShelf;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromFileManager;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromLink;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromOmnibox;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromChromeInternal;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromKeyboard;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromOtherApp;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromMenu;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromInstalledNotification;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromTest;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromArc;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromSharesheet;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromReleaseNotesNotification;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromFullRestore;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromSmartTextContextMenu;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::mojom::LaunchSource::kFromDiscoverTabNotification;
    apps::mojom::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
}

TEST(AppServiceTypesTraitsTest, RoundTripPermissions) {
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kUnknown;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_bool_value(true);
    permission->is_managed = false;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kCamera;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_bool_value(false);
    permission->is_managed = true;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kLocation;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_tristate_value(apps::mojom::TriState::kAllow);
    permission->is_managed = false;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kMicrophone;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_tristate_value(apps::mojom::TriState::kBlock);
    permission->is_managed = true;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kNotifications;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_tristate_value(apps::mojom::TriState::kAsk);
    permission->is_managed = false;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kContacts;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_tristate_value(apps::mojom::TriState::kAllow);
    permission->is_managed = true;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
  {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type = apps::mojom::PermissionType::kStorage;
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_tristate_value(apps::mojom::TriState::kBlock);
    permission->is_managed = false;
    apps::mojom::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(permission->permission_type, output->permission_type);
    EXPECT_EQ(permission->value, output->value);
    EXPECT_EQ(permission->is_managed, output->is_managed);
  }
}
