// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types_mojom_traits.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/shortcut.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

// Test that every field in apps::App in correctly converted.
TEST(AppServiceTypesMojomTraitsTest, RoundTrip) {
  auto input = std::make_unique<apps::App>(apps::AppType::kWeb, "abcdefg");
  input->readiness = apps::Readiness::kReady;
  input->name = "lacros test name";
  input->short_name = "lacros test name";
  input->publisher_id = "publisher_id";
  input->description = "description";
  input->version = "version";
  input->additional_search_terms = {"1", "2"};
  input->icon_key = apps::IconKey(
      /*timeline=*/1, apps::IconKey::kInvalidResourceId, /*icon_effects=*/2);
  input->last_launch_time = base::Time() + base::Days(1);
  input->install_time = base::Time() + base::Days(2);
  input->install_reason = apps::InstallReason::kUser;
  input->policy_id = "https://app.site/alpha";
  input->recommendable = true;
  input->searchable = true;
  input->show_in_launcher = true;
  input->show_in_shelf = true;
  input->show_in_search = true;
  input->show_in_management = true;
  input->has_badge = absl::nullopt;
  input->paused = false;

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "https",
                                         apps::PatternMatchType::kNone);
  intent_filter->activity_name = "activity_name";
  intent_filter->activity_label = "activity_label";
  input->intent_filters.push_back(std::move(intent_filter));

  input->window_mode = apps::WindowMode::kWindow;

  input->permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kCamera,
      std::make_unique<apps::PermissionValue>(/*bool_value=*/true),
      /*is_managed=*/true));

  input->allow_uninstall = true;
  input->handles_intents = true;

  input->shortcuts.push_back(
      std::make_unique<apps::Shortcut>("test_id", "test_name", /*position*/ 1));

  input->is_platform_app = true;

  apps::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  EXPECT_EQ(output->app_type, apps::AppType::kWeb);
  EXPECT_EQ(output->app_id, "abcdefg");
  EXPECT_EQ(output->readiness, apps::Readiness::kReady);
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

  EXPECT_EQ(output->install_reason, apps::InstallReason::kUser);
  EXPECT_EQ(output->policy_id, "https://app.site/alpha");
  EXPECT_TRUE(output->recommendable.value());
  EXPECT_TRUE(output->searchable.value());
  EXPECT_TRUE(output->show_in_launcher.value());
  EXPECT_TRUE(output->show_in_shelf.value());
  EXPECT_TRUE(output->show_in_search.value());
  EXPECT_TRUE(output->show_in_management.value());
  EXPECT_FALSE(output->has_badge.has_value());
  EXPECT_FALSE(output->paused.value());

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 1U);
  auto& condition = filter->conditions[0];
  EXPECT_EQ(condition->condition_type, apps::ConditionType::kScheme);
  ASSERT_EQ(condition->condition_values.size(), 1U);
  EXPECT_EQ(condition->condition_values[0]->value, "https");
  EXPECT_EQ(condition->condition_values[0]->match_type,
            apps::PatternMatchType::kNone);
  EXPECT_EQ(filter->activity_name, "activity_name");
  EXPECT_EQ(filter->activity_label, "activity_label");

  EXPECT_EQ(output->window_mode, apps::WindowMode::kWindow);

  ASSERT_EQ(output->permissions.size(), 1U);
  auto& out_permission = output->permissions[0];
  EXPECT_EQ(out_permission->permission_type, apps::PermissionType::kCamera);
  ASSERT_TRUE(out_permission->value->bool_value.has_value());
  EXPECT_TRUE(out_permission->value->bool_value.value());
  EXPECT_TRUE(out_permission->is_managed);

  EXPECT_TRUE(output->allow_uninstall.value());
  EXPECT_TRUE(output->handles_intents.value());

  ASSERT_EQ(output->shortcuts.size(), 1U);
  auto& shortcut = output->shortcuts[0];
  EXPECT_EQ(shortcut->shortcut_id, "test_id");
  EXPECT_EQ(shortcut->name, "test_name");
  EXPECT_EQ(shortcut->position, 1);

  EXPECT_TRUE(output->is_platform_app.value());
}

// Test that serialization and deserialization works with optional fields that
// doesn't fill up.
TEST(AppServiceTypesMojomTraitsTest, RoundTripNoOptional) {
  auto input = std::make_unique<apps::App>(apps::AppType::kWeb, "abcdefg");
  input->readiness = apps::Readiness::kReady;
  input->additional_search_terms = {"1", "2"};

  input->install_reason = apps::InstallReason::kUser;
  input->recommendable = true;
  input->searchable = true;
  input->show_in_launcher = true;
  input->show_in_shelf = true;
  input->show_in_search = true;
  input->show_in_management = true;
  input->has_badge = absl::nullopt;
  input->paused = false;

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "https",
                                         apps::PatternMatchType::kNone);
  input->intent_filters.push_back(std::move(intent_filter));
  input->window_mode = apps::WindowMode::kBrowser;
  input->allow_uninstall = true;
  input->handles_intents = true;
  input->is_platform_app = absl::nullopt;

  apps::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  EXPECT_EQ(output->app_type, apps::AppType::kWeb);
  EXPECT_EQ(output->app_id, "abcdefg");
  EXPECT_EQ(output->readiness, apps::Readiness::kReady);
  EXPECT_EQ(output->additional_search_terms, input->additional_search_terms);

  EXPECT_EQ(output->install_reason, apps::InstallReason::kUser);
  EXPECT_FALSE(output->policy_id.has_value());
  EXPECT_TRUE(output->recommendable.value());
  EXPECT_TRUE(output->searchable.value());
  EXPECT_TRUE(output->show_in_launcher.value());
  EXPECT_TRUE(output->show_in_shelf.value());
  EXPECT_TRUE(output->show_in_search.value());
  EXPECT_TRUE(output->show_in_management.value());
  EXPECT_FALSE(output->has_badge.has_value());
  EXPECT_FALSE(output->paused.value());

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 1U);
  auto& condition = filter->conditions[0];
  EXPECT_EQ(condition->condition_type, apps::ConditionType::kScheme);
  ASSERT_EQ(condition->condition_values.size(), 1U);
  EXPECT_EQ(condition->condition_values[0]->value, "https");
  EXPECT_EQ(condition->condition_values[0]->match_type,
            apps::PatternMatchType::kNone);

  EXPECT_EQ(output->window_mode, apps::WindowMode::kBrowser);
  EXPECT_TRUE(output->allow_uninstall);
  EXPECT_TRUE(output->handles_intents);
  EXPECT_FALSE(output->is_platform_app.has_value());
}

// Test that serialization and deserialization works with updating app type.
TEST(AppServiceTypesMojomTraitsTest, RoundTripAppType) {
  {
    auto input =
        std::make_unique<apps::App>(apps::AppType::kUnknown, "abcdefg");
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::AppType::kUnknown);
  }
  {
    auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::AppType::kArc);
  }
  {
    auto input = std::make_unique<apps::App>(apps::AppType::kWeb, "abcdefg");
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::AppType::kWeb);
  }
  {
    auto input =
        std::make_unique<apps::App>(apps::AppType::kSystemWeb, "abcdefg");
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->app_type, apps::AppType::kSystemWeb);
  }
}

// Test that serialization and deserialization works with updating readiness.
TEST(AppServiceTypesMojomTraitsTest, RoundTripReadiness) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->readiness = apps::Readiness::kUnknown;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kUnknown);
  }
  {
    input->readiness = apps::Readiness::kReady;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kReady);
  }
  {
    input->readiness = apps::Readiness::kDisabledByBlocklist;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kDisabledByBlocklist);
  }
  {
    input->readiness = apps::Readiness::kDisabledByPolicy;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kDisabledByPolicy);
  }
  {
    input->readiness = apps::Readiness::kDisabledByUser;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kDisabledByUser);
  }
  {
    input->readiness = apps::Readiness::kTerminated;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kTerminated);
  }
  {
    input->readiness = apps::Readiness::kUninstalledByUser;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kUninstalledByUser);
  }
  {
    input->readiness = apps::Readiness::kRemoved;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));

    EXPECT_EQ(output->readiness, apps::Readiness::kRemoved);
  }
}

// Test that serialization and deserialization works with updating install
// reason.
TEST(AppServiceTypesMojomTraitsTest, RoundTripInstallReason) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->install_reason = apps::InstallReason::kUnknown;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kUnknown);
  }
  {
    input->install_reason = apps::InstallReason::kSystem;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kSystem);
  }
  {
    input->install_reason = apps::InstallReason::kPolicy;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kPolicy);
  }
  {
    input->install_reason = apps::InstallReason::kOem;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kOem);
  }
  {
    input->install_reason = apps::InstallReason::kDefault;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kDefault);
  }
  {
    input->install_reason = apps::InstallReason::kSync;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kSync);
  }
  {
    input->install_reason = apps::InstallReason::kUser;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kUser);
  }
}

// Test that serialization and deserialization works with updating
// recommendable.
TEST(AppServiceTypesMojomTraitsTest, RoundTripRecommendable) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->recommendable = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->recommendable.has_value());
  }
  {
    input->recommendable = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->recommendable.value());
  }
  {
    input->recommendable = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->recommendable.value());
  }
}

// Test that serialization and deserialization works with updating searchable.
TEST(AppServiceTypesMojomTraitsTest, RoundTripSearchable) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->searchable = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->searchable.has_value());
  }
  {
    input->searchable = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->searchable.value());
  }
  {
    input->searchable = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->searchable.value());
  }
}

// Test that serialization and deserialization works with updating
// show_in_launcher.
TEST(AppServiceTypesMojomTraitsTest, RoundTripShowInLauncher) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->show_in_launcher = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_launcher.has_value());
  }
  {
    input->show_in_launcher = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_launcher.value());
  }
  {
    input->show_in_launcher = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->show_in_launcher.value());
  }
}

// Test that serialization and deserialization works with updating
// show_in_shelf.
TEST(AppServiceTypesMojomTraitsTest, RoundTripShowInShelf) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->show_in_shelf = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_shelf.has_value());
  }
  {
    input->show_in_shelf = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_shelf.value());
  }
  {
    input->show_in_shelf = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->show_in_shelf.value());
  }
}

// Test that serialization and deserialization works with updating
// show_in_search.
TEST(AppServiceTypesMojomTraitsTest, RoundTripShowInSearch) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->show_in_search = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_search.has_value());
  }
  {
    input->show_in_search = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_search.value());
  }
  {
    input->show_in_search = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->show_in_search.value());
  }
}

// Test that serialization and deserialization works with updating
// show_in_management.
TEST(AppServiceTypesMojomTraitsTest, RoundTripShowInManagement) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->show_in_management = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_management.has_value());
  }
  {
    input->show_in_management = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->show_in_management.value());
  }
  {
    input->show_in_management = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->show_in_management.value());
  }
}

// Test that serialization and deserialization works with updating has_badge.
TEST(AppServiceTypesMojomTraitsTest, RoundTripHasBadge) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->has_badge = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->has_badge.has_value());
  }
  {
    input->has_badge = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->has_badge.value());
  }
  {
    input->has_badge = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->has_badge.value());
  }
}

// Test that serialization and deserialization works with updating paused.
TEST(AppServiceTypesMojomTraitsTest, RoundTripPaused) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->paused = absl::nullopt;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->paused.has_value());
  }
  {
    input->paused = false;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_FALSE(output->paused.value());
  }
  {
    input->paused = true;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_TRUE(output->paused.value());
  }
}

// Test that serialization and deserialization works with updating
// intent_filters.
TEST(AppServiceTypesMojomTraitsTest, RoundTripIntentFilters) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "1",
                                         apps::PatternMatchType::kNone);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost, "2",
                                         apps::PatternMatchType::kLiteral);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kPattern, "3",
                                         apps::PatternMatchType::kPrefix);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction, "4",
                                         apps::PatternMatchType::kGlob);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kMimeType, "5",
                                         apps::PatternMatchType::kMimeType);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kFile, "6",
                                         apps::PatternMatchType::kMimeType);
  intent_filter->AddSingleValueCondition(
      apps::ConditionType::kFile, "7", apps::PatternMatchType::kFileExtension);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kHost, "8",
                                         apps::PatternMatchType::kSuffix);
  input->intent_filters.push_back(std::move(intent_filter));

  apps::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 8U);
  {
    auto& condition = filter->conditions[0];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kScheme);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kNone);
    EXPECT_EQ(condition->condition_values[0]->value, "1");
  }
  {
    auto& condition = filter->conditions[1];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kHost);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition->condition_values[0]->value, "2");
  }
  {
    auto& condition = filter->conditions[2];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kPattern);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kPrefix);
    EXPECT_EQ(condition->condition_values[0]->value, "3");
  }
  {
    auto& condition = filter->conditions[3];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kAction);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kGlob);
    EXPECT_EQ(condition->condition_values[0]->value, "4");
  }
  {
    auto& condition = filter->conditions[4];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kMimeType);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[0]->value, "5");
  }
  {
    auto& condition = filter->conditions[5];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[0]->value, "6");
  }
  {
    auto& condition = filter->conditions[6];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kFileExtension);
    EXPECT_EQ(condition->condition_values[0]->value, "7");
  }
  {
    auto& condition = filter->conditions[7];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kHost);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kSuffix);
    EXPECT_EQ(condition->condition_values[0]->value, "8");
  }
}

// Test that serialization and deserialization works with uninstall source.
TEST(AppServiceTypesMojomTraitsTest, RoundTripUninstallSource) {
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
TEST(AppServiceTypesMojomTraitsTest, RoundTripIconType) {
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
TEST(AppServiceTypesMojomTraitsTest, RoundTripIconValue) {
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
TEST(AppServiceTypesMojomTraitsTest, RoundTripWindowMode) {
  apps::WindowMode input;
  {
    input = apps::WindowMode::kUnknown;
    apps::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::WindowMode::kUnknown);
  }
  {
    input = apps::WindowMode::kWindow;
    apps::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::WindowMode::kWindow);
  }
  {
    input = apps::WindowMode::kBrowser;
    apps::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::WindowMode::kBrowser);
  }
  {
    input = apps::WindowMode::kTabbedWindow;
    apps::WindowMode output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::WindowMode>(
        input, output));
    EXPECT_EQ(output, apps::WindowMode::kTabbedWindow);
  }
}

// Test that serialization and deserialization works with launch source.
TEST(AppServiceTypesMojomTraitsTest, RoundTripLaunchSource) {
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

TEST(AppServiceTypesMojomTraitsTest, RoundTripPermissions) {
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kUnknown,
        std::make_unique<apps::PermissionValue>(true),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kCamera,
        std::make_unique<apps::PermissionValue>(true),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kLocation,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAllow),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kMicrophone,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kNotifications,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAsk),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kContacts,
        std::make_unique<apps::PermissionValue>(apps::TriState::kAllow),
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kStorage,
        std::make_unique<apps::PermissionValue>(apps::TriState::kBlock),
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
}

// Test that serialization and deserialization works with updating
// preferred app.
TEST(AppServiceTypesMojomTraitsTest, PreferredApp) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "1",
                                         apps::PatternMatchType::kNone);
  auto input =
      std::make_unique<apps::PreferredApp>(std::move(intent_filter), "abcdefg");

  apps::PreferredAppPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::PreferredApp>(
      input, output));
  EXPECT_EQ(*input, *output);
}

// Test that serialization and deserialization works with updating
// PreferredAppChanges.
TEST(AppServiceTypesMojomTraitsTest, PreferredAppChanges) {
  apps::IntentFilters added_filters;
  auto intent_filter1 = std::make_unique<apps::IntentFilter>();
  intent_filter1->AddSingleValueCondition(apps::ConditionType::kScheme, "1",
                                          apps::PatternMatchType::kNone);
  auto intent_filter2 = std::make_unique<apps::IntentFilter>();
  intent_filter2->AddSingleValueCondition(apps::ConditionType::kHost, "2",
                                          apps::PatternMatchType::kLiteral);
  added_filters.push_back(std::move(intent_filter1));
  added_filters.push_back(std::move(intent_filter2));

  apps::IntentFilters removed_filters;
  auto intent_filter3 = std::make_unique<apps::IntentFilter>();
  intent_filter3->AddSingleValueCondition(apps::ConditionType::kPattern, "3",
                                          apps::PatternMatchType::kPrefix);
  auto intent_filter4 = std::make_unique<apps::IntentFilter>();
  intent_filter4->AddSingleValueCondition(apps::ConditionType::kAction, "4",
                                          apps::PatternMatchType::kGlob);
  removed_filters.push_back(std::move(intent_filter3));
  removed_filters.push_back(std::move(intent_filter4));

  auto input = std::make_unique<apps::PreferredAppChanges>();
  input->added_filters["a"] = std::move(added_filters);
  input->removed_filters["b"] = std::move(removed_filters);

  apps::PreferredAppChangesPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::PreferredAppChanges>(
          input, output));

  EXPECT_EQ(input->added_filters.size(), output->added_filters.size());
  for (const auto& added_filters : input->added_filters) {
    EXPECT_TRUE(IsEqual(added_filters.second,
                        output->added_filters[added_filters.first]));
  }

  EXPECT_EQ(input->removed_filters.size(), output->removed_filters.size());
  for (const auto& removed_filters : input->removed_filters) {
    EXPECT_TRUE(IsEqual(removed_filters.second,
                        output->removed_filters[removed_filters.first]));
  }
}

TEST(AppServiceTypesMojomTraitsTest, RoundTripShortcuts) {
  {
    auto shortcut = std::make_unique<apps::Shortcut>("test_id", "test_name",
                                                     /*position*/ 1);
    apps::ShortcutPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Shortcut>(
        shortcut, output));
    EXPECT_EQ(*shortcut, *output);
  }
  {
    auto shortcut = std::make_unique<apps::Shortcut>("", "", /*position*/ 0);
    apps::ShortcutPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Shortcut>(
        shortcut, output));
    EXPECT_EQ(*shortcut, *output);
  }
  {
    auto shortcut =
        std::make_unique<apps::Shortcut>("A", "B", /*position*/ 100);
    apps::ShortcutPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Shortcut>(
        shortcut, output));
    EXPECT_EQ(*shortcut, *output);
  }
  {
    auto shortcut = std::make_unique<apps::Shortcut>("", "B", /*position*/ 1);
    apps::ShortcutPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Shortcut>(
        shortcut, output));
    EXPECT_EQ(*shortcut, *output);
  }
  {
    auto shortcut = std::make_unique<apps::Shortcut>("A", "", /*position*/ 1);
    apps::ShortcutPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Shortcut>(
        shortcut, output));
    EXPECT_EQ(*shortcut, *output);
  }
}
