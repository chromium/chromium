// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"

#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {
const AppType app_type = AppType::kArc;
const char app_id[] = "abcdefgh";
const char test_name_0[] = "Inigo Montoya";
const char test_name_1[] = "Dread Pirate Roberts";

PermissionPtr MakePermission(PermissionType permission_type,
                             TriState tri_state,
                             bool is_managed) {
  return std::make_unique<Permission>(
      permission_type, std::make_unique<PermissionValue>(tri_state),
      is_managed);
}

PermissionPtr MakePermission(PermissionType permission_type,
                             bool bool_value,
                             bool is_managed) {
  return std::make_unique<Permission>(
      permission_type, std::make_unique<PermissionValue>(bool_value),
      is_managed);
}

bool IsEqual(const Permissions& source, const Permissions& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

class AppUpdateTest : public testing::Test {
 protected:
  Readiness expect_readiness_;
  Readiness expect_prior_readiness_;

  std::string expect_name_;
  bool expect_name_changed_;

  std::string expect_short_name_;

  std::string expect_publisher_id_;

  std::string expect_description_;

  std::string expect_version_;

  std::vector<std::string> expect_additional_search_terms_;

  absl::optional<IconKey> expect_icon_key_;

  base::Time expect_last_launch_time_;

  base::Time expect_install_time_;

  Permissions expect_permissions_;

  InstallReason expect_install_reason_;

  InstallSource expect_install_source_;

  std::string expect_policy_id_;

  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");

  void CheckExpects(const AppUpdate& u) {
    EXPECT_EQ(expect_readiness_, u.GetReadiness());
    EXPECT_EQ(expect_prior_readiness_, u.GetPriorReadiness());

    EXPECT_EQ(expect_name_, u.GetName());

    EXPECT_EQ(expect_short_name_, u.GetShortName());

    EXPECT_EQ(expect_publisher_id_, u.GetPublisherId());

    EXPECT_EQ(expect_description_, u.GetDescription());

    EXPECT_EQ(expect_version_, u.GetVersion());

    EXPECT_EQ(expect_additional_search_terms_, u.GetAdditionalSearchTerms());

    if (expect_icon_key_.has_value()) {
      ASSERT_TRUE(u.GetIconKey().has_value());
      EXPECT_EQ(expect_icon_key_.value(), u.GetIconKey().value());
    } else {
      ASSERT_FALSE(u.GetIconKey().has_value());
    }

    EXPECT_EQ(expect_last_launch_time_, u.GetLastLaunchTime());

    EXPECT_EQ(expect_install_time_, u.GetInstallTime());

    EXPECT_TRUE(IsEqual(expect_permissions_, u.GetPermissions()));

    EXPECT_EQ(expect_install_reason_, u.GetInstallReason());

    EXPECT_EQ(expect_install_source_, u.GetInstallSource());

    EXPECT_EQ(expect_policy_id_, u.GetPolicyId());

    EXPECT_EQ(account_id_, u.AccountId());
  }

  void TestAppUpdate(App* state, App* delta) {
    AppUpdate u(state, delta, account_id_);

    EXPECT_EQ(app_type, u.GetAppType());
    EXPECT_EQ(app_id, u.GetAppId());

    expect_readiness_ = Readiness::kUnknown;
    expect_prior_readiness_ = Readiness::kUnknown;
    expect_name_ = "";
    expect_short_name_ = "";
    expect_publisher_id_ = "";
    expect_description_ = "";
    expect_version_ = "";
    expect_additional_search_terms_.clear();
    expect_icon_key_ = absl::nullopt;
    expect_last_launch_time_ = base::Time();
    expect_install_time_ = base::Time();
    expect_permissions_.clear();
    expect_install_reason_ = InstallReason::kUnknown;
    expect_install_source_ = InstallSource::kUnknown;
    expect_policy_id_ = "";
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
      delta->readiness = Readiness::kReady;
      expect_readiness_ = Readiness::kReady;
      CheckExpects(u);

      delta->name = absl::nullopt;
      expect_name_ = state ? test_name_0 : "";
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      expect_prior_readiness_ = state->readiness;
      EXPECT_EQ(expect_name_, state->name);
      EXPECT_EQ(expect_readiness_, state->readiness);
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = Readiness::kDisabledByPolicy;
      expect_readiness_ = Readiness::kDisabledByPolicy;
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
      AppUpdate::Merge(state, delta);
      expect_prior_readiness_ = state->readiness;
      EXPECT_EQ(expect_short_name_, state->short_name);
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
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_publisher_id_, state->publisher_id);
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
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_description_, state->description);
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
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_version_, state->version);
      CheckExpects(u);
    }

    // AdditionalSearchTerms tests.

    if (state) {
      state->additional_search_terms.push_back("cat");
      state->additional_search_terms.push_back("dog");
      expect_additional_search_terms_.push_back("cat");
      expect_additional_search_terms_.push_back("dog");
      CheckExpects(u);
    }

    if (delta) {
      expect_additional_search_terms_.clear();
      delta->additional_search_terms.push_back("horse");
      delta->additional_search_terms.push_back("mouse");
      expect_additional_search_terms_.push_back("horse");
      expect_additional_search_terms_.push_back("mouse");
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_additional_search_terms_,
                state->additional_search_terms);
      CheckExpects(u);
    }

    // IconKey tests.

    if (state) {
      state->icon_key = IconKey(100, 0, 0);
      expect_icon_key_ = IconKey(100, 0, 0);
      CheckExpects(u);
    }

    if (delta) {
      delta->icon_key = IconKey(200, 0, 0);
      expect_icon_key_ = IconKey(200, 0, 0);
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_icon_key_.value(), state->icon_key.value());
      CheckExpects(u);
    }

    // LastLaunchTime tests.

    if (state) {
      state->last_launch_time = base::Time::FromDoubleT(1000.0);
      expect_last_launch_time_ = base::Time::FromDoubleT(1000.0);
      CheckExpects(u);
    }

    if (delta) {
      delta->last_launch_time = base::Time::FromDoubleT(1001.0);
      expect_last_launch_time_ = base::Time::FromDoubleT(1001.0);
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_last_launch_time_, state->last_launch_time);
      CheckExpects(u);
    }

    // InstallTime tests.

    if (state) {
      state->install_time = base::Time::FromDoubleT(2000.0);
      expect_install_time_ = base::Time::FromDoubleT(2000.0);
      CheckExpects(u);
    }

    if (delta) {
      delta->install_time = base::Time::FromDoubleT(2001.0);
      expect_install_time_ = base::Time::FromDoubleT(2001.0);
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_install_time_, state->install_time);
      CheckExpects(u);
    }

    // Permission tests.

    if (state) {
      auto p0 = MakePermission(PermissionType::kLocation, TriState::kAllow,
                               /*is_managed=*/true);
      auto p1 = MakePermission(PermissionType::kNotifications, TriState::kBlock,
                               /*is_managed=*/false);
      state->permissions.push_back(p0->Clone());
      state->permissions.push_back(p1->Clone());
      expect_permissions_.push_back(p0->Clone());
      expect_permissions_.push_back(p1->Clone());
      CheckExpects(u);
    }

    if (delta) {
      expect_permissions_.clear();
      auto p0 = MakePermission(PermissionType::kNotifications,
                               /*bool_value=*/true,
                               /*is_managed=*/false);
      auto p1 = MakePermission(PermissionType::kLocation,
                               /*bool_value=*/false,
                               /*is_managed=*/true);
      delta->permissions.push_back(p0->Clone());
      delta->permissions.push_back(p1->Clone());
      expect_permissions_.push_back(p0->Clone());
      expect_permissions_.push_back(p1->Clone());
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_TRUE(IsEqual(expect_permissions_, state->permissions));
      CheckExpects(u);
    }

    // InstallReason tests.
    if (state) {
      state->install_reason = InstallReason::kUser;
      expect_install_reason_ = InstallReason::kUser;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_reason = InstallReason::kPolicy;
      expect_install_reason_ = InstallReason::kPolicy;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_install_reason_, state->install_reason);
      CheckExpects(u);
    }

    // InstallSource tests.
    if (state) {
      state->install_source = InstallSource::kPlayStore;
      expect_install_source_ = InstallSource::kPlayStore;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_source = InstallSource::kSync;
      expect_install_source_ = InstallSource::kSync;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_install_source_, state->install_source);
      CheckExpects(u);
    }

    // PolicyId tests.
    if (state) {
      state->policy_id = "https://app.site/alpha";
      expect_policy_id_ = "https://app.site/alpha";
      CheckExpects(u);
    }

    if (delta) {
      delta->policy_id = "https://app.site/delta";
      expect_policy_id_ = "https://app.site/delta";
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_policy_id_, state->policy_id);
      CheckExpects(u);
    }
  }
};

TEST_F(AppUpdateTest, StateIsNonNull) {
  App state(app_type, app_id);
  TestAppUpdate(&state, nullptr);
}

TEST_F(AppUpdateTest, DeltaIsNonNull) {
  App delta(app_type, app_id);
  TestAppUpdate(nullptr, &delta);
}

TEST_F(AppUpdateTest, BothAreNonNull) {
  App state(app_type, app_id);
  App delta(app_type, app_id);
  TestAppUpdate(&state, &delta);
}

}  // namespace apps
