// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/android_app_description_tools.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/payments/core/android_app_description.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace payments {
namespace {

// Absence of activities should result in zero apps.
TEST(AndroidAppDescriptionToolsTest, SplitNoActivities) {
  std::vector<std::unique_ptr<AndroidAppDescription>> destination;
  SplitPotentiallyMultipleActivities(std::make_unique<AndroidAppDescription>(),
                                     &destination);
  EXPECT_TRUE(destination.empty());
}

// One activity should result in one app.
TEST(AndroidAppDescriptionToolsTest, SplitOneActivity) {
  std::vector<std::unique_ptr<AndroidAppDescription>> destination;
  auto app = std::make_unique<AndroidAppDescription>();
  app->package = "com.example.app";
  app->service_names = {"com.example.app.Service"};
  app->activities.emplace_back(std::make_unique<AndroidActivityDescription>());
  app->activities.back()->name = "com.example.app.Activity";
  app->activities.back()->default_payment_method = "https://example.test";

  SplitPotentiallyMultipleActivities(std::move(app), &destination);

  ASSERT_EQ(1U, destination.size());
  EXPECT_EQ("com.example.app", destination.back()->package);
  EXPECT_EQ(std::vector<std::string>{"com.example.app.Service"},
            destination.back()->service_names);
  ASSERT_EQ(1U, destination.back()->activities.size());
  EXPECT_EQ("com.example.app.Activity",
            destination.back()->activities.back()->name);
  EXPECT_EQ("https://example.test",
            destination.back()->activities.back()->default_payment_method);
}

// Two activities should result in two apps.
TEST(AndroidAppDescriptionToolsTest, SplitTwoActivities) {
  std::vector<std::unique_ptr<AndroidAppDescription>> destination;
  auto app = std::make_unique<AndroidAppDescription>();
  app->package = "com.example.app";
  app->service_names = {"com.example.app.Service"};
  app->activities.emplace_back(std::make_unique<AndroidActivityDescription>());
  app->activities.back()->name = "com.example.app.ActivityOne";
  app->activities.back()->default_payment_method = "https://one.test";
  app->activities.emplace_back(std::make_unique<AndroidActivityDescription>());
  app->activities.back()->name = "com.example.app.ActivityTwo";
  app->activities.back()->default_payment_method = "https://two.test";

  SplitPotentiallyMultipleActivities(std::move(app), &destination);

  ASSERT_EQ(2U, destination.size());

  EXPECT_EQ("com.example.app", destination.front()->package);
  EXPECT_EQ(std::vector<std::string>{"com.example.app.Service"},
            destination.front()->service_names);
  ASSERT_EQ(1U, destination.front()->activities.size());
  EXPECT_EQ("com.example.app.ActivityOne",
            destination.front()->activities.back()->name);
  EXPECT_EQ("https://one.test",
            destination.front()->activities.back()->default_payment_method);

  EXPECT_EQ("com.example.app", destination.back()->package);
  EXPECT_EQ(std::vector<std::string>{"com.example.app.Service"},
            destination.back()->service_names);
  ASSERT_EQ(1U, destination.back()->activities.size());
  EXPECT_EQ("com.example.app.ActivityTwo",
            destination.back()->activities.back()->name);
  EXPECT_EQ("https://two.test",
            destination.back()->activities.back()->default_payment_method);
}

// Verifies that SplitPotentiallyMultipleActivities extracts exactly one app per
// activity.
void MaintainsSingleActivityInvariant(
    size_t initial_destination_size,
    const std::string& app_package,
    const std::vector<std::string>& app_service_names,
    const std::vector<std::pair<std::string, std::string>>& app_activities) {
  std::vector<std::unique_ptr<AndroidAppDescription>> destination;
  destination.reserve(initial_destination_size);
  for (size_t i = 0; i < initial_destination_size; ++i) {
    auto existing_app = std::make_unique<AndroidAppDescription>();
    existing_app->activities.push_back(
        std::make_unique<AndroidActivityDescription>());
    destination.push_back(std::move(existing_app));
  }

  auto app = std::make_unique<AndroidAppDescription>();
  app->package = app_package;
  app->service_names = app_service_names;
  app->activities.reserve(app_activities.size());
  for (const auto& pair : app_activities) {
    auto activity = std::make_unique<AndroidActivityDescription>();
    activity->name = pair.first;
    activity->default_payment_method = pair.second;
    app->activities.push_back(std::move(activity));
  }

  size_t snapshot_size = destination.size();

  SplitPotentiallyMultipleActivities(std::move(app), &destination);

  // Assert split performed correctly.
  EXPECT_EQ(app_activities.size(), destination.size() - snapshot_size);
  for (size_t i = snapshot_size; i < destination.size(); ++i) {
    ASSERT_EQ(1u, destination[i]->activities.size());
    EXPECT_EQ(app_package, destination[i]->package);
    EXPECT_EQ(app_service_names, destination[i]->service_names);

    size_t activity_index = i - snapshot_size;
    EXPECT_EQ(app_activities[activity_index].first,
              destination[i]->activities.front()->name);
    EXPECT_EQ(app_activities[activity_index].second,
              destination[i]->activities.front()->default_payment_method);
  }

  // Verify that pre-existing elements in the destination were untouched.
  for (size_t i = 0; i < snapshot_size; ++i) {
    EXPECT_EQ(1u, destination[i]->activities.size());
    EXPECT_TRUE(destination[i]->package.empty());
  }
}

FUZZ_TEST(AndroidAppDescriptionToolsFuzzTest, MaintainsSingleActivityInvariant)
    .WithDomains(fuzztest::InRange<size_t>(0, 10),
                 fuzztest::String(),
                 fuzztest::VectorOf(fuzztest::String()),
                 fuzztest::VectorOf(fuzztest::PairOf(fuzztest::String(),
                                                     fuzztest::String())));

}  // namespace
}  // namespace payments
