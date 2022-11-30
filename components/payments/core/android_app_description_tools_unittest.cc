// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/android_app_description_tools.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/payments/core/android_app_description.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace
}  // namespace payments
