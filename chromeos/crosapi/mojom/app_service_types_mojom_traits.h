// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_APP_SERVICE_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_APP_SERVICE_TYPES_MOJOM_TRAITS_H_

#include <string>

#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

template <>
struct StructTraits<crosapi::mojom::AppDataView, apps::AppPtr> {
  static apps::AppType app_type(const apps::AppPtr& r) { return r->app_type; }

  static const std::string& app_id(const apps::AppPtr& r) { return r->app_id; }

  static apps::Readiness readiness(const apps::AppPtr& r) {
    return r->readiness;
  }

  static const absl::optional<std::string>& name(const apps::AppPtr& r) {
    return r->name;
  }

  static const absl::optional<std::string>& short_name(const apps::AppPtr& r) {
    return r->short_name;
  }

  static const absl::optional<std::string>& publisher_id(
      const apps::AppPtr& r) {
    return r->publisher_id;
  }

  static const absl::optional<std::string>& description(const apps::AppPtr& r) {
    return r->description;
  }

  static const absl::optional<std::string>& version(const apps::AppPtr& r) {
    return r->version;
  }

  static const std::vector<std::string>& additional_search_terms(
      const apps::AppPtr& r) {
    return r->additional_search_terms;
  }

  static apps::IconKeyPtr icon_key(const apps::AppPtr& r);

  static const absl::optional<base::Time>& last_launch_time(
      const apps::AppPtr& r) {
    return r->last_launch_time;
  }

  static const absl::optional<base::Time>& install_time(const apps::AppPtr& r) {
    return r->install_time;
  }

  static const apps::InstallReason& install_reason(const apps::AppPtr& r) {
    return r->install_reason;
  }

  // This method is required for Ash-Lacros backwards compatibility.
  static absl::optional<std::string> deprecated_policy_id(
      const apps::AppPtr& r);

  static const std::vector<std::string>& policy_ids(const apps::AppPtr& r) {
    return r->policy_ids;
  }

  static crosapi::mojom::OptionalBool recommendable(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool searchable(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool show_in_launcher(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool show_in_shelf(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool show_in_search(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool show_in_management(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool has_badge(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool paused(const apps::AppPtr& r);

  static const apps::IntentFilters& intent_filters(const apps::AppPtr& r) {
    return r->intent_filters;
  }

  static const apps::WindowMode& window_mode(const apps::AppPtr& r) {
    return r->window_mode;
  }

  static const apps::Permissions& permissions(const apps::AppPtr& r) {
    return r->permissions;
  }

  static crosapi::mojom::OptionalBool allow_uninstall(const apps::AppPtr& r);

  static crosapi::mojom::OptionalBool handles_intents(const apps::AppPtr& r);

  static const apps::Shortcuts& shortcuts(const apps::AppPtr& r) {
    return r->shortcuts;
  }

  static crosapi::mojom::OptionalBool is_platform_app(const apps::AppPtr& r);

  static bool Read(crosapi::mojom::AppDataView data, apps::AppPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::AppType, apps::AppType> {
  static crosapi::mojom::AppType ToMojom(apps::AppType input);
  static bool FromMojom(crosapi::mojom::AppType input, apps::AppType* output);
};

template <>
struct EnumTraits<crosapi::mojom::Readiness, apps::Readiness> {
  static crosapi::mojom::Readiness ToMojom(apps::Readiness input);
  static bool FromMojom(crosapi::mojom::Readiness input,
                        apps::Readiness* output);
};

template <>
struct StructTraits<crosapi::mojom::IconKeyDataView, apps::IconKeyPtr> {
  static bool IsNull(const apps::IconKeyPtr& r) { return !r; }

  static void SetToNull(apps::IconKeyPtr* r) { r->reset(); }

  static uint64_t timeline(const apps::IconKeyPtr& r) { return r->timeline; }

  static uint32_t icon_effects(const apps::IconKeyPtr& r) {
    return r->icon_effects;
  }

  static bool raw_icon_updated(const apps::IconKeyPtr& r) {
    return r->raw_icon_updated;
  }

  static bool Read(crosapi::mojom::IconKeyDataView, apps::IconKeyPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::InstallReason, apps::InstallReason> {
  static crosapi::mojom::InstallReason ToMojom(apps::InstallReason input);
  static bool FromMojom(crosapi::mojom::InstallReason input,
                        apps::InstallReason* output);
};

template <>
struct StructTraits<crosapi::mojom::IntentFilterDataView,
                    apps::IntentFilterPtr> {
  static const std::vector<apps::ConditionPtr>& conditions(
      const apps::IntentFilterPtr& r) {
    return r->conditions;
  }

  static const absl::optional<std::string>& activity_name(
      const apps::IntentFilterPtr& r) {
    return r->activity_name;
  }

  static const absl::optional<std::string>& activity_label(
      const apps::IntentFilterPtr& r) {
    return r->activity_label;
  }

  static bool Read(crosapi::mojom::IntentFilterDataView,
                   apps::IntentFilterPtr* out);
};

template <>
struct StructTraits<crosapi::mojom::ConditionDataView, apps::ConditionPtr> {
  static const apps::ConditionType& condition_type(
      const apps::ConditionPtr& r) {
    return r->condition_type;
  }

  static const apps::ConditionValues& condition_values(
      const apps::ConditionPtr& r) {
    return r->condition_values;
  }

  static bool Read(crosapi::mojom::ConditionDataView, apps::ConditionPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::ConditionType, apps::ConditionType> {
  static crosapi::mojom::ConditionType ToMojom(apps::ConditionType input);
  static bool FromMojom(crosapi::mojom::ConditionType input,
                        apps::ConditionType* output);
};

template <>
struct StructTraits<crosapi::mojom::ConditionValueDataView,
                    apps::ConditionValuePtr> {
  static const std::string& value(const apps::ConditionValuePtr& r) {
    return r->value;
  }

  static const apps::PatternMatchType& match_type(
      const apps::ConditionValuePtr& r) {
    return r->match_type;
  }

  static bool Read(crosapi::mojom::ConditionValueDataView,
                   apps::ConditionValuePtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::PatternMatchType, apps::PatternMatchType> {
  static crosapi::mojom::PatternMatchType ToMojom(apps::PatternMatchType input);
  static bool FromMojom(crosapi::mojom::PatternMatchType input,
                        apps::PatternMatchType* output);
};

template <>
struct EnumTraits<crosapi::mojom::UninstallSource, apps::UninstallSource> {
  static crosapi::mojom::UninstallSource ToMojom(apps::UninstallSource input);
  static bool FromMojom(crosapi::mojom::UninstallSource input,
                        apps::UninstallSource* output);
};

template <>
struct StructTraits<crosapi::mojom::CapabilityAccessDataView,
                    apps::CapabilityAccessPtr> {
  static const std::string& app_id(const apps::CapabilityAccessPtr& r) {
    return r->app_id;
  }

  static crosapi::mojom::OptionalBool camera(
      const apps::CapabilityAccessPtr& r);

  static crosapi::mojom::OptionalBool microphone(
      const apps::CapabilityAccessPtr& r);

  static bool Read(crosapi::mojom::CapabilityAccessDataView,
                   apps::CapabilityAccessPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::IconType, apps::IconType> {
  static crosapi::mojom::IconType ToMojom(apps::IconType input);
  static bool FromMojom(crosapi::mojom::IconType input, apps::IconType* output);
};

template <>
struct StructTraits<crosapi::mojom::IconValueDataView, apps::IconValuePtr> {
  static apps::IconType icon_type(const apps::IconValuePtr& r) {
    return r->icon_type;
  }

  static const gfx::ImageSkia& uncompressed(const apps::IconValuePtr& r) {
    return r->uncompressed;
  }

  static const std::vector<uint8_t>& compressed(const apps::IconValuePtr& r) {
    return r->compressed;
  }

  static bool is_placeholder_icon(const apps::IconValuePtr& r) {
    return r->is_placeholder_icon;
  }

  static bool is_maskable_icon(const apps::IconValuePtr& r) {
    return r->is_maskable_icon;
  }

  static bool Read(crosapi::mojom::IconValueDataView, apps::IconValuePtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::WindowMode, apps::WindowMode> {
  static crosapi::mojom::WindowMode ToMojom(apps::WindowMode input);
  static bool FromMojom(crosapi::mojom::WindowMode input,
                        apps::WindowMode* output);
};

template <>
struct EnumTraits<crosapi::mojom::LaunchSource, apps::LaunchSource> {
  static crosapi::mojom::LaunchSource ToMojom(apps::LaunchSource input);
  static bool FromMojom(crosapi::mojom::LaunchSource input,
                        apps::LaunchSource* output);
};

template <>
struct StructTraits<crosapi::mojom::PermissionDataView, apps::PermissionPtr> {
  static apps::PermissionType permission_type(const apps::PermissionPtr& r) {
    return r->permission_type;
  }

  static const apps::PermissionValuePtr& value(const apps::PermissionPtr& r) {
    return r->value;
  }

  static bool is_managed(const apps::PermissionPtr& r) { return r->is_managed; }

  static bool Read(crosapi::mojom::PermissionDataView,
                   apps::PermissionPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::PermissionType, apps::PermissionType> {
  static crosapi::mojom::PermissionType ToMojom(apps::PermissionType input);
  static bool FromMojom(crosapi::mojom::PermissionType input,
                        apps::PermissionType* output);
};

template <>
struct EnumTraits<crosapi::mojom::TriState, apps::TriState> {
  static crosapi::mojom::TriState ToMojom(apps::TriState input);
  static bool FromMojom(crosapi::mojom::TriState input, apps::TriState* output);
};

template <>
struct UnionTraits<crosapi::mojom::PermissionValueDataView,
                   apps::PermissionValuePtr> {
  static crosapi::mojom::PermissionValueDataView::Tag GetTag(
      const apps::PermissionValuePtr& r);

  static bool IsNull(const apps::PermissionValuePtr& r) {
    return !absl::holds_alternative<bool>(r->value) &&
           !absl::holds_alternative<apps::TriState>(r->value);
  }

  static void SetToNull(apps::PermissionValuePtr* out) {}

  static bool bool_value(const apps::PermissionValuePtr& r) {
    if (absl::holds_alternative<bool>(r->value)) {
      return absl::get<bool>(r->value);
    }
    return false;
  }

  static apps::TriState tristate_value(const apps::PermissionValuePtr& r) {
    if (absl::holds_alternative<apps::TriState>(r->value)) {
      return absl::get<apps::TriState>(r->value);
    }
    return apps::TriState::kBlock;
  }

  static bool Read(crosapi::mojom::PermissionValueDataView data,
                   apps::PermissionValuePtr* out);
};

template <>
struct StructTraits<crosapi::mojom::PreferredAppDataView,
                    apps::PreferredAppPtr> {
  static apps::IntentFilterPtr intent_filter(const apps::PreferredAppPtr& r) {
    return r->intent_filter->Clone();
  }

  static const std::string& app_id(const apps::PreferredAppPtr& r) {
    return r->app_id;
  }

  static bool Read(crosapi::mojom::PreferredAppDataView,
                   apps::PreferredAppPtr* out);
};

template <>
struct StructTraits<crosapi::mojom::PreferredAppChangesDataView,
                    apps::PreferredAppChangesPtr> {
  static base::flat_map<std::string, apps::IntentFilters> added_filters(
      const apps::PreferredAppChangesPtr& r) {
    return apps::CloneIntentFiltersMap(r->added_filters);
  }

  static base::flat_map<std::string, apps::IntentFilters> removed_filters(
      const apps::PreferredAppChangesPtr& r) {
    return apps::CloneIntentFiltersMap(r->removed_filters);
  }

  static bool Read(crosapi::mojom::PreferredAppChangesDataView,
                   apps::PreferredAppChangesPtr* out);
};

template <>
struct StructTraits<crosapi::mojom::ShortcutDataView, apps::ShortcutPtr> {
  static const std::string& shortcut_id(const apps::ShortcutPtr& r) {
    return r->shortcut_id;
  }

  static const std::string& name(const apps::ShortcutPtr& r) { return r->name; }

  static uint8_t position(const apps::ShortcutPtr& r) { return r->position; }

  static bool Read(crosapi::mojom::ShortcutDataView data,
                   apps::ShortcutPtr* out);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_APP_SERVICE_TYPES_MOJOM_TRAITS_H_
