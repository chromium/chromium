// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_update.h"

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
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
  return std::make_unique<Permission>(permission_type, tri_state, is_managed);
}

PermissionPtr MakePermission(PermissionType permission_type,
                             bool bool_value,
                             bool is_managed) {
  return std::make_unique<Permission>(permission_type, bool_value, is_managed);
}

}  // namespace

class AppUpdateTest : public testing::Test {
 protected:
  bool expect_changed_;

  Readiness expect_readiness_;
  Readiness expect_prior_readiness_;
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

  std::optional<IconKey> expect_icon_key_;
  bool expect_icon_key_changed_;

  base::Time expect_last_launch_time_;
  bool expect_last_launch_time_changed_;

  base::Time expect_install_time_;
  bool expect_install_time_changed_;

  Permissions expect_permissions_;
  bool expect_permissions_changed_;

  InstallReason expect_install_reason_;
  bool expect_install_reason_changed_;

  InstallSource expect_install_source_;
  bool expect_install_source_changed_;

  std::vector<std::string> expect_policy_ids_;
  bool expect_policy_ids_changed_;

  std::optional<bool> expect_is_platform_app_;
  bool expect_is_platform_app_changed_;

  std::optional<bool> expect_recommendable_;
  bool expect_recommendable_changed_;

  std::optional<bool> expect_searchable_;
  bool expect_searchable_changed_;

  std::optional<bool> expect_show_in_launcher_;
  bool expect_show_in_launcher_changed_;

  std::optional<bool> expect_show_in_shelf_;
  bool expect_show_in_shelf_changed_;

  std::optional<bool> expect_show_in_search_;
  bool expect_show_in_search_changed_;

  std::optional<bool> expect_show_in_management_;
  bool expect_show_in_management_changed_;

  std::optional<bool> expect_handles_intents_;
  bool expect_handles_intents_changed_;

  std::optional<bool> expect_allow_uninstall_;
  bool expect_allow_uninstall_changed_;

  std::optional<bool> expect_has_badge_;
  bool expect_has_badge_changed_;

  std::optional<bool> expect_paused_;
  bool expect_paused_changed_;

  IntentFilters expect_intent_filters_;
  bool expect_intent_filters_changed_;

  std::optional<bool> expect_resize_locked_;
  bool expect_resize_locked_changed_;

  std::optional<bool> expect_allow_window_mode_selection_;
  bool expect_allow_window_mode_selection_changed_;

  WindowMode expect_window_mode_;
  bool expect_window_mode_changed_;

  std::optional<RunOnOsLogin> expect_run_on_os_login_;
  bool expect_run_on_os_login_changed_;

  std::optional<bool> expect_allow_close_;
  bool expect_allow_close_changed_;

  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");

  std::optional<uint64_t> expect_app_size_in_bytes_;
  bool expect_app_size_in_bytes_changed_;

  std::optional<uint64_t> expect_data_size_in_bytes_;
  bool expect_data_size_in_bytes_changed_;

  std::vector<std::string> expect_supported_locales_;
  bool expect_supported_locales_changed_;

  std::optional<std::string> expect_selected_locale_;
  bool expect_selected_locale_changed_;

  std::optional<base::Value::Dict> expect_extra_;
  bool expect_extra_changed_;

  void ExpectNoChange() {
    expect_changed_ = false;
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
    expect_policy_ids_changed_ = false;
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
    expect_allow_window_mode_selection_changed_ = false;
    expect_window_mode_changed_ = false;
    expect_run_on_os_login_changed_ = false;
    expect_allow_close_changed_ = false;
    expect_app_size_in_bytes_changed_ = false;
    expect_data_size_in_bytes_changed_ = false;
    expect_supported_locales_changed_ = false;
    expect_selected_locale_changed_ = false;
  }

  void CheckExpects(const AppUpdate& u) {
    EXPECT_EQ(expect_changed_, AppUpdate::IsChanged(u.State(), u.Delta()));

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

    EXPECT_THAT(u.PolicyIds(),
                testing::UnorderedElementsAreArray(expect_policy_ids_));
    EXPECT_EQ(expect_policy_ids_changed_, u.PolicyIdsChanged());

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

    EXPECT_EQ(expect_allow_window_mode_selection_,
              u.AllowWindowModeSelection());
    EXPECT_EQ(expect_allow_window_mode_selection_changed_,
              u.AllowWindowModeSelectionChanged());

    EXPECT_EQ(expect_window_mode_, u.WindowMode());
    EXPECT_EQ(expect_window_mode_changed_, u.WindowModeChanged());

    EXPECT_EQ(expect_run_on_os_login_, u.RunOnOsLogin());
    EXPECT_EQ(expect_run_on_os_login_changed_, u.RunOnOsLoginChanged());

    EXPECT_EQ(expect_allow_close_, u.AllowClose());
    EXPECT_EQ(expect_allow_close_changed_, u.AllowCloseChanged());

    EXPECT_EQ(account_id_, u.AccountId());

    EXPECT_EQ(expect_app_size_in_bytes_, u.AppSizeInBytes());
    EXPECT_EQ(expect_app_size_in_bytes_changed_, u.AppSizeInBytesChanged());

    EXPECT_EQ(expect_data_size_in_bytes_, u.DataSizeInBytes());
    EXPECT_EQ(expect_data_size_in_bytes_changed_, u.DataSizeInBytesChanged());

    EXPECT_EQ(expect_supported_locales_changed_, u.SupportedLocalesChanged());
    EXPECT_EQ(expect_selected_locale_changed_, u.SelectedLocaleChanged());
  }

  void TestAppUpdate(App* state, App* delta) {
    AppUpdate u(state, delta, account_id_);

    EXPECT_EQ(app_type, u.AppType());
    EXPECT_EQ(app_id, u.AppId());

    expect_readiness_ = Readiness::kUnknown;
    expect_prior_readiness_ = Readiness::kUnknown;
    expect_name_ = "";
    expect_short_name_ = "";
    expect_publisher_id_ = "";
    expect_description_ = "";
    expect_version_ = "";
    expect_additional_search_terms_.clear();
    expect_icon_key_ = std::nullopt;
    expect_last_launch_time_ = base::Time();
    expect_install_time_ = base::Time();
    expect_permissions_.clear();
    expect_install_reason_ = InstallReason::kUnknown;
    expect_install_source_ = InstallSource::kUnknown;
    expect_policy_ids_ = {};
    expect_is_platform_app_ = std::nullopt;
    expect_recommendable_ = std::nullopt;
    expect_searchable_ = std::nullopt;
    expect_show_in_launcher_ = std::nullopt;
    expect_show_in_shelf_ = std::nullopt;
    expect_show_in_search_ = std::nullopt;
    expect_show_in_management_ = std::nullopt;
    expect_handles_intents_ = std::nullopt;
    expect_has_badge_ = std::nullopt;
    expect_paused_ = std::nullopt;
    expect_allow_window_mode_selection_ = true;
    expect_intent_filters_.clear();
    expect_resize_locked_ = std::nullopt;
    expect_window_mode_ = WindowMode::kUnknown;
    expect_run_on_os_login_ = std::nullopt;
    expect_allow_close_ = std::nullopt;
    expect_app_size_in_bytes_ = std::nullopt;
    expect_data_size_in_bytes_ = std::nullopt;
    expect_supported_locales_.clear();
    expect_selected_locale_ = std::nullopt;
    ExpectNoChange();

    if (!state && delta) {
      expect_changed_ = true;
    }

    CheckExpects(u);

    if (delta) {
      delta->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      state->name = test_name_0;
      expect_name_ = test_name_0;
      expect_name_changed_ = false;
      expect_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = Readiness::kReady;
      expect_readiness_ = Readiness::kReady;
      expect_readiness_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);

      delta->name = std::nullopt;
      expect_name_ = state ? test_name_0 : "";
      expect_name_changed_ = false;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      expect_prior_readiness_ = state->readiness;
      EXPECT_EQ(expect_name_, state->name);
      EXPECT_EQ(expect_readiness_, state->readiness);
      ExpectNoChange();
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = Readiness::kDisabledByPolicy;
      expect_readiness_ = Readiness::kDisabledByPolicy;
      expect_readiness_changed_ = true;
      delta->name = test_name_1;
      expect_name_ = test_name_1;
      expect_name_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (delta) {
      delta->readiness = Readiness::kDisabledByLocalSettings;
      expect_readiness_ = Readiness::kDisabledByLocalSettings;
      expect_readiness_changed_ = true;
      expect_changed_ = true;
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
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      expect_prior_readiness_ = state->readiness;
      EXPECT_EQ(expect_short_name_, state->short_name);
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
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_publisher_id_, state->publisher_id);
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
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_description_, state->description);
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
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_version_, state->version);
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
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_additional_search_terms_,
                state->additional_search_terms);
      ExpectNoChange();
      CheckExpects(u);
    }

    // IconKey tests.

    if (state) {
      state->icon_key = IconKey();
      state->icon_key->update_version = 100;
      expect_icon_key_ = IconKey();
      expect_icon_key_->update_version = 100;
      expect_icon_key_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->icon_key = IconKey(/*raw_icon_updated=*/true, IconEffects::kNone);
      expect_icon_key_ = IconKey();
      expect_icon_key_->update_version =
          (state && state->icon_key.has_value())
              ? absl::get<int32_t>(state->icon_key->update_version) + 1
              : IconKey::kInitVersion;
      expect_icon_key_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      if (delta) {
        delta->icon_key->update_version = false;
      }
      EXPECT_EQ(expect_icon_key_.value(), state->icon_key.value());
      ExpectNoChange();
      CheckExpects(u);
    }

    // LastLaunchTime tests.

    if (state) {
      state->last_launch_time = base::Time::FromSecondsSinceUnixEpoch(1000);
      expect_last_launch_time_ = base::Time::FromSecondsSinceUnixEpoch(1000);
      expect_last_launch_time_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->last_launch_time = base::Time::FromSecondsSinceUnixEpoch(1001);
      expect_last_launch_time_ = base::Time::FromSecondsSinceUnixEpoch(1001);
      expect_last_launch_time_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_last_launch_time_, state->last_launch_time);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallTime tests.

    if (state) {
      state->install_time = base::Time::FromSecondsSinceUnixEpoch(2000);
      expect_install_time_ = base::Time::FromSecondsSinceUnixEpoch(2000);
      expect_install_time_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_time = base::Time::FromSecondsSinceUnixEpoch(2001);
      expect_install_time_ = base::Time::FromSecondsSinceUnixEpoch(2001);
      expect_install_time_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_install_time_, state->install_time);
      ExpectNoChange();
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
      expect_permissions_changed_ = false;
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
      expect_permissions_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_TRUE(IsEqual(expect_permissions_, state->permissions));
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallReason tests.

    if (state) {
      state->install_reason = InstallReason::kUser;
      expect_install_reason_ = InstallReason::kUser;
      expect_install_reason_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_reason = InstallReason::kPolicy;
      expect_install_reason_ = InstallReason::kPolicy;
      expect_install_reason_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_install_reason_, state->install_reason);
      ExpectNoChange();
      CheckExpects(u);
    }

    // InstallSource tests.

    if (state) {
      state->install_source = InstallSource::kPlayStore;
      expect_install_source_ = InstallSource::kPlayStore;
      expect_install_source_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->install_source = InstallSource::kSync;
      expect_install_source_ = InstallSource::kSync;
      expect_install_source_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_install_source_, state->install_source);
      ExpectNoChange();
      CheckExpects(u);
    }

    // PolicyId tests.

    if (state) {
      state->policy_ids = {"https://app.site/alpha", "https://site.app/alpha"};
      expect_policy_ids_ = {"https://app.site/alpha", "https://site.app/alpha"};
      expect_policy_ids_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->policy_ids = {"https://app.site/delta", "https://site.app/delta"};
      expect_policy_ids_ = {"https://app.site/delta", "https://site.app/delta"};
      expect_policy_ids_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_THAT(state->policy_ids,
                  testing::UnorderedElementsAreArray(expect_policy_ids_));
      ExpectNoChange();
      CheckExpects(u);
    }

    // IsPlatformApp tests.

    if (state) {
      state->is_platform_app = false;
      expect_is_platform_app_ = false;
      expect_is_platform_app_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->is_platform_app = true;
      expect_is_platform_app_ = true;
      expect_is_platform_app_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_is_platform_app_, state->is_platform_app);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Recommendable tests.

    if (state) {
      state->recommendable = false;
      expect_recommendable_ = false;
      expect_recommendable_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->recommendable = true;
      expect_recommendable_ = true;
      expect_recommendable_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_recommendable_, state->recommendable);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Searchable tests.

    if (state) {
      state->searchable = false;
      expect_searchable_ = false;
      expect_searchable_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->searchable = true;
      expect_searchable_ = true;
      expect_searchable_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_searchable_, state->searchable);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInLauncher tests.

    if (state) {
      state->show_in_launcher = false;
      expect_show_in_launcher_ = false;
      expect_show_in_launcher_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_launcher = true;
      expect_show_in_launcher_ = true;
      expect_show_in_launcher_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_launcher_, state->show_in_launcher);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInShelf tests.

    if (state) {
      state->show_in_shelf = false;
      expect_show_in_shelf_ = false;
      expect_show_in_shelf_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_shelf = true;
      expect_show_in_shelf_ = true;
      expect_show_in_shelf_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_shelf_, state->show_in_shelf);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInSearch tests.

    if (state) {
      state->show_in_search = false;
      expect_show_in_search_ = false;
      expect_show_in_search_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_search = true;
      expect_show_in_search_ = true;
      expect_show_in_search_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_search_, state->show_in_search);
      ExpectNoChange();
      CheckExpects(u);
    }

    // ShowInManagement tests.

    if (state) {
      state->show_in_management = false;
      expect_show_in_management_ = false;
      expect_show_in_management_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->show_in_management = true;
      expect_show_in_management_ = true;
      expect_show_in_management_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_show_in_management_, state->show_in_management);
      ExpectNoChange();
      CheckExpects(u);
    }

    // HandlesIntents tests.

    if (state) {
      state->handles_intents = false;
      expect_handles_intents_ = false;
      expect_handles_intents_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->handles_intents = true;
      expect_handles_intents_ = true;
      expect_handles_intents_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_handles_intents_, state->handles_intents);
      ExpectNoChange();
      CheckExpects(u);
    }

    // AllowUninstall tests

    if (state) {
      state->allow_uninstall = false;
      expect_allow_uninstall_ = false;
      expect_allow_uninstall_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->allow_uninstall = true;
      expect_allow_uninstall_ = true;
      expect_allow_uninstall_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_allow_uninstall_, state->allow_uninstall);
      ExpectNoChange();
      CheckExpects(u);
    }

    // HasBadge tests.

    if (state) {
      state->has_badge = false;
      expect_has_badge_ = false;
      expect_has_badge_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->has_badge = true;
      expect_has_badge_ = true;
      expect_has_badge_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_has_badge_, state->has_badge);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Pause tests.

    if (state) {
      state->paused = false;
      expect_paused_ = false;
      expect_paused_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->paused = true;
      expect_paused_ = true;
      expect_paused_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_paused_, state->paused);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Intent Filter tests.

    if (state) {
      IntentFilterPtr intent_filter = std::make_unique<IntentFilter>();

      ConditionValues scheme_condition_values;
      scheme_condition_values.push_back(std::make_unique<ConditionValue>(
          "https", PatternMatchType::kLiteral));
      ConditionPtr scheme_condition = std::make_unique<Condition>(
          ConditionType::kScheme, std::move(scheme_condition_values));

      ConditionValues host_condition_values;
      host_condition_values.push_back(std::make_unique<ConditionValue>(
          "www.google.com", PatternMatchType::kLiteral));
      auto host_condition = std::make_unique<Condition>(
          ConditionType::kAuthority, std::move(host_condition_values));

      intent_filter->conditions.push_back(std::move(scheme_condition));
      intent_filter->conditions.push_back(std::move(host_condition));

      state->intent_filters.push_back(intent_filter->Clone());
      expect_intent_filters_.push_back(intent_filter->Clone());
      expect_intent_filters_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_intent_filters_.clear();

      IntentFilterPtr intent_filter = std::make_unique<IntentFilter>();

      ConditionValues scheme_condition_values;
      scheme_condition_values.push_back(std::make_unique<ConditionValue>(
          "https", PatternMatchType::kLiteral));
      ConditionPtr scheme_condition = std::make_unique<Condition>(
          ConditionType::kScheme, std::move(scheme_condition_values));
      intent_filter->conditions.push_back(scheme_condition->Clone());

      ConditionValues host_condition_values;
      host_condition_values.push_back(std::make_unique<ConditionValue>(
          "www.abc.com", PatternMatchType::kLiteral));
      auto host_condition = std::make_unique<Condition>(
          ConditionType::kAuthority, std::move(host_condition_values));
      intent_filter->conditions.push_back(host_condition->Clone());

      intent_filter->conditions.push_back(std::move(scheme_condition));
      intent_filter->conditions.push_back(std::move(host_condition));

      delta->intent_filters.push_back(intent_filter->Clone());
      expect_intent_filters_.push_back(intent_filter->Clone());
      expect_intent_filters_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_TRUE(IsEqual(expect_intent_filters_, state->intent_filters));
      ExpectNoChange();
      CheckExpects(u);
    }

    // ResizeLocked tests.

    if (state) {
      state->resize_locked = false;
      expect_resize_locked_ = false;
      expect_resize_locked_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->resize_locked = true;
      expect_resize_locked_ = true;
      expect_resize_locked_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_resize_locked_, state->resize_locked);
      ExpectNoChange();
      CheckExpects(u);
    }

    // AllowWindowModeSelection tests.

    if (state) {
      state->allow_window_mode_selection = false;
      expect_allow_window_mode_selection_ = false;
      expect_allow_window_mode_selection_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->allow_window_mode_selection = true;
      expect_allow_window_mode_selection_ = true;
      expect_allow_window_mode_selection_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_allow_window_mode_selection_,
                state->allow_window_mode_selection);
      ExpectNoChange();
      CheckExpects(u);
    }

    // WindowMode tests.

    if (state) {
      state->window_mode = WindowMode::kBrowser;
      expect_window_mode_ = WindowMode::kBrowser;
      expect_window_mode_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->window_mode = WindowMode::kWindow;
      expect_window_mode_ = WindowMode::kWindow;
      expect_window_mode_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_window_mode_, state->window_mode);
      ExpectNoChange();
      CheckExpects(u);
    }

    // RunOnOsLogin tests.

    if (state) {
      state->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
      expect_run_on_os_login_ = RunOnOsLogin(RunOnOsLoginMode::kNotRun, false);
      expect_run_on_os_login_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->run_on_os_login = RunOnOsLogin(RunOnOsLoginMode::kWindowed, false);
      expect_run_on_os_login_ =
          RunOnOsLogin(RunOnOsLoginMode::kWindowed, false);
      expect_run_on_os_login_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_run_on_os_login_.value(),
                state->run_on_os_login.value());
      ExpectNoChange();
      CheckExpects(u);
    }

    // AllowClose tests.

    if (state) {
      state->allow_close = false;
      expect_allow_close_ = false;
      expect_allow_close_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->allow_close = true;
      expect_allow_close_ = true;
      expect_allow_close_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_allow_close_, state->allow_close);
      ExpectNoChange();
      CheckExpects(u);
    }

    // App size in bytes tests.

    if (state) {
      state->app_size_in_bytes = 17;
      expect_app_size_in_bytes_ = 17;
      expect_app_size_in_bytes_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->app_size_in_bytes = 42;
      expect_app_size_in_bytes_ = 42;
      expect_app_size_in_bytes_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_app_size_in_bytes_, state->app_size_in_bytes);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Data size in bytes tests.

    if (state) {
      state->data_size_in_bytes = 17;
      expect_data_size_in_bytes_ = 17;
      expect_data_size_in_bytes_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->data_size_in_bytes = 42;
      expect_data_size_in_bytes_ = 42;
      expect_data_size_in_bytes_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_data_size_in_bytes_, state->data_size_in_bytes);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Supported locales tests.

    if (state) {
      state->supported_locales.push_back("en-US");
      state->supported_locales.push_back("ja-JP");
      expect_supported_locales_.push_back("en-US");
      expect_supported_locales_.push_back("ja-JP");
      expect_supported_locales_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      expect_supported_locales_.clear();
      delta->supported_locales.push_back("en-GB");
      delta->supported_locales.push_back("fr-FR");
      expect_supported_locales_.push_back("en-GB");
      expect_supported_locales_.push_back("fr-FR");
      expect_supported_locales_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_supported_locales_, state->supported_locales);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Selected locale tests.

    if (state) {
      state->selected_locale = "en-US";
      expect_selected_locale_ = "en-US";
      expect_selected_locale_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->selected_locale = "ja-JP";
      expect_selected_locale_ = "ja-JP";
      expect_selected_locale_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_selected_locale_, state->selected_locale);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Extra tests.

    if (state) {
      base::Value::Dict dict;
      dict.Set("vm_name", "vm_name_value");
      state->SetExtraField("vm_name", "vm_name_value");
      expect_extra_ = std::move(dict);
      expect_extra_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      base::Value::Dict dict;
      dict.Set("container_name", "container_name_value");
      delta->SetExtraField("container_name", "container_name_value");
      expect_extra_ = std::move(dict);
      expect_extra_changed_ = true;
      expect_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::AppUpdate::Merge(state, delta);
      EXPECT_EQ(expect_extra_, state->extra);
      ExpectNoChange();
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

TEST_F(AppUpdateTest, VerifyIconKeyWithResourceId) {
  // Verify for the null delta.
  App state(app_type, app_id);
  AppUpdate update1(&state, nullptr, account_id_);
  EXPECT_FALSE(update1.IconKeyChanged());
  EXPECT_FALSE(update1.IconKey().has_value());
  EXPECT_FALSE(state.icon_key.has_value());

  EXPECT_FALSE(AppUpdate::IsChanged(&state, nullptr));
  AppUpdate::Merge(&state, nullptr);
  EXPECT_FALSE(state.icon_key.has_value());

  // Update for the icon having a `resource_id`.
  App delta(app_type, app_id);
  IconKey icon_key(/*resource_id=*/65535, IconEffects::kCrOsStandardIcon);
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate update2(&state, &delta, account_id_);
  EXPECT_TRUE(update2.IconKeyChanged());
  icon_key.update_version = IconKey::kInvalidVersion;
  EXPECT_EQ(icon_key, update2.IconKey().value());
  EXPECT_FALSE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());

  // Update for the icon having a `resource_id` again, and verify, no change.
  AppUpdate update3(&state, &delta, account_id_);
  EXPECT_FALSE(update3.IconKeyChanged());
  EXPECT_EQ(icon_key, update3.IconKey().value());
  EXPECT_TRUE(state.icon_key.has_value());

  EXPECT_FALSE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());

  // Update the icon effect.
  delta.icon_key->icon_effects = IconEffects::kNone;
  icon_key.icon_effects = IconEffects::kNone;
  AppUpdate update4(&state, &delta, account_id_);
  EXPECT_TRUE(update4.IconKeyChanged());
  EXPECT_EQ(icon_key, update4.IconKey().value());
  EXPECT_TRUE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());
}

TEST_F(AppUpdateTest, VerifyIconKeyWithoutResourceId) {
  // Update for the icon without a `resource_id`.
  App state(app_type, app_id);
  App delta(app_type, app_id);
  IconKey icon_key(/*resource_id=*/IconKey::kInvalidResourceId,
                   IconEffects::kCrOsStandardIcon);
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate update1(&state, &delta, account_id_);
  EXPECT_TRUE(update1.IconKeyChanged());
  icon_key.update_version = IconKey::kInitVersion;
  EXPECT_EQ(icon_key, update1.IconKey().value());
  EXPECT_FALSE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());

  // Update the icon again, and verify, no change.
  AppUpdate update2(&state, &delta, account_id_);
  EXPECT_FALSE(update2.IconKeyChanged());
  EXPECT_EQ(icon_key, update2.IconKey().value());
  EXPECT_TRUE(state.icon_key.has_value());

  EXPECT_FALSE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());

  // Update the icon with `update_version` = true, and verify `update_version`
  // is increased.
  delta.icon_key->update_version = true;
  AppUpdate update3(&state, &delta, account_id_);
  EXPECT_TRUE(update3.IconKeyChanged());
  icon_key.update_version = IconKey::kInitVersion + 1;
  EXPECT_EQ(icon_key, update3.IconKey().value());
  EXPECT_TRUE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());
}

TEST_F(AppUpdateTest, VerifyIconKeyWithEffectChange) {
  // Update for the icon without a `resource_id`.
  App state(app_type, app_id);
  App delta(app_type, app_id);
  IconKey icon_key(/*resource_id=*/IconKey::kInvalidResourceId,
                   IconEffects::kCrOsStandardIcon);
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate update1(&state, &delta, account_id_);
  EXPECT_TRUE(update1.IconKeyChanged());
  icon_key.update_version = IconKey::kInitVersion;
  EXPECT_EQ(icon_key, update1.IconKey().value());
  EXPECT_FALSE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());

  // Update the icon with the icon effect change.
  delta.icon_key->icon_effects =
      IconEffects::kCrOsStandardIcon | IconEffects::kPaused;
  AppUpdate update2(&state, &delta, account_id_);
  EXPECT_TRUE(update2.IconKeyChanged());
  icon_key.icon_effects = IconEffects::kCrOsStandardIcon | IconEffects::kPaused;
  EXPECT_EQ(icon_key, update2.IconKey().value());
  EXPECT_TRUE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());

  // Update the icon with `update_version` = true, and the icon effect change.
  // Verify `update_version` is increased.
  delta.icon_key->update_version = true;
  delta.icon_key->icon_effects = IconEffects::kCrOsStandardIcon;
  AppUpdate update3(&state, &delta, account_id_);
  EXPECT_TRUE(update3.IconKeyChanged());
  icon_key.update_version = IconKey::kInitVersion + 1;
  icon_key.icon_effects = IconEffects::kCrOsStandardIcon;
  EXPECT_EQ(icon_key, update3.IconKey().value());
  EXPECT_TRUE(state.icon_key.has_value());

  EXPECT_TRUE(AppUpdate::IsChanged(&state, &delta));
  AppUpdate::Merge(&state, &delta);
  EXPECT_EQ(icon_key, state.icon_key.value());
}

TEST_F(AppUpdateTest, VerifyMergeIconKeyDeltaWithResourceId) {
  App new_delta(app_type, app_id);
  AppUpdate::MergeDelta(&new_delta, nullptr);
  EXPECT_FALSE(new_delta.icon_key.has_value());

  // Update for the icon having a `resource_id`.
  App delta(app_type, app_id);
  IconKey icon_key(/*resource_id=*/65535, IconEffects::kCrOsStandardIcon);
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());

  // Update for the icon having a `resource_id` again, and verify, no change.
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());

  // Update the icon effect.
  icon_key.icon_effects = IconEffects::kNone;
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());
}

TEST_F(AppUpdateTest, VerifyMergeIconKeyDeltaWithoutResourceId) {
  // Update for the icon without a `resource_id`.
  App new_delta(app_type, app_id);
  App delta(app_type, app_id);
  IconKey icon_key(/*resource_id=*/IconKey::kInvalidResourceId,
                   IconEffects::kCrOsStandardIcon);
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());

  // Update the icon again, and verify, no change.
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());

  // Update the icon with `update_version` = true.
  icon_key.update_version = true;
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());
}

TEST_F(AppUpdateTest, VerifyMergeIconKeyDeltaWithEffectChange) {
  // Update for the icon without a `resource_id`.
  App new_delta(app_type, app_id);
  App delta(app_type, app_id);
  IconKey icon_key(/*resource_id=*/IconKey::kInvalidResourceId,
                   IconEffects::kCrOsStandardIcon);
  icon_key.update_version = true;
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());

  // Update the icon with the icon effect change.
  icon_key.icon_effects = IconEffects::kCrOsStandardIcon | IconEffects::kPaused;
  icon_key.update_version = false;
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  icon_key.update_version = true;
  EXPECT_EQ(icon_key, new_delta.icon_key.value());

  // Update the icon with `update_version` = true, and the icon effect change.
  icon_key.update_version = true;
  icon_key.icon_effects = IconEffects::kCrOsStandardIcon;
  delta.icon_key = std::move(*icon_key.Clone());
  AppUpdate::MergeDelta(&new_delta, &delta);
  EXPECT_EQ(icon_key, new_delta.icon_key.value());
}

}  // namespace apps
