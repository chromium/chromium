// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const apps::AppType app_type = apps::AppType::kArc;
const char app_id[] = "abcdefgh";
const char test_name_0[] = "Inigo Montoya";
const char test_name_1[] = "Dread Pirate Roberts";
}  // namespace

class AppUpdateTest : public testing::Test {
 protected:
  apps::Readiness expect_readiness_;
  apps::Readiness expect_prior_readiness_;

  std::string expect_name_;
  bool expect_name_changed_;

  std::string expect_short_name_;

  std::string expect_publisher_id_;

  std::string expect_description_;

  std::string expect_version_;

  absl::optional<apps::IconKey> expect_icon_key_;

  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");

  void CheckExpects(const apps::AppUpdate& u) {
    EXPECT_EQ(expect_readiness_, u.GetReadiness());
    EXPECT_EQ(expect_prior_readiness_, u.GetPriorReadiness());

    EXPECT_EQ(expect_name_, u.GetName());

    EXPECT_EQ(expect_short_name_, u.GetShortName());

    EXPECT_EQ(expect_publisher_id_, u.GetPublisherId());

    EXPECT_EQ(expect_description_, u.GetDescription());

    EXPECT_EQ(expect_version_, u.GetVersion());

    if (expect_icon_key_.has_value()) {
      ASSERT_TRUE(u.GetIconKey().has_value());
      EXPECT_EQ(expect_icon_key_.value(), u.GetIconKey().value());
    } else {
      ASSERT_FALSE(u.GetIconKey().has_value());
    }

    EXPECT_EQ(account_id_, u.AccountId());
  }

  void TestAppUpdate(apps::App* state, apps::App* delta) {
    apps::AppUpdate u(state, delta, account_id_);

    EXPECT_EQ(app_type, u.GetAppType());
    EXPECT_EQ(app_id, u.GetAppId());

    expect_readiness_ = apps::Readiness::kUnknown;
    expect_prior_readiness_ = apps::Readiness::kUnknown;
    expect_name_ = "";
    expect_short_name_ = "";
    expect_publisher_id_ = "";
    expect_description_ = "";
    expect_version_ = "";
    expect_icon_key_ = absl::nullopt;
    CheckExpects(u);

    if (delta) {
      delta->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      state->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::Readiness::kReady;
      expect_readiness_ = apps::Readiness::kReady;
      CheckExpects(u);

      delta->name = absl::nullopt;
      expect_name_ = state ? test_name_0 : "";
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      expect_prior_readiness_ = state->readiness;
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::Readiness::kDisabledByPolicy;
      expect_readiness_ = apps::Readiness::kDisabledByPolicy;
      delta->name = test_name_1;
      expect_name_ = test_name_1;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    // ShortName tests.

    if (state) {
      state->short_name = "Kate";
      expect_short_name_ = "Kate";
      CheckExpects(u);
    }

    if (delta) {
      delta->short_name = "Bob";
      expect_short_name_ = "Bob";
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      expect_prior_readiness_ = state->readiness;
      CheckExpects(u);
    }

    // PublisherId tests.

    if (state) {
      state->publisher_id = "com.google.android.youtube";
      expect_publisher_id_ = "com.google.android.youtube";
      CheckExpects(u);
    }

    if (delta) {
      delta->publisher_id = "com.android.youtube";
      expect_publisher_id_ = "com.android.youtube";
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      CheckExpects(u);
    }

    // Description tests.

    if (state) {
      state->description = "Has a cat.";
      expect_description_ = "Has a cat.";
      CheckExpects(u);
    }

    if (delta) {
      delta->description = "Has a dog.";
      expect_description_ = "Has a dog.";
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      CheckExpects(u);
    }

    // Version tests.

    if (state) {
      state->version = "1.0.0";
      expect_version_ = "1.0.0";
      CheckExpects(u);
    }

    if (delta) {
      delta->version = "1.0.1";
      expect_version_ = "1.0.1";
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      CheckExpects(u);
    }

    // IconKey tests.

    if (state) {
      state->icon_key = apps::IconKey(100, 0, 0);
      expect_icon_key_ = apps::IconKey(100, 0, 0);
      CheckExpects(u);
    }

    if (delta) {
      delta->icon_key = apps::IconKey(200, 0, 0);
      expect_icon_key_ = apps::IconKey(200, 0, 0);
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      CheckExpects(u);
    }
  }
};

TEST_F(AppUpdateTest, StateIsNonNull) {
  apps::App state(app_type, app_id);
  TestAppUpdate(&state, nullptr);
}

TEST_F(AppUpdateTest, DeltaIsNonNull) {
  apps::App delta(app_type, app_id);
  TestAppUpdate(nullptr, &delta);
}

TEST_F(AppUpdateTest, BothAreNonNull) {
  apps::App state(app_type, app_id);
  apps::App delta(app_type, app_id);
  TestAppUpdate(&state, &delta);
}
