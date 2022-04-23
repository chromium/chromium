// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const apps::mojom::AppType app_type = apps::mojom::AppType::kArc;
const char app_id[] = "abcdefgh";
const char test_name_0[] = "Inigo Montoya";
const char test_name_1[] = "Dread Pirate Roberts";
}  // namespace

class AppUpdateMojomTest : public testing::Test {
 protected:
  AppUpdateMojomTest() {
    scoped_feature_list_.InitAndDisableFeature(
        apps::kAppServiceOnAppUpdateWithoutMojom);
  }

  apps::Readiness expect_readiness_;
  apps::Readiness expect_prior_readiness_;
  bool expect_readiness_changed_;

  std::string expect_name_;
  bool expect_name_changed_;

  std::string expect_short_name_;
  bool expect_short_name_changed_;

  std::string expect_publisher_id_;
  bool expect_publisher_id_changed_;

  std::string expect_description_;
  bool expect_description_changed_;

  std::string expect_version_;
  bool expect_version_changed_;

  std::vector<std::string> expect_additional_search_terms_;
  bool expect_additional_search_terms_changed_;

  absl::optional<apps::IconKey> expect_icon_key_;
  bool expect_icon_key_changed_;

  base::Time expect_last_launch_time_;
  bool expect_last_launch_time_changed_;

  base::Time expect_install_time_;
  bool expect_install_time_changed_;

  apps::Permissions expect_permissions_;
  bool expect_permissions_changed_;

  apps::InstallReason expect_install_reason_;
  bool expect_install_reason_changed_;

  apps::InstallSource expect_install_source_;
  bool expect_install_source_changed_;

  std::string expect_policy_id_;
  bool expect_policy_id_changed_;

  absl::optional<bool> expect_is_platform_app_;
  bool expect_is_platform_app_changed_;

  absl::optional<bool> expect_recommendable_;
  bool expect_recommendable_changed_;

  absl::optional<bool> expect_searchable_;
  bool expect_searchable_changed_;

  absl::optional<bool> expect_show_in_launcher_;
  bool expect_show_in_launcher_changed_;

  absl::optional<bool> expect_show_in_shelf_;
  bool expect_show_in_shelf_changed_;

  absl::optional<bool> expect_show_in_search_;
  bool expect_show_in_search_changed_;

  absl::optional<bool> expect_show_in_management_;
  bool expect_show_in_management_changed_;

  absl::optional<bool> expect_handles_intents_;
  bool expect_handles_intents_changed_;

  absl::optional<bool> expect_allow_uninstall_;
  bool expect_allow_uninstall_changed_;

  absl::optional<bool> expect_has_badge_;
  bool expect_has_badge_changed_;

  absl::optional<bool> expect_paused_;
  bool expect_paused_changed_;

  std::vector<apps::IntentFilterPtr> expect_intent_filters_;
  bool expect_intent_filters_changed_;

  absl::optional<bool> expect_resize_locked_;
  bool expect_resize_locked_changed_;

  apps::WindowMode expect_window_mode_;
  bool expect_window_mode_changed_;

  absl::optional<apps::RunOnOsLogin> expect_run_on_os_login_;
  bool expect_run_on_os_login_changed_;

  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");

  apps::mojom::PermissionPtr MakePermission(
      apps::mojom::PermissionType permission_type,
      apps::mojom::TriState value) {
    apps::mojom::PermissionPtr permission = apps::mojom::Permission::New();
    permission->permission_type = permission_type;
    permission->value = apps::mojom::PermissionValue::NewTristateValue(value);
    return permission;
  }

  void ExpectNoChange() {
    expect_readiness_changed_ = false;
    expect_name_changed_ = false;
    expect_short_name_changed_ = false;
    expect_publisher_id_changed_ = false;
    expect_description_changed_ = false;
    expect_version_changed_ = false;
    expect_additional_search_terms_changed_ = false;
    expect_icon_key_changed_ = false;
    expect_last_launch_time_changed_ = false;
    expect_install_time_changed_ = false;
    expect_permissions_changed_ = false;
    expect_install_reason_changed_ = false;
    expect_install_source_changed_ = false;
    expect_policy_id_changed_ = false;
    expect_is_platform_app_changed_ = false;
    expect_recommendable_changed_ = false;
    expect_searchable_changed_ = false;
    expect_show_in_launcher_changed_ = false;
    expect_show_in_shelf_changed_ = false;
    expect_show_in_search_changed_ = false;
    expect_show_in_management_changed_ = false;
    expect_handles_intents_changed_ = false;
    expect_allow_uninstall_changed_ = false;
    expect_has_badge_changed_ = false;
    expect_paused_changed_ = false;
    expect_intent_filters_changed_ = false;
    expect_resize_locked_changed_ = false;
    expect_window_mode_changed_ = false;
    expect_run_on_os_login_changed_ = false;
  }

  void CheckExpects(const apps::AppUpdate& u) {
    EXPECT_EQ(expect_readiness_, u.Readiness());
    EXPECT_EQ(expect_prior_readiness_, u.PriorReadiness());
    EXPECT_EQ(expect_readiness_changed_, u.ReadinessChanged());

    EXPECT_EQ(expect_name_, u.Name());
    EXPECT_EQ(expect_name_changed_, u.NameChanged());

    EXPECT_EQ(expect_short_name_, u.ShortName());
    EXPECT_EQ(expect_short_name_changed_, u.ShortNameChanged());

    EXPECT_EQ(expect_publisher_id_, u.PublisherId());
    EXPECT_EQ(expect_publisher_id_changed_, u.PublisherIdChanged());

    EXPECT_EQ(expect_description_, u.Description());
    EXPECT_EQ(expect_description_changed_, u.DescriptionChanged());

    EXPECT_EQ(expect_version_, u.Version());
    EXPECT_EQ(expect_version_changed_, u.VersionChanged());

    EXPECT_EQ(expect_additional_search_terms_, u.AdditionalSearchTerms());
    EXPECT_EQ(expect_additional_search_terms_changed_,
              u.AdditionalSearchTermsChanged());

    EXPECT_EQ(expect_icon_key_, u.IconKey());
    EXPECT_EQ(expect_icon_key_changed_, u.IconKeyChanged());

    EXPECT_EQ(expect_last_launch_time_, u.LastLaunchTime());
    EXPECT_EQ(expect_last_launch_time_changed_, u.LastLaunchTimeChanged());

    EXPECT_EQ(expect_install_time_, u.InstallTime());
    EXPECT_EQ(expect_install_time_changed_, u.InstallTimeChanged());

    EXPECT_TRUE(IsEqual(expect_permissions_, u.Permissions()));
    EXPECT_EQ(expect_permissions_changed_, u.PermissionsChanged());

    EXPECT_EQ(expect_install_reason_, u.InstallReason());
    EXPECT_EQ(expect_install_reason_changed_, u.InstallReasonChanged());

    EXPECT_EQ(expect_install_source_, u.InstallSource());
    EXPECT_EQ(expect_install_source_changed_, u.InstallSourceChanged());

    EXPECT_EQ(expect_policy_id_, u.PolicyId());
    EXPECT_EQ(expect_policy_id_changed_, u.PolicyIdChanged());

    EXPECT_EQ(expect_is_platform_app_, u.IsPlatformApp());
    EXPECT_EQ(expect_is_platform_app_changed_, u.IsPlatformAppChanged());

    EXPECT_EQ(expect_recommendable_, u.Recommendable());
    EXPECT_EQ(expect_recommendable_changed_, u.RecommendableChanged());

    EXPECT_EQ(expect_searchable_, u.Searchable());
    EXPECT_EQ(expect_searchable_changed_, u.SearchableChanged());

    EXPECT_EQ(expect_show_in_launcher_, u.ShowInLauncher());
    EXPECT_EQ(expect_show_in_launcher_changed_, u.ShowInLauncherChanged());

    EXPECT_EQ(expect_show_in_shelf_, u.ShowInShelf());
    EXPECT_EQ(expect_show_in_shelf_changed_, u.ShowInShelfChanged());

    EXPECT_EQ(expect_show_in_search_, u.ShowInSearch());
    EXPECT_EQ(expect_show_in_search_changed_, u.ShowInSearchChanged());

    EXPECT_EQ(expect_show_in_management_, u.ShowInManagement());
    EXPECT_EQ(expect_show_in_management_changed_, u.ShowInManagementChanged());

    EXPECT_EQ(expect_handles_intents_, u.HandlesIntents());
    EXPECT_EQ(expect_handles_intents_changed_, u.HandlesIntentsChanged());

    EXPECT_EQ(expect_allow_uninstall_, u.AllowUninstall());
    EXPECT_EQ(expect_allow_uninstall_changed_, u.AllowUninstallChanged());

    EXPECT_EQ(expect_has_badge_, u.HasBadge());
    EXPECT_EQ(expect_has_badge_changed_, u.HasBadgeChanged());

    EXPECT_EQ(expect_paused_, u.Paused());
    EXPECT_EQ(expect_paused_changed_, u.PausedChanged());

    EXPECT_TRUE(IsEqual(expect_intent_filters_, u.IntentFilters()));
    EXPECT_EQ(expect_intent_filters_changed_, u.IntentFiltersChanged());

    EXPECT_EQ(expect_resize_locked_, u.ResizeLocked());
    EXPECT_EQ(expect_resize_locked_changed_, u.ResizeLockedChanged());

    EXPECT_EQ(expect_window_mode_, u.WindowMode());
    EXPECT_EQ(expect_window_mode_changed_, u.WindowModeChanged());

    EXPECT_EQ(expect_run_on_os_login_, u.RunOnOsLogin());
    EXPECT_EQ(expect_run_on_os_login_changed_, u.RunOnOsLoginChanged());

    EXPECT_EQ(account_id_, u.AccountId());
  }

  void TestAppUpdate(apps::mojom::App* state, apps::mojom::App* delta) {
    apps::AppUpdate u(state, delta, account_id_);

    EXPECT_EQ(app_type, apps::ConvertAppTypeToMojomAppType(u.AppType()));
    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());

    expect_readiness_ = apps::Readiness::kUnknown;
    expect_prior_readiness_ = apps::Readiness::kUnknown;
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
    expect_install_reason_ = apps::InstallReason::kUnknown;
    expect_install_source_ = apps::InstallSource::kUnknown;
    expect_policy_id_ = "";
    expect_is_platform_app_ = absl::nullopt;
    expect_recommendable_ = absl::nullopt;
    expect_searchable_ = absl::nullopt;
    expect_show_in_launcher_ = absl::nullopt;
    expect_show_in_shelf_ = absl::nullopt;
    expect_show_in_search_ = absl::nullopt;
    expect_show_in_management_ = absl::nullopt;
    expect_handles_intents_ = absl::nullopt;
    expect_allow_uninstall_ = absl::nullopt;
    expect_has_badge_ = absl::nullopt;
    expect_paused_ = absl::nullopt;
    expect_intent_filters_.clear();
    expect_resize_locked_ = absl::nullopt;
    expect_window_mode_ = apps::WindowMode::kUnknown;
    expect_run_on_os_login_ = absl::nullopt;
    ExpectNoChange();
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
      delta->readiness = apps::mojom::Readiness::kReady;
      expect_readiness_ = apps::Readiness::kReady;
      expect_readiness_changed_ = true;
      CheckExpects(u);

      delta->name = absl::nullopt;
      expect_name_ = state ? test_name_0 : "";
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      expect_prior_readiness_ =
          apps::ConvertMojomReadinessToReadiness(state->readiness);
      ExpectNoChange();
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = apps::mojom::Readiness::kDisabledByPolicy;
      expect_readiness_ = apps::Readiness::kDisabledByPolicy;
      expect_readiness_changed_ = true;
      delta->name = test_name_1;
      expect_name_ = test_name_1;
      expect_name_changed_ = true;
      CheckExpects(u);
    }

    // ShortName tests.

    if (state) {
      state->short_name = "Kate";
      expect_short_name_ = "Kate";
      expect_short_name_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->short_name = "Bob";
      expect_short_name_ = "Bob";
      expect_short_name_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      expect_prior_readiness_ =
          apps::ConvertMojomReadinessToReadiness(state->readiness);
      ExpectNoChange();
      CheckExpects(u);
    }

    // PublisherId tests.

    if (state) {
      state->publisher_id = "com.google.android.youtube";
      expect_publisher_id_ = "com.google.android.youtube";
      expect_publisher_id_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->publisher_id = "com.android.youtube";
      expect_publisher_id_ = "com.android.youtube";
      expect_publisher_id_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Description tests.

    if (state) {
      state->description = "Has a cat.";
      expect_description_ = "Has a cat.";
      expect_description_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->description = "Has a dog.";
      expect_description_ = "Has a dog.";
      expect_description_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Version tests.

    if (state) {
      state->version = "1.0.0";
      expect_version_ = "1.0.0";
      expect_version_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->version = "1.0.1";
      expect_version_ = "1.0.1";
      expect_version_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // AdditionalSearchTerms tests.

    if (state) {
      state->additional_search_terms.push_back("cat");
      state->additional_search_terms.push_back("dog");
      expect_additional_search_terms_.push_back("cat");
      expect_additional_search_terms_.push_back("dog");
      expect_additional_search_terms_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_additional_search_terms_.clear();
      delta->additional_search_terms.push_back("horse");
      delta->additional_search_terms.push_back("mouse");
      expect_additional_search_terms_.push_back("horse");
      expect_additional_search_terms_.push_back("mouse");
      expect_additional_search_terms_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // IconKey tests.

    if (state) {
      auto x = apps::mojom::IconKey::New(100, 0, 0);
      state->icon_key = x.Clone();
      expect_icon_key_ = std::move(*apps::ConvertMojomIconKeyToIconKey(x));
      expect_icon_key_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      auto x = apps::mojom::IconKey::New(200, 0, 0);
      delta->icon_key = x.Clone();
      expect_icon_key_ = std::move(*apps::ConvertMojomIconKeyToIconKey(x));
      expect_icon_key_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // LastLaunchTime tests.

    if (state) {
      state->last_launch_time = base::Time::FromDoubleT(1000.0);
      expect_last_launch_time_ = base::Time::FromDoubleT(1000.0);
      expect_last_launch_time_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->last_launch_time = base::Time::FromDoubleT(1001.0);
      expect_last_launch_time_ = base::Time::FromDoubleT(1001.0);
      expect_last_launch_time_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallTime tests.

    if (state) {
      state->install_time = base::Time::FromDoubleT(2000.0);
      expect_install_time_ = base::Time::FromDoubleT(2000.0);
      expect_install_time_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_time = base::Time::FromDoubleT(2001.0);
      expect_install_time_ = base::Time::FromDoubleT(2001.0);
      expect_install_time_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallReason tests.
    if (state) {
      state->install_reason = apps::mojom::InstallReason::kUser;
      expect_install_reason_ = apps::InstallReason::kUser;
      expect_install_reason_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_reason = apps::mojom::InstallReason::kPolicy;
      expect_install_reason_ = apps::InstallReason::kPolicy;
      expect_install_reason_changed_ = true;
      CheckExpects(u);
    }

    // PolicyId tests.
    if (state) {
      state->policy_id = "https://app.site/alpha";
      expect_policy_id_ = "https://app.site/alpha";
      expect_policy_id_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->policy_id = "https://app.site/delta";
      expect_policy_id_ = "https://app.site/delta";
      expect_policy_id_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallSource tests.
    if (state) {
      state->install_source = apps::mojom::InstallSource::kPlayStore;
      expect_install_source_ = apps::InstallSource::kPlayStore;
      expect_install_source_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_source = apps::mojom::InstallSource::kSync;
      expect_install_source_ = apps::InstallSource::kSync;
      expect_install_source_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // IsPlatformApp tests.

    if (state) {
      state->is_platform_app = apps::mojom::OptionalBool::kFalse;
      expect_is_platform_app_ = false;
      expect_is_platform_app_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->is_platform_app = apps::mojom::OptionalBool::kTrue;
      expect_is_platform_app_ = true;
      expect_is_platform_app_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Recommendable tests.

    if (state) {
      state->recommendable = apps::mojom::OptionalBool::kFalse;
      expect_recommendable_ = false;
      expect_recommendable_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->recommendable = apps::mojom::OptionalBool::kTrue;
      expect_recommendable_ = true;
      expect_recommendable_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Searchable tests.

    if (state) {
      state->searchable = apps::mojom::OptionalBool::kFalse;
      expect_searchable_ = false;
      expect_searchable_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->searchable = apps::mojom::OptionalBool::kTrue;
      expect_searchable_ = true;
      expect_searchable_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInLauncher tests.

    if (state) {
      state->show_in_launcher = apps::mojom::OptionalBool::kFalse;
      expect_show_in_launcher_ = false;
      expect_show_in_launcher_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_launcher = apps::mojom::OptionalBool::kTrue;
      expect_show_in_launcher_ = true;
      expect_show_in_launcher_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInShelf tests.

    if (state) {
      state->show_in_shelf = apps::mojom::OptionalBool::kFalse;
      expect_show_in_shelf_ = false;
      expect_show_in_shelf_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_shelf = apps::mojom::OptionalBool::kTrue;
      expect_show_in_shelf_ = true;
      expect_show_in_shelf_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInSearch tests.

    if (state) {
      state->show_in_search = apps::mojom::OptionalBool::kFalse;
      expect_show_in_search_ = false;
      expect_show_in_search_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_search = apps::mojom::OptionalBool::kTrue;
      expect_show_in_search_ = true;
      expect_show_in_search_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInManagement tests.

    if (state) {
      state->show_in_management = apps::mojom::OptionalBool::kFalse;
      expect_show_in_management_ = false;
      expect_show_in_management_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_management = apps::mojom::OptionalBool::kTrue;
      expect_show_in_management_ = true;
      expect_show_in_management_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // HandlesIntents tests.

    if (state) {
      state->handles_intents = apps::mojom::OptionalBool::kFalse;
      expect_handles_intents_ = false;
      expect_handles_intents_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->handles_intents = apps::mojom::OptionalBool::kTrue;
      expect_handles_intents_ = true;
      expect_handles_intents_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // AllowUninstall tests

    if (state) {
      state->allow_uninstall = apps::mojom::OptionalBool::kFalse;
      expect_allow_uninstall_ = false;
      expect_allow_uninstall_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->allow_uninstall = apps::mojom::OptionalBool::kTrue;
      expect_allow_uninstall_ = true;
      expect_allow_uninstall_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // HasBadge tests.

    if (state) {
      state->has_badge = apps::mojom::OptionalBool::kFalse;
      expect_has_badge_ = false;
      expect_has_badge_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->has_badge = apps::mojom::OptionalBool::kTrue;
      expect_has_badge_ = true;
      expect_has_badge_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Pause tests.

    if (state) {
      state->paused = apps::mojom::OptionalBool::kFalse;
      expect_paused_ = false;
      expect_paused_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->paused = apps::mojom::OptionalBool::kTrue;
      expect_paused_ = true;
      expect_paused_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Permission tests.

    if (state) {
      auto p0 = MakePermission(apps::mojom::PermissionType::kLocation,
                               apps::mojom::TriState::kAllow);
      auto p1 = MakePermission(apps::mojom::PermissionType::kNotifications,
                               apps::mojom::TriState::kAllow);
      state->permissions.push_back(p0.Clone());
      state->permissions.push_back(p1.Clone());
      expect_permissions_.push_back(
          apps::ConvertMojomPermissionToPermission(p0));
      expect_permissions_.push_back(
          apps::ConvertMojomPermissionToPermission(p1));
      expect_permissions_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_permissions_.clear();
      auto p0 = MakePermission(apps::mojom::PermissionType::kNotifications,
                               apps::mojom::TriState::kAllow);
      auto p1 = MakePermission(apps::mojom::PermissionType::kLocation,
                               apps::mojom::TriState::kBlock);

      delta->permissions.push_back(p0.Clone());
      delta->permissions.push_back(p1.Clone());
      expect_permissions_.push_back(
          apps::ConvertMojomPermissionToPermission(p0));
      expect_permissions_.push_back(
          apps::ConvertMojomPermissionToPermission(p1));
      expect_permissions_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Intent Filter tests.

    if (state) {
      auto intent_filter = apps::mojom::IntentFilter::New();

      std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
      scheme_condition_values.push_back(apps_util::MakeConditionValue(
          "https", apps::mojom::PatternMatchType::kNone));
      auto scheme_condition =
          apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                   std::move(scheme_condition_values));

      std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
      host_condition_values.push_back(apps_util::MakeConditionValue(
          "www.google.com", apps::mojom::PatternMatchType::kNone));
      auto host_condition = apps_util::MakeCondition(
          apps::mojom::ConditionType::kHost, std::move(host_condition_values));

      intent_filter->conditions.push_back(std::move(scheme_condition));
      intent_filter->conditions.push_back(std::move(host_condition));

      state->intent_filters.push_back(intent_filter.Clone());
      expect_intent_filters_.push_back(
          apps::ConvertMojomIntentFilterToIntentFilter(intent_filter));
      expect_intent_filters_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_intent_filters_.clear();

      auto intent_filter = apps::mojom::IntentFilter::New();

      std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
      scheme_condition_values.push_back(apps_util::MakeConditionValue(
          "https", apps::mojom::PatternMatchType::kNone));
      auto scheme_condition =
          apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                   std::move(scheme_condition_values));
      intent_filter->conditions.push_back(scheme_condition.Clone());

      std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
      host_condition_values.push_back(apps_util::MakeConditionValue(
          "www.abc.com", apps::mojom::PatternMatchType::kNone));
      auto host_condition = apps_util::MakeCondition(
          apps::mojom::ConditionType::kHost, std::move(host_condition_values));
      intent_filter->conditions.push_back(host_condition.Clone());

      intent_filter->conditions.push_back(std::move(scheme_condition));
      intent_filter->conditions.push_back(std::move(host_condition));

      delta->intent_filters.push_back(intent_filter.Clone());
      expect_intent_filters_.push_back(
          apps::ConvertMojomIntentFilterToIntentFilter(intent_filter));
      expect_intent_filters_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ResizeLocked tests.

    if (state) {
      state->resize_locked = apps::mojom::OptionalBool::kFalse;
      expect_resize_locked_ = false;
      expect_resize_locked_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->resize_locked = apps::mojom::OptionalBool::kTrue;
      expect_resize_locked_ = true;
      expect_resize_locked_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // WindowMode tests.

    if (state) {
      state->window_mode = apps::mojom::WindowMode::kBrowser;
      expect_window_mode_ = apps::WindowMode::kBrowser;
      expect_window_mode_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->window_mode = apps::mojom::WindowMode::kWindow;
      expect_window_mode_ = apps::WindowMode::kWindow;
      expect_window_mode_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // RunOnOsLogin tests.

    if (state) {
      apps::RunOnOsLogin value(apps::RunOnOsLoginMode::kNotRun, false);
      state->run_on_os_login =
          apps::ConvertRunOnOsLoginToMojomRunOnOsLogin(value);
      expect_run_on_os_login_ = std::move(value);
      expect_run_on_os_login_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      apps::RunOnOsLogin value(apps::RunOnOsLoginMode::kWindowed, false);
      delta->run_on_os_login =
          apps::ConvertRunOnOsLoginToMojomRunOnOsLogin(value);
      expect_run_on_os_login_ = std::move(value);
      expect_run_on_os_login_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppUpdateMojomTest, StateIsNonNull) {
  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  TestAppUpdate(state.get(), nullptr);
}

TEST_F(AppUpdateMojomTest, DeltaIsNonNull) {
  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  TestAppUpdate(nullptr, delta.get());
}

TEST_F(AppUpdateMojomTest, BothAreNonNull) {
  apps::mojom::AppPtr state = apps::mojom::App::New();
  state->app_type = app_type;
  state->app_id = app_id;

  apps::mojom::AppPtr delta = apps::mojom::App::New();
  delta->app_type = app_type;
  delta->app_id = app_id;

  TestAppUpdate(state.get(), delta.get());
}

TEST_F(AppUpdateMojomTest, AppConvert) {
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
  input->is_platform_app = apps::mojom::OptionalBool::kFalse;
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
  permission->value = apps::mojom::PermissionValue::NewBoolValue(true);
  permission->is_managed = true;
  input->permissions.push_back(std::move(permission));

  input->allow_uninstall = apps::mojom::OptionalBool::kTrue;
  input->handles_intents = apps::mojom::OptionalBool::kTrue;

  auto output = apps::ConvertMojomAppToApp(input);

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
  EXPECT_FALSE(output->is_platform_app.value());
  EXPECT_TRUE(output->recommendable.value());
  EXPECT_TRUE(output->searchable.value());
  EXPECT_FALSE(output->paused.value());
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

  auto mojom_app = apps::ConvertAppToMojomApp(output);

  EXPECT_EQ(mojom_app->app_type, apps::mojom::AppType::kWeb);
  EXPECT_EQ(mojom_app->app_id, "abcdefg");
  EXPECT_EQ(mojom_app->readiness, apps::mojom::Readiness::kReady);
  EXPECT_EQ(mojom_app->name, "lacros test name");
  EXPECT_EQ(mojom_app->short_name, "lacros test name");
  EXPECT_EQ(mojom_app->publisher_id, "publisher_id");
  EXPECT_EQ(mojom_app->description, "description");
  EXPECT_EQ(mojom_app->version, "version");
  EXPECT_EQ(mojom_app->additional_search_terms, input->additional_search_terms);

  EXPECT_EQ(mojom_app->icon_key->timeline, 1U);
  EXPECT_EQ(mojom_app->icon_key->icon_effects, 2U);

  EXPECT_EQ(mojom_app->last_launch_time, base::Time() + base::Days(1));
  EXPECT_EQ(mojom_app->install_time, base::Time() + base::Days(2));

  EXPECT_EQ(mojom_app->install_reason, apps::mojom::InstallReason::kUser);
  EXPECT_EQ(mojom_app->policy_id, "https://app.site/alpha");
  EXPECT_EQ(mojom_app->recommendable, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->searchable, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->paused, apps::mojom::OptionalBool::kFalse);
  EXPECT_EQ(mojom_app->show_in_launcher, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->show_in_shelf, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->show_in_search, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->show_in_management, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->has_badge, apps::mojom::OptionalBool::kUnknown);
  EXPECT_EQ(mojom_app->paused, apps::mojom::OptionalBool::kFalse);

  ASSERT_EQ(mojom_app->intent_filters.size(), 1U);
  auto& mojom_filter = mojom_app->intent_filters[0];
  ASSERT_EQ(mojom_filter->conditions.size(), 1U);
  auto& mojom_condition = mojom_filter->conditions[0];
  EXPECT_EQ(mojom_condition->condition_type,
            apps::mojom::ConditionType::kScheme);
  ASSERT_EQ(mojom_condition->condition_values.size(), 1U);
  EXPECT_EQ(mojom_condition->condition_values[0]->value, "https");
  EXPECT_EQ(mojom_condition->condition_values[0]->match_type,
            apps::mojom::PatternMatchType::kNone);
  EXPECT_EQ(mojom_filter->activity_name, "activity_name");
  EXPECT_EQ(mojom_filter->activity_label, "activity_label");

  EXPECT_EQ(mojom_app->window_mode, apps::mojom::WindowMode::kWindow);

  ASSERT_EQ(mojom_app->permissions.size(), 1U);
  auto& mojom_permission = mojom_app->permissions[0];
  EXPECT_EQ(mojom_permission->permission_type,
            apps::mojom::PermissionType::kCamera);
  ASSERT_TRUE(mojom_permission->value->is_bool_value());
  EXPECT_TRUE(mojom_permission->value->get_bool_value());
  EXPECT_TRUE(mojom_permission->is_managed);

  EXPECT_EQ(mojom_app->allow_uninstall, apps::mojom::OptionalBool::kTrue);
  EXPECT_EQ(mojom_app->handles_intents, apps::mojom::OptionalBool::kTrue);
}
