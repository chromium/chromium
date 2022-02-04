// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"

#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
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

bool IsEqual(const IntentFilters& source, const IntentFilters& target) {
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

  absl::optional<bool> expect_is_platform_app_;

  absl::optional<bool> expect_recommendable_;

  absl::optional<bool> expect_searchable_;

  absl::optional<bool> expect_show_in_launcher_;

  absl::optional<bool> expect_show_in_shelf_;

  absl::optional<bool> expect_show_in_search_;

  absl::optional<bool> expect_show_in_management_;

  absl::optional<bool> expect_handles_intents_;

  absl::optional<bool> expect_allow_uninstall_;

  absl::optional<bool> expect_has_badge_;

  absl::optional<bool> expect_paused_;

  IntentFilters expect_intent_filters_;

  absl::optional<bool> expect_resize_locked_;

  WindowMode expect_window_mode_;

  absl::optional<RunOnOsLogin> expect_run_on_os_login_;

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

    EXPECT_EQ(expect_is_platform_app_, u.GetIsPlatformApp());

    EXPECT_EQ(expect_recommendable_, u.GetRecommendable());

    EXPECT_EQ(expect_searchable_, u.GetSearchable());

    EXPECT_EQ(expect_show_in_launcher_, u.GetShowInLauncher());

    EXPECT_EQ(expect_show_in_shelf_, u.GetShowInShelf());

    EXPECT_EQ(expect_show_in_search_, u.GetShowInSearch());

    EXPECT_EQ(expect_show_in_management_, u.GetShowInManagement());

    EXPECT_EQ(expect_handles_intents_, u.GetHandlesIntents());

    EXPECT_EQ(expect_has_badge_, u.GetHasBadge());

    EXPECT_EQ(expect_paused_, u.GetPaused());

    EXPECT_TRUE(IsEqual(expect_intent_filters_, u.GetIntentFilters()));

    EXPECT_EQ(expect_resize_locked_, u.GetResizeLocked());

    EXPECT_EQ(expect_window_mode_, u.GetWindowMode());
    if (expect_run_on_os_login_.has_value()) {
      ASSERT_TRUE(u.GetRunOnOsLogin().has_value());
      EXPECT_EQ(expect_run_on_os_login_.value(), u.GetRunOnOsLogin().value());
    } else {
      ASSERT_FALSE(u.GetRunOnOsLogin().has_value());
    }

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
    expect_is_platform_app_ = absl::nullopt;
    expect_recommendable_ = absl::nullopt;
    expect_searchable_ = absl::nullopt;
    expect_show_in_launcher_ = absl::nullopt;
    expect_show_in_shelf_ = absl::nullopt;
    expect_show_in_search_ = absl::nullopt;
    expect_show_in_management_ = absl::nullopt;
    expect_handles_intents_ = absl::nullopt;
    expect_has_badge_ = absl::nullopt;
    expect_paused_ = absl::nullopt;
    expect_intent_filters_.clear();
    expect_resize_locked_ = absl::nullopt;
    expect_window_mode_ = WindowMode::kUnknown;
    expect_run_on_os_login_ = absl::nullopt;
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

    // IsPlatformApp tests.

    if (state) {
      state->is_platform_app = false;
      expect_is_platform_app_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->is_platform_app = true;
      expect_is_platform_app_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_is_platform_app_, state->is_platform_app);
      CheckExpects(u);
    }

    // Recommendable tests.

    if (state) {
      state->recommendable = false;
      expect_recommendable_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->recommendable = true;
      expect_recommendable_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_recommendable_, state->recommendable);
      CheckExpects(u);
    }

    // Searchable tests.

    if (state) {
      state->searchable = false;
      expect_searchable_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->searchable = true;
      expect_searchable_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_searchable_, state->searchable);
      CheckExpects(u);
    }

    // ShowInLauncher tests.

    if (state) {
      state->show_in_launcher = false;
      expect_show_in_launcher_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_launcher = true;
      expect_show_in_launcher_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_launcher_, state->show_in_launcher);
      CheckExpects(u);
    }

    // ShowInShelf tests.

    if (state) {
      state->show_in_shelf = false;
      expect_show_in_shelf_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_shelf = true;
      expect_show_in_shelf_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_shelf_, state->show_in_shelf);
      CheckExpects(u);
    }

    // ShowInSearch tests.

    if (state) {
      state->show_in_search = false;
      expect_show_in_search_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_search = true;
      expect_show_in_search_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_search_, state->show_in_search);
      CheckExpects(u);
    }

    // ShowInManagement tests.

    if (state) {
      state->show_in_management = false;
      expect_show_in_management_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_management = true;
      expect_show_in_management_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_management_, state->show_in_management);
      CheckExpects(u);
    }

    // HandlesIntents tests.

    if (state) {
      state->handles_intents = false;
      expect_handles_intents_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->handles_intents = true;
      expect_handles_intents_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_handles_intents_, state->handles_intents);
      CheckExpects(u);
    }

    // AllowUninstall tests

    if (state) {
      state->allow_uninstall = false;
      expect_allow_uninstall_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->allow_uninstall = true;
      expect_allow_uninstall_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_allow_uninstall_, state->allow_uninstall);
      CheckExpects(u);
    }

    // HasBadge tests.

    if (state) {
      state->has_badge = false;
      expect_has_badge_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->has_badge = true;
      expect_has_badge_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_has_badge_, state->has_badge);
      CheckExpects(u);
    }

    // Pause tests.

    if (state) {
      state->paused = false;
      expect_paused_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->paused = true;
      expect_paused_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_paused_, state->paused);
      CheckExpects(u);
    }

    // Intent Filter tests.

    if (state) {
      IntentFilterPtr intent_filter = std::make_unique<IntentFilter>();

      ConditionValues scheme_condition_values;
      scheme_condition_values.push_back(
          std::make_unique<ConditionValue>("https", PatternMatchType::kNone));
      ConditionPtr scheme_condition = std::make_unique<Condition>(
          ConditionType::kScheme, std::move(scheme_condition_values));

      ConditionValues host_condition_values;
      host_condition_values.push_back(std::make_unique<ConditionValue>(
          "www.google.com", PatternMatchType::kNone));
      auto host_condition = std::make_unique<Condition>(
          ConditionType::kHost, std::move(host_condition_values));

      intent_filter->conditions.push_back(std::move(scheme_condition));
      intent_filter->conditions.push_back(std::move(host_condition));

      state->intent_filters.push_back(intent_filter->Clone());
      expect_intent_filters_.push_back(intent_filter->Clone());
      CheckExpects(u);
    }

    if (delta) {
      expect_intent_filters_.clear();

      IntentFilterPtr intent_filter = std::make_unique<IntentFilter>();

      ConditionValues scheme_condition_values;
      scheme_condition_values.push_back(
          std::make_unique<ConditionValue>("https", PatternMatchType::kNone));
      ConditionPtr scheme_condition = std::make_unique<Condition>(
          ConditionType::kScheme, std::move(scheme_condition_values));
      intent_filter->conditions.push_back(scheme_condition->Clone());

      ConditionValues host_condition_values;
      host_condition_values.push_back(std::make_unique<ConditionValue>(
          "www.abc.com", PatternMatchType::kNone));
      auto host_condition = std::make_unique<Condition>(
          ConditionType::kHost, std::move(host_condition_values));
      intent_filter->conditions.push_back(host_condition->Clone());

      intent_filter->conditions.push_back(std::move(scheme_condition));
      intent_filter->conditions.push_back(std::move(host_condition));

      delta->intent_filters.push_back(intent_filter->Clone());
      expect_intent_filters_.push_back(intent_filter->Clone());
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_TRUE(IsEqual(expect_intent_filters_, state->intent_filters));
      CheckExpects(u);
    }

    // ResizeLocked tests.

    if (state) {
      state->resize_locked = false;
      expect_resize_locked_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->resize_locked = true;
      expect_resize_locked_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_resize_locked_, state->resize_locked);
      CheckExpects(u);
    }

    // WindowMode tests.

    if (state) {
      state->window_mode = WindowMode::kBrowser;
      expect_window_mode_ = WindowMode::kBrowser;
      CheckExpects(u);
    }

    if (delta) {
      delta->window_mode = WindowMode::kWindow;
      expect_window_mode_ = WindowMode::kWindow;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_window_mode_, state->window_mode);
      CheckExpects(u);
    }

    // RunOnOsLogin tests.

    if (state) {
      state->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
      expect_run_on_os_login_ = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
      CheckExpects(u);
    }

    if (delta) {
      delta->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kWindowed, false);
      expect_run_on_os_login_ =
          RunOnOsLogin(RunOnOsLoginMode::kWindowed, false);
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_run_on_os_login_.value(),
                state->run_on_os_login.value());
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
