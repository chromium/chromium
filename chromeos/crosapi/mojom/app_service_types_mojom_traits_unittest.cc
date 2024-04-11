// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types_mojom_traits.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
  input->icon_key =
      apps::IconKey(/*raw_icon_updated=*/true,
                    /*icon_effects=*/apps::IconEffects::kChromeBadge);
  input->last_launch_time = base::Time() + base::Days(1);
  input->install_time = base::Time() + base::Days(2);
  input->install_reason = apps::InstallReason::kUser;
  input->policy_ids = {"https://app.site/alpha"};
  input->recommendable = true;
  input->searchable = true;
  input->show_in_launcher = true;
  input->show_in_shelf = true;
  input->show_in_search = true;
  input->show_in_management = true;
  input->has_badge = std::nullopt;
  input->paused = false;
  input->app_size_in_bytes = 1000000;
  input->data_size_in_bytes = 1000000;

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "https",
                                         apps::PatternMatchType::kLiteral);
  intent_filter->activity_name = "activity_name";
  intent_filter->activity_label = "activity_label";
  input->intent_filters.push_back(std::move(intent_filter));

  input->window_mode = apps::WindowMode::kWindow;

  input->permissions.push_back(
      std::make_unique<apps::Permission>(apps::PermissionType::kCamera,
                                         /*value=*/true,
                                         /*is_managed=*/true));

  input->allow_uninstall = true;
  input->handles_intents = true;

  input->is_platform_app = true;
  input->allow_close = true;
  input->allow_window_mode_selection = true;
  input->installer_package_id =
      apps::PackageId(apps::PackageType::kArc, "com.foo.bar");

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
  EXPECT_EQ(output->app_size_in_bytes, 1000000);
  EXPECT_EQ(output->data_size_in_bytes, 1000000);

  EXPECT_TRUE(absl::holds_alternative<bool>(output->icon_key->update_version));
  EXPECT_TRUE(absl::get<bool>(output->icon_key->update_version));
  EXPECT_EQ(output->icon_key->icon_effects, 2U);

  EXPECT_EQ(output->last_launch_time, base::Time() + base::Days(1));
  EXPECT_EQ(output->install_time, base::Time() + base::Days(2));

  EXPECT_EQ(output->install_reason, apps::InstallReason::kUser);
  EXPECT_THAT(output->policy_ids,
              ::testing::ElementsAre("https://app.site/alpha"));
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
            apps::PatternMatchType::kLiteral);
  EXPECT_EQ(filter->activity_name, "activity_name");
  EXPECT_EQ(filter->activity_label, "activity_label");

  EXPECT_EQ(output->window_mode, apps::WindowMode::kWindow);

  ASSERT_EQ(output->permissions.size(), 1U);
  auto& out_permission = output->permissions[0];
  EXPECT_EQ(out_permission->permission_type, apps::PermissionType::kCamera);
  ASSERT_TRUE(absl::holds_alternative<bool>(out_permission->value));
  EXPECT_TRUE(absl::get<bool>(out_permission->value));
  EXPECT_TRUE(out_permission->is_managed);

  EXPECT_TRUE(output->allow_uninstall.value());
  EXPECT_TRUE(output->handles_intents.value());

  EXPECT_TRUE(output->is_platform_app.value());
  EXPECT_TRUE(output->allow_close.value());
  EXPECT_TRUE(output->allow_window_mode_selection.value());
  EXPECT_EQ(output->installer_package_id,
            apps::PackageId(apps::PackageType::kArc, "com.foo.bar"));
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
  input->has_badge = std::nullopt;
  input->paused = false;

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->AddSingleValueCondition(apps::ConditionType::kScheme, "https",
                                         apps::PatternMatchType::kLiteral);
  input->intent_filters.push_back(std::move(intent_filter));
  input->window_mode = apps::WindowMode::kBrowser;
  input->allow_uninstall = true;
  input->handles_intents = true;
  input->is_platform_app = std::nullopt;
  input->app_size_in_bytes = std::nullopt;
  input->data_size_in_bytes = std::nullopt;
  input->allow_close = std::nullopt;
  input->allow_window_mode_selection = std::nullopt;
  input->installer_package_id = std::nullopt;

  apps::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  EXPECT_EQ(output->app_type, apps::AppType::kWeb);
  EXPECT_EQ(output->app_id, "abcdefg");
  EXPECT_EQ(output->readiness, apps::Readiness::kReady);
  EXPECT_EQ(output->additional_search_terms, input->additional_search_terms);

  EXPECT_EQ(output->install_reason, apps::InstallReason::kUser);
  EXPECT_TRUE(output->policy_ids.empty());
  EXPECT_TRUE(output->recommendable.value());
  EXPECT_TRUE(output->searchable.value());
  EXPECT_TRUE(output->show_in_launcher.value());
  EXPECT_TRUE(output->show_in_shelf.value());
  EXPECT_TRUE(output->show_in_search.value());
  EXPECT_TRUE(output->show_in_management.value());
  EXPECT_FALSE(output->has_badge.has_value());
  EXPECT_FALSE(output->paused.value());
  EXPECT_FALSE(output->app_size_in_bytes.has_value());
  EXPECT_FALSE(output->data_size_in_bytes.has_value());

  ASSERT_EQ(output->intent_filters.size(), 1U);
  auto& filter = output->intent_filters[0];
  ASSERT_EQ(filter->conditions.size(), 1U);
  auto& condition = filter->conditions[0];
  EXPECT_EQ(condition->condition_type, apps::ConditionType::kScheme);
  ASSERT_EQ(condition->condition_values.size(), 1U);
  EXPECT_EQ(condition->condition_values[0]->value, "https");
  EXPECT_EQ(condition->condition_values[0]->match_type,
            apps::PatternMatchType::kLiteral);

  EXPECT_EQ(output->window_mode, apps::WindowMode::kBrowser);
  EXPECT_TRUE(output->allow_uninstall);
  EXPECT_TRUE(output->handles_intents);
  EXPECT_FALSE(output->is_platform_app.has_value());
  EXPECT_FALSE(output->allow_close.has_value());
  EXPECT_FALSE(output->allow_window_mode_selection.has_value());
  EXPECT_FALSE(output->installer_package_id.has_value());
}

// Test that serialization and deserialization ignores unknown PackageId values.
TEST(AppServiceTypesMojomTraitsTest, RoundTripUnknownPackageId) {
  auto input = std::make_unique<apps::App>(apps::AppType::kWeb, "abcdefg");
  // In practice, nobody should ever create an Unknown PackageId like this. The
  // most likely cause of this case is version skew in crosapi.
  input->installer_package_id =
      apps::PackageId(apps::PackageType::kUnknown, "foo");

  apps::AppPtr output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(input, output));

  ASSERT_EQ(output->installer_package_id, std::nullopt);
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

// Test that serialization and deserialization works with updating IconKey.
TEST(AppServiceTypesMojomTraitsTest, RoundTripIconKey) {
  {
    auto icon_key = std::make_unique<apps::IconKey>(/*raw_icon_updated=*/true,
                                                    apps::IconEffects::kNone);
    apps::IconKeyPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconKey>(
        icon_key, output));
    EXPECT_EQ(*icon_key, *output);
  }
  {
    auto icon_key = std::make_unique<apps::IconKey>();
    apps::IconKeyPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconKey>(
        icon_key, output));
    EXPECT_EQ(*icon_key, *output);
  }
  {
    auto icon_key =
        std::make_unique<apps::IconKey>(apps::IconEffects::kBlocked);
    icon_key->update_version = apps::IconKey::kInitVersion;
    apps::IconKeyPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconKey>(
        icon_key, output));
    EXPECT_EQ(*icon_key, *output);
  }
  {
    auto icon_key = std::make_unique<apps::IconKey>(
        apps::IconEffects::kCrOsStandardBackground);
    icon_key->update_version = 100;
    apps::IconKeyPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::IconKey>(
        icon_key, output));
    EXPECT_EQ(*icon_key, *output);
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
  {
    input->install_reason = apps::InstallReason::kKiosk;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kKiosk);
  }
  {
    input->install_reason = apps::InstallReason::kCommandLine;
    apps::AppPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::App>(
        input, output));
    EXPECT_EQ(output->install_reason, apps::InstallReason::kCommandLine);
  }
}

// Test that serialization and deserialization works with updating
// recommendable.
TEST(AppServiceTypesMojomTraitsTest, RoundTripRecommendable) {
  auto input = std::make_unique<apps::App>(apps::AppType::kArc, "abcdefg");
  {
    input->recommendable = std::nullopt;
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
    input->searchable = std::nullopt;
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
    input->show_in_launcher = std::nullopt;
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
    input->show_in_shelf = std::nullopt;
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
    input->show_in_search = std::nullopt;
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
    input->show_in_management = std::nullopt;
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
    input->has_badge = std::nullopt;
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
    input->paused = std::nullopt;
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
                                         apps::PatternMatchType::kLiteral);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kAuthority, "2",
                                         apps::PatternMatchType::kLiteral);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kPath, "3",
                                         apps::PatternMatchType::kPrefix);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction, "4",
                                         apps::PatternMatchType::kGlob);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kMimeType, "5",
                                         apps::PatternMatchType::kMimeType);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kFile, "6",
                                         apps::PatternMatchType::kMimeType);
  intent_filter->AddSingleValueCondition(
      apps::ConditionType::kFile, "7", apps::PatternMatchType::kFileExtension);
  intent_filter->AddSingleValueCondition(apps::ConditionType::kAuthority, "8",
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
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition->condition_values[0]->value, "1");
  }
  {
    auto& condition = filter->conditions[1];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kAuthority);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kLiteral);
    EXPECT_EQ(condition->condition_values[0]->value, "2");
  }
  {
    auto& condition = filter->conditions[2];
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kPath);
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
    EXPECT_EQ(condition->condition_type, apps::ConditionType::kAuthority);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              apps::PatternMatchType::kSuffix);
    EXPECT_EQ(condition->condition_values[0]->value, "8");
  }
}

// Test that serialization and deserialization works with uninstall source.
TEST(AppServiceTypesMojomTraitsTest, RoundTripUninstallSource) {
  apps::UninstallSource input;
  {
    input = apps::UninstallSource::kUnknown;
    apps::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::UninstallSource::kUnknown);
  }
  {
    input = apps::UninstallSource::kAppList;
    apps::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::UninstallSource::kAppList);
  }
  {
    input = apps::UninstallSource::kAppManagement;
    apps::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::UninstallSource::kAppManagement);
  }
  {
    input = apps::UninstallSource::kShelf;
    apps::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::UninstallSource::kShelf);
  }
  {
    input = apps::UninstallSource::kMigration;
    apps::UninstallSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::UninstallSource>(
            input, output));
    EXPECT_EQ(output, apps::UninstallSource::kMigration);
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

    gfx::ImageSkia image = gfx::test::CreateImageSkia(1, 2);
    input->uncompressed = image;

    input->compressed = {1u, 2u};
    input->is_placeholder_icon = true;
    input->is_maskable_icon = false;

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

    gfx::ImageSkia image = gfx::test::CreateImageSkia(3, 4);
    input->uncompressed = image;
    input->is_placeholder_icon = false;
    input->is_maskable_icon = true;

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
    input->is_maskable_icon = true;

    auto output = std::make_unique<apps::IconValue>();
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
  apps::LaunchSource input;
  {
    input = apps::LaunchSource::kUnknown;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromAppListGrid;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromAppListGridContextMenu;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromAppListQuery;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromAppListQueryContextMenu;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromAppListRecommendation;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromParentalControls;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromShelf;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromFileManager;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromLink;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromOmnibox;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromChromeInternal;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromKeyboard;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromOtherApp;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromMenu;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromInstalledNotification;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromTest;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromArc;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromSharesheet;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromReleaseNotesNotification;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromFullRestore;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromSmartTextContextMenu;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromDiscoverTabNotification;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
  {
    input = apps::LaunchSource::kFromInstaller;
    apps::LaunchSource output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::LaunchSource>(
            input, output));
    EXPECT_EQ(output, input);
  }
}

TEST(AppServiceTypesMojomTraitsTest, RoundTripPermissions) {
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kUnknown, /*value=*/true,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kCamera, /*value=*/true,
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kLocation, /*value=*/apps::TriState::kAllow,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kMicrophone, /*value=*/apps::TriState::kBlock,
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kNotifications, /*value=*/apps::TriState::kAsk,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kContacts, /*value=*/apps::TriState::kAllow,
        /*is_managed=*/true);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kStorage, /*value=*/apps::TriState::kBlock,
        /*is_managed=*/false);
    apps::PermissionPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::Permission>(
        permission, output));
    EXPECT_EQ(*permission, *output);
  }
  {
    auto permission = std::make_unique<apps::Permission>(
        apps::PermissionType::kFileHandling, /*value=*/true,
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
                                         apps::PatternMatchType::kLiteral);
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
                                          apps::PatternMatchType::kLiteral);
  auto intent_filter2 = std::make_unique<apps::IntentFilter>();
  intent_filter2->AddSingleValueCondition(apps::ConditionType::kAuthority, "2",
                                          apps::PatternMatchType::kLiteral);
  added_filters.push_back(std::move(intent_filter1));
  added_filters.push_back(std::move(intent_filter2));

  apps::IntentFilters removed_filters;
  auto intent_filter3 = std::make_unique<apps::IntentFilter>();
  intent_filter3->AddSingleValueCondition(apps::ConditionType::kPath, "3",
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
  for (const auto& filter : input->added_filters) {
    EXPECT_TRUE(IsEqual(filter.second, output->added_filters[filter.first]));
  }

  EXPECT_EQ(input->removed_filters.size(), output->removed_filters.size());
  for (const auto& filter : input->removed_filters) {
    EXPECT_TRUE(IsEqual(filter.second, output->removed_filters[filter.first]));
  }
}

TEST(AppServiceTypesMojomTraitsTest, RoundTripCapabilityAccess) {
  {
    auto capability_access = std::make_unique<apps::CapabilityAccess>("a");
    apps::CapabilityAccessPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::CapabilityAccess>(
            capability_access, output));
    EXPECT_EQ("a", output->app_id);
    EXPECT_FALSE(output->camera.has_value());
    EXPECT_FALSE(output->microphone.has_value());
  }
  {
    auto capability_access = std::make_unique<apps::CapabilityAccess>("b");
    capability_access->camera = true;
    apps::CapabilityAccessPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::CapabilityAccess>(
            capability_access, output));
    EXPECT_EQ("b", output->app_id);
    EXPECT_TRUE(output->camera.value_or(false));
    EXPECT_FALSE(output->microphone.has_value());
  }
  {
    auto capability_access = std::make_unique<apps::CapabilityAccess>("c");
    capability_access->camera = false;
    capability_access->microphone = true;
    apps::CapabilityAccessPtr output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<crosapi::mojom::CapabilityAccess>(
            capability_access, output));
    EXPECT_EQ("c", output->app_id);
    EXPECT_FALSE(output->camera.value_or(true));
    EXPECT_TRUE(output->microphone.value_or(false));
  }
}

// Test that every field in apps::Shortcut in correctly converted.
TEST(AppServiceTypesMojomTraitsTest, ShortcutRoundTrip) {
  auto input = std::make_unique<apps::Shortcut>("host_app_id", "local_id");
  input->name = "lacros test name";
  input->icon_key =
      apps::IconKey(/*raw_icon_updated=*/true,
                    /*icon_effects=*/apps::IconEffects::kChromeBadge);
  input->allow_removal = true;

  apps::ShortcutPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::AppShortcut>(
      input, output));

  EXPECT_EQ(output->host_app_id, "host_app_id");
  EXPECT_EQ(output->local_id, "local_id");
  EXPECT_EQ(output->shortcut_id,
            apps::GenerateShortcutId("host_app_id", "local_id"));
  EXPECT_EQ(output->name, "lacros test name");
  EXPECT_EQ(output->shortcut_source, apps::ShortcutSource::kUser);

  EXPECT_EQ(output->icon_key->icon_effects, 2U);
  EXPECT_TRUE(absl::holds_alternative<bool>(output->icon_key->update_version));
  EXPECT_TRUE(absl::get<bool>(output->icon_key->update_version));
  EXPECT_TRUE(output->allow_removal);
}

// Test that serialization and deserialization works with optional fields that
// doesn't fill up.
TEST(AppServiceTypesMojomTraitsTest, ShortcutRoundTripNoOptional) {
  auto input = std::make_unique<apps::Shortcut>("host_app_id", "local_id");

  apps::ShortcutPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::AppShortcut>(
      input, output));

  EXPECT_EQ(output->host_app_id, "host_app_id");
  EXPECT_EQ(output->local_id, "local_id");
  EXPECT_EQ(output->shortcut_id,
            apps::GenerateShortcutId("host_app_id", "local_id"));
  EXPECT_EQ(output->shortcut_source, apps::ShortcutSource::kUser);
}

TEST(AppServiceTypesMojomTraitsTest, PackageIdRoundTrip) {
  {
    auto package_id = apps::PackageId(apps::PackageType::kArc, "com.foo.bar");
    apps::PackageId output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::PackageId>(
        package_id, output));
    EXPECT_EQ(package_id, output);
  }
  {
    auto package_id =
        apps::PackageId(apps::PackageType::kWeb, "https://www.foo.com/bar");
    apps::PackageId output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::PackageId>(
        package_id, output));
    EXPECT_EQ(package_id, output);
  }
  {
    auto package_id = apps::PackageId(apps::PackageType::kUnknown, "someapp");
    apps::PackageId output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<crosapi::mojom::PackageId>(
        package_id, output));
    EXPECT_EQ(package_id, output);
  }
}
