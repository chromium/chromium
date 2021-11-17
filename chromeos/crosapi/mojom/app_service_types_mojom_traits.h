// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_APP_SERVICE_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_APP_SERVICE_TYPES_MOJOM_TRAITS_H_

#include <string>

#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct StructTraits<crosapi::mojom::AppDataView, apps::mojom::AppPtr> {
  static apps::mojom::AppType app_type(const apps::mojom::AppPtr& r) {
    return r->app_type;
  }

  static const std::string& app_id(const apps::mojom::AppPtr& r) {
    return r->app_id;
  }

  static apps::mojom::Readiness readiness(const apps::mojom::AppPtr& r) {
    return r->readiness;
  }

  static const absl::optional<std::string>& name(const apps::mojom::AppPtr& r) {
    return r->name;
  }

  static const absl::optional<std::string>& short_name(
      const apps::mojom::AppPtr& r) {
    return r->short_name;
  }

  static const absl::optional<std::string>& publisher_id(
      const apps::mojom::AppPtr& r) {
    return r->publisher_id;
  }

  static const absl::optional<std::string>& description(
      const apps::mojom::AppPtr& r) {
    return r->description;
  }

  static const absl::optional<std::string>& version(
      const apps::mojom::AppPtr& r) {
    return r->version;
  }

  static const std::vector<std::string>& additional_search_terms(
      const apps::mojom::AppPtr& r) {
    return r->additional_search_terms;
  }

  static const apps::mojom::IconKeyPtr& icon_key(const apps::mojom::AppPtr& r) {
    return r->icon_key;
  }

  static const absl::optional<base::Time>& last_launch_time(
      const apps::mojom::AppPtr& r) {
    return r->last_launch_time;
  }

  static const absl::optional<base::Time>& install_time(
      const apps::mojom::AppPtr& r) {
    return r->install_time;
  }

  static const apps::mojom::InstallReason& install_reason(
      const apps::mojom::AppPtr& r) {
    return r->install_reason;
  }

  static const absl::optional<std::string>& policy_id(
      const apps::mojom::AppPtr& r) {
    return r->policy_id;
  }

  static const apps::mojom::OptionalBool& recommendable(
      const apps::mojom::AppPtr& r) {
    return r->recommendable;
  }

  static const apps::mojom::OptionalBool& searchable(
      const apps::mojom::AppPtr& r) {
    return r->searchable;
  }

  static const apps::mojom::OptionalBool& show_in_launcher(
      const apps::mojom::AppPtr& r) {
    return r->show_in_launcher;
  }

  static const apps::mojom::OptionalBool& show_in_shelf(
      const apps::mojom::AppPtr& r) {
    return r->show_in_shelf;
  }

  static const apps::mojom::OptionalBool& show_in_search(
      const apps::mojom::AppPtr& r) {
    return r->show_in_search;
  }

  static const apps::mojom::OptionalBool& show_in_management(
      const apps::mojom::AppPtr& r) {
    return r->show_in_management;
  }

  static const apps::mojom::OptionalBool& has_badge(
      const apps::mojom::AppPtr& r) {
    return r->has_badge;
  }

  static const apps::mojom::OptionalBool& paused(const apps::mojom::AppPtr& r) {
    return r->paused;
  }

  static const std::vector<apps::mojom::IntentFilterPtr>& intent_filters(
      const apps::mojom::AppPtr& r) {
    return r->intent_filters;
  }

  static const apps::mojom::WindowMode& window_mode(
      const apps::mojom::AppPtr& r) {
    return r->window_mode;
  }

  // TODO(crbug.com/1253250):
  // Create empty permission vector because currently apps::mojom::App
  // doesn't accept null. The permissions field should be null for
  // no permission change after app service mojom removed.
  static const std::vector<apps::mojom::PermissionPtr>& permissions(
      const apps::mojom::AppPtr& r) {
    return r->permissions;
  }

  static const apps::mojom::OptionalBool& allow_uninstall(
      const apps::mojom::AppPtr& r) {
    return r->allow_uninstall;
  }

  static const apps::mojom::OptionalBool& handles_intents(
      const apps::mojom::AppPtr& r) {
    return r->handles_intents;
  }

  static bool Read(crosapi::mojom::AppDataView data, apps::mojom::AppPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::AppType, apps::mojom::AppType> {
  static crosapi::mojom::AppType ToMojom(apps::mojom::AppType input);
  static bool FromMojom(crosapi::mojom::AppType input,
                        apps::mojom::AppType* output);
};

template <>
struct EnumTraits<crosapi::mojom::Readiness, apps::mojom::Readiness> {
  static crosapi::mojom::Readiness ToMojom(apps::mojom::Readiness input);
  static bool FromMojom(crosapi::mojom::Readiness input,
                        apps::mojom::Readiness* output);
};

template <>
struct StructTraits<crosapi::mojom::IconKeyDataView, apps::mojom::IconKeyPtr> {
  static bool IsNull(const apps::mojom::IconKeyPtr& r) { return r.is_null(); }

  static void SetToNull(apps::mojom::IconKeyPtr* r) { r->reset(); }

  static uint64_t timeline(const apps::mojom::IconKeyPtr& r) {
    return r->timeline;
  }

  static uint32_t icon_effects(const apps::mojom::IconKeyPtr& r) {
    return r->icon_effects;
  }

  static bool Read(crosapi::mojom::IconKeyDataView,
                   apps::mojom::IconKeyPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::InstallReason, apps::mojom::InstallReason> {
  static crosapi::mojom::InstallReason ToMojom(
      apps::mojom::InstallReason input);
  static bool FromMojom(crosapi::mojom::InstallReason input,
                        apps::mojom::InstallReason* output);
};

template <>
struct EnumTraits<crosapi::mojom::OptionalBool, apps::mojom::OptionalBool> {
  static crosapi::mojom::OptionalBool ToMojom(apps::mojom::OptionalBool input);
  static bool FromMojom(crosapi::mojom::OptionalBool input,
                        apps::mojom::OptionalBool* output);
};

template <>
struct StructTraits<crosapi::mojom::IntentFilterDataView,
                    apps::mojom::IntentFilterPtr> {
  static const std::vector<apps::mojom::ConditionPtr>& conditions(
      const apps::mojom::IntentFilterPtr& r) {
    return r->conditions;
  }

  static const absl::optional<std::string>& activity_name(
      const apps::mojom::IntentFilterPtr& r) {
    return r->activity_name;
  }

  static const absl::optional<std::string>& activity_label(
      const apps::mojom::IntentFilterPtr& r) {
    return r->activity_label;
  }

  static bool Read(crosapi::mojom::IntentFilterDataView,
                   apps::mojom::IntentFilterPtr* out);
};

template <>
struct StructTraits<crosapi::mojom::ConditionDataView,
                    apps::mojom::ConditionPtr> {
  static const apps::mojom::ConditionType& condition_type(
      const apps::mojom::ConditionPtr& r) {
    return r->condition_type;
  }

  static const std::vector<apps::mojom::ConditionValuePtr>& condition_values(
      const apps::mojom::ConditionPtr& r) {
    return r->condition_values;
  }

  static bool Read(crosapi::mojom::ConditionDataView,
                   apps::mojom::ConditionPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::ConditionType, apps::mojom::ConditionType> {
  static crosapi::mojom::ConditionType ToMojom(
      apps::mojom::ConditionType input);
  static bool FromMojom(crosapi::mojom::ConditionType input,
                        apps::mojom::ConditionType* output);
};

template <>
struct StructTraits<crosapi::mojom::ConditionValueDataView,
                    apps::mojom::ConditionValuePtr> {
  static const std::string& value(const apps::mojom::ConditionValuePtr& r) {
    return r->value;
  }

  static const apps::mojom::PatternMatchType& match_type(
      const apps::mojom::ConditionValuePtr& r) {
    return r->match_type;
  }

  static bool Read(crosapi::mojom::ConditionValueDataView,
                   apps::mojom::ConditionValuePtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::PatternMatchType,
                  apps::mojom::PatternMatchType> {
  static crosapi::mojom::PatternMatchType ToMojom(
      apps::mojom::PatternMatchType input);
  static bool FromMojom(crosapi::mojom::PatternMatchType input,
                        apps::mojom::PatternMatchType* output);
};

template <>
struct EnumTraits<crosapi::mojom::UninstallSource,
                  apps::mojom::UninstallSource> {
  static crosapi::mojom::UninstallSource ToMojom(
      apps::mojom::UninstallSource input);
  static bool FromMojom(crosapi::mojom::UninstallSource input,
                        apps::mojom::UninstallSource* output);
};

template <>
struct StructTraits<crosapi::mojom::CapabilityAccessDataView,
                    apps::mojom::CapabilityAccessPtr> {
  static const std::string& app_id(const apps::mojom::CapabilityAccessPtr& r) {
    return r->app_id;
  }

  static const apps::mojom::OptionalBool& camera(
      const apps::mojom::CapabilityAccessPtr& r) {
    return r->camera;
  }

  static const apps::mojom::OptionalBool& microphone(
      const apps::mojom::CapabilityAccessPtr& r) {
    return r->microphone;
  }

  static bool Read(crosapi::mojom::CapabilityAccessDataView,
                   apps::mojom::CapabilityAccessPtr* out);
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

  static bool Read(crosapi::mojom::IconValueDataView, apps::IconValuePtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::WindowMode, apps::mojom::WindowMode> {
  static crosapi::mojom::WindowMode ToMojom(apps::mojom::WindowMode input);
  static bool FromMojom(crosapi::mojom::WindowMode input,
                        apps::mojom::WindowMode* output);
};

template <>
struct EnumTraits<crosapi::mojom::LaunchSource, apps::mojom::LaunchSource> {
  static crosapi::mojom::LaunchSource ToMojom(apps::mojom::LaunchSource input);
  static bool FromMojom(crosapi::mojom::LaunchSource input,
                        apps::mojom::LaunchSource* output);
};

template <>
struct StructTraits<crosapi::mojom::PermissionDataView,
                    apps::mojom::PermissionPtr> {
  static apps::mojom::PermissionType permission_type(
      const apps::mojom::PermissionPtr& r) {
    return r->permission_type;
  }

  static const apps::mojom::PermissionValuePtr& value(
      const apps::mojom::PermissionPtr& r) {
    return r->value;
  }

  static bool is_managed(const apps::mojom::PermissionPtr& r) {
    return r->is_managed;
  }

  static bool Read(crosapi::mojom::PermissionDataView,
                   apps::mojom::PermissionPtr* out);
};

template <>
struct EnumTraits<crosapi::mojom::PermissionType, apps::mojom::PermissionType> {
  static crosapi::mojom::PermissionType ToMojom(
      apps::mojom::PermissionType input);
  static bool FromMojom(crosapi::mojom::PermissionType input,
                        apps::mojom::PermissionType* output);
};

template <>
struct EnumTraits<crosapi::mojom::TriState, apps::mojom::TriState> {
  static crosapi::mojom::TriState ToMojom(apps::mojom::TriState input);
  static bool FromMojom(crosapi::mojom::TriState input,
                        apps::mojom::TriState* output);
};

template <>
struct UnionTraits<crosapi::mojom::PermissionValueDataView,
                   apps::mojom::PermissionValuePtr> {
  static crosapi::mojom::PermissionValueDataView::Tag GetTag(
      const apps::mojom::PermissionValuePtr& r);

  static bool IsNull(const apps::mojom::PermissionValuePtr& r) {
    return !r->is_bool_value() && !r->is_tristate_value();
  }

  static void SetToNull(apps::mojom::PermissionValuePtr* out) {}

  static bool bool_value(const apps::mojom::PermissionValuePtr& r) {
    return r->get_bool_value();
  }

  static apps::mojom::TriState tristate_value(
      const apps::mojom::PermissionValuePtr& r) {
    return r->get_tristate_value();
  }

  static bool Read(crosapi::mojom::PermissionValueDataView data,
                   apps::mojom::PermissionValuePtr* out);
};

template <>
struct StructTraits<crosapi::mojom::PreferredAppDataView,
                    apps::mojom::PreferredAppPtr> {
  static const apps::mojom::IntentFilterPtr& intent_filter(
      const apps::mojom::PreferredAppPtr& r) {
    return r->intent_filter;
  }

  static const std::string& app_id(const apps::mojom::PreferredAppPtr& r) {
    return r->app_id;
  }

  static bool Read(crosapi::mojom::PreferredAppDataView,
                   apps::mojom::PreferredAppPtr* out);
};

template <>
struct StructTraits<crosapi::mojom::PreferredAppChangesDataView,
                    apps::mojom::PreferredAppChangesPtr> {
  static const base::flat_map<std::string,
                              std::vector<apps::mojom::IntentFilterPtr>>&
  added_filters(const apps::mojom::PreferredAppChangesPtr& r) {
    return r->added_filters;
  }

  static const base::flat_map<std::string,
                              std::vector<apps::mojom::IntentFilterPtr>>&
  removed_filters(const apps::mojom::PreferredAppChangesPtr& r) {
    return r->removed_filters;
  }

  static bool Read(crosapi::mojom::PreferredAppChangesDataView,
                   apps::mojom::PreferredAppChangesPtr* out);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_APP_SERVICE_TYPES_MOJOM_TRAITS_H_
