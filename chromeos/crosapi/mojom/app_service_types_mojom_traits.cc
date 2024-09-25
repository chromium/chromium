// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/app_service_types_mojom_traits.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {

crosapi::mojom::OptionalBool ConvertOptionalBoolToMojomOptionalBool(
    const std::optional<bool>& option_bool) {
  if (!option_bool.has_value())
    return crosapi::mojom::OptionalBool::kUnknown;
  return option_bool.value() ? crosapi::mojom::OptionalBool::kTrue
                             : crosapi::mojom::OptionalBool::kFalse;
}

std::optional<bool> ConvertMojomOptionalBoolToOptionalBool(
    const crosapi::mojom::OptionalBool& mojom_option_bool) {
  switch (mojom_option_bool) {
    case crosapi::mojom::OptionalBool::kUnknown:
      return std::nullopt;
    case crosapi::mojom::OptionalBool::kTrue:
      return true;
    case crosapi::mojom::OptionalBool::kFalse:
      return false;
  }
}

apps::IconKeyPtr ConvertOptionalIconKeyToIconKeyPtr(
    const std::optional<apps::IconKey>& icon_key) {
  if (!icon_key.has_value()) {
    return nullptr;
  }
  return icon_key->Clone();
}

}  // namespace

namespace mojo {

apps::IconKeyPtr StructTraits<crosapi::mojom::AppDataView,
                              apps::AppPtr>::icon_key(const apps::AppPtr& r) {
  if (!r->icon_key.has_value()) {
    return nullptr;
  }

  auto icon_key = std::make_unique<apps::IconKey>(r->icon_key->resource_id,
                                                  r->icon_key->icon_effects);
  icon_key->update_version = r->icon_key->update_version;
  return icon_key;
}

// static
std::optional<std::string>
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::deprecated_policy_id(
    const apps::AppPtr& r) {
  if (!r->policy_ids.empty()) {
    return r->policy_ids[0];
  }
  return {};
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::recommendable(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->recommendable);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::searchable(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->searchable);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::show_in_launcher(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->show_in_launcher);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::show_in_shelf(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->show_in_shelf);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::show_in_search(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->show_in_search);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::show_in_management(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->show_in_management);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::has_badge(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->has_badge);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::paused(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->paused);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::allow_uninstall(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->allow_uninstall);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::handles_intents(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->handles_intents);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::is_platform_app(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->is_platform_app);
}

// static
std::optional<uint64_t>
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::app_size_in_bytes(
    const apps::AppPtr& r) {
  return r->app_size_in_bytes;
}

// static
std::optional<uint64_t>
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::data_size_in_bytes(
    const apps::AppPtr& r) {
  return r->data_size_in_bytes;
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::allow_close(
    const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->allow_close);
}

// static
crosapi::mojom::OptionalBool
StructTraits<crosapi::mojom::AppDataView,
             apps::AppPtr>::allow_window_mode_selection(const apps::AppPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->allow_window_mode_selection);
}

bool StructTraits<crosapi::mojom::AppDataView, apps::AppPtr>::Read(
    crosapi::mojom::AppDataView data,
    apps::AppPtr* out) {
  apps::AppType app_type;
  if (!data.ReadAppType(&app_type))
    return false;

  std::string app_id;
  if (!data.ReadAppId(&app_id))
    return false;

  apps::Readiness readiness;
  if (!data.ReadReadiness(&readiness))
    return false;

  std::optional<std::string> name;
  if (!data.ReadName(&name))
    return false;

  std::optional<std::string> short_name;
  if (!data.ReadShortName(&short_name))
    return false;

  std::optional<std::string> publisher_id;
  if (!data.ReadPublisherId(&publisher_id))
    return false;

  std::optional<std::string> description;
  if (!data.ReadDescription(&description))
    return false;

  std::optional<std::string> version;
  if (!data.ReadVersion(&version))
    return false;

  std::vector<std::string> additional_search_terms;
  if (!data.ReadAdditionalSearchTerms(&additional_search_terms))
    return false;

  apps::IconKeyPtr icon_key;
  if (!data.ReadIconKey(&icon_key))
    return false;

  std::optional<base::Time> last_launch_time;
  if (!data.ReadLastLaunchTime(&last_launch_time))
    return false;

  std::optional<base::Time> install_time;
  if (!data.ReadInstallTime(&install_time))
    return false;

  apps::InstallReason install_reason;
  if (!data.ReadInstallReason(&install_reason))
    return false;

  std::optional<std::string> deprecated_policy_id;
  if (!data.ReadDeprecatedPolicyId(&deprecated_policy_id))
    return false;

  std::vector<std::string> policy_ids;
  if (!data.ReadPolicyIds(&policy_ids))
    return false;

  crosapi::mojom::OptionalBool recommendable;
  if (!data.ReadRecommendable(&recommendable))
    return false;

  crosapi::mojom::OptionalBool searchable;
  if (!data.ReadSearchable(&searchable))
    return false;

  crosapi::mojom::OptionalBool show_in_launcher;
  if (!data.ReadShowInLauncher(&show_in_launcher))
    return false;

  crosapi::mojom::OptionalBool show_in_shelf;
  if (!data.ReadShowInShelf(&show_in_shelf))
    return false;

  crosapi::mojom::OptionalBool show_in_search;
  if (!data.ReadShowInSearch(&show_in_search))
    return false;

  crosapi::mojom::OptionalBool show_in_management;
  if (!data.ReadShowInManagement(&show_in_management))
    return false;

  crosapi::mojom::OptionalBool has_badge;
  if (!data.ReadHasBadge(&has_badge))
    return false;

  crosapi::mojom::OptionalBool paused;
  if (!data.ReadPaused(&paused))
    return false;

  apps::IntentFilters intent_filters;
  if (!data.ReadIntentFilters(&intent_filters))
    return false;

  apps::WindowMode window_mode;
  if (!data.ReadWindowMode(&window_mode))
    return false;

  apps::Permissions permissions;
  if (!data.ReadPermissions(&permissions))
    return false;

  crosapi::mojom::OptionalBool allow_uninstall;
  if (!data.ReadAllowUninstall(&allow_uninstall))
    return false;

  crosapi::mojom::OptionalBool handles_intents;
  if (!data.ReadHandlesIntents(&handles_intents))
    return false;

  crosapi::mojom::OptionalBool is_platform_app;
  if (!data.ReadIsPlatformApp(&is_platform_app))
    return false;

  std::optional<uint64_t> app_size_in_bytes = data.app_size_in_bytes();

  std::optional<uint64_t> data_size_in_bytes = data.data_size_in_bytes();

  crosapi::mojom::OptionalBool allow_close;
  if (!data.ReadAllowClose(&allow_close)) {
    return false;
  }

  crosapi::mojom::OptionalBool allow_window_mode_selection;
  if (!data.ReadAllowWindowModeSelection(&allow_window_mode_selection)) {
    return false;
  }

  std::optional<apps::PackageId> installer_package_id;
  if (!data.ReadInstallerPackageId(&installer_package_id)) {
    return false;
  }
  // If a PackageID is set but has an unknown type, it most likely means that
  // version skew caused the type to be dropped. Reset the value to nullopt, as
  // if it was not set in the first place.
  if (installer_package_id.has_value() &&
      installer_package_id->package_type() == apps::PackageType::kUnknown) {
    installer_package_id = std::nullopt;
  }

  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->short_name = short_name;
  app->publisher_id = publisher_id;
  app->description = description;
  app->version = version;
  app->additional_search_terms = std::move(additional_search_terms);
  if (icon_key)
    app->icon_key = std::move(*icon_key);
  app->last_launch_time = last_launch_time;
  app->install_time = install_time;
  app->install_reason = install_reason;

  if (!policy_ids.empty()) {
    app->policy_ids = std::move(policy_ids);
  } else if (deprecated_policy_id) {
    app->policy_ids = {std::move(*deprecated_policy_id)};
  }

  app->recommendable = ConvertMojomOptionalBoolToOptionalBool(recommendable);
  app->searchable = ConvertMojomOptionalBoolToOptionalBool(searchable);
  app->show_in_launcher =
      ConvertMojomOptionalBoolToOptionalBool(show_in_launcher);
  app->show_in_shelf = ConvertMojomOptionalBoolToOptionalBool(show_in_shelf);
  app->show_in_search = ConvertMojomOptionalBoolToOptionalBool(show_in_search);
  app->show_in_management =
      ConvertMojomOptionalBoolToOptionalBool(show_in_management);
  app->has_badge = ConvertMojomOptionalBoolToOptionalBool(has_badge);
  app->paused = ConvertMojomOptionalBoolToOptionalBool(paused);
  app->intent_filters = std::move(intent_filters);
  app->window_mode = window_mode;
  app->permissions = std::move(permissions);
  app->allow_uninstall =
      ConvertMojomOptionalBoolToOptionalBool(allow_uninstall);
  app->handles_intents =
      ConvertMojomOptionalBoolToOptionalBool(handles_intents);
  app->is_platform_app =
      ConvertMojomOptionalBoolToOptionalBool(is_platform_app);
  app->app_size_in_bytes = app_size_in_bytes;
  app->data_size_in_bytes = data_size_in_bytes;
  app->allow_close = ConvertMojomOptionalBoolToOptionalBool(allow_close);
  app->allow_window_mode_selection =
      ConvertMojomOptionalBoolToOptionalBool(allow_window_mode_selection);
  app->installer_package_id = installer_package_id;
  *out = std::move(app);
  return true;
}

crosapi::mojom::AppType
EnumTraits<crosapi::mojom::AppType, apps::AppType>::ToMojom(
    apps::AppType input) {
  switch (input) {
    case apps::AppType::kUnknown:
      return crosapi::mojom::AppType::kUnknown;
    case apps::AppType::kArc:
      return crosapi::mojom::AppType::kArc;
    case apps::AppType::kWeb:
      return crosapi::mojom::AppType::kWeb;
    case apps::AppType::kSystemWeb:
      return crosapi::mojom::AppType::kSystemWeb;
    case apps::AppType::kStandaloneBrowserChromeApp:
      return crosapi::mojom::AppType::kStandaloneBrowserChromeApp;
    case apps::AppType::kStandaloneBrowserExtension:
      return crosapi::mojom::AppType::kStandaloneBrowserExtension;
    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kPluginVm:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::AppType::kUnknown;
  }
}

bool EnumTraits<crosapi::mojom::AppType, apps::AppType>::FromMojom(
    crosapi::mojom::AppType input,
    apps::AppType* output) {
  switch (input) {
    case crosapi::mojom::AppType::kUnknown:
      *output = apps::AppType::kUnknown;
      return true;
    case crosapi::mojom::AppType::kArc:
      *output = apps::AppType::kArc;
      return true;
    case crosapi::mojom::AppType::kWeb:
      *output = apps::AppType::kWeb;
      return true;
    case crosapi::mojom::AppType::kSystemWeb:
      *output = apps::AppType::kSystemWeb;
      return true;
    case crosapi::mojom::AppType::kStandaloneBrowserChromeApp:
      *output = apps::AppType::kStandaloneBrowserChromeApp;
      return true;
    case crosapi::mojom::AppType::kStandaloneBrowserExtension:
      *output = apps::AppType::kStandaloneBrowserExtension;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::Readiness
EnumTraits<crosapi::mojom::Readiness, apps::Readiness>::ToMojom(
    apps::Readiness input) {
  switch (input) {
    case apps::Readiness::kUnknown:
      return crosapi::mojom::Readiness::kUnknown;
    case apps::Readiness::kReady:
      return crosapi::mojom::Readiness::kReady;
    case apps::Readiness::kDisabledByBlocklist:
      return crosapi::mojom::Readiness::kDisabledByBlocklist;
    case apps::Readiness::kDisabledByPolicy:
      return crosapi::mojom::Readiness::kDisabledByPolicy;
    case apps::Readiness::kDisabledByUser:
      return crosapi::mojom::Readiness::kDisabledByUser;
    case apps::Readiness::kTerminated:
      return crosapi::mojom::Readiness::kTerminated;
    case apps::Readiness::kUninstalledByUser:
      return crosapi::mojom::Readiness::kUninstalledByUser;
    case apps::Readiness::kRemoved:
      return crosapi::mojom::Readiness::kRemoved;
    case apps::Readiness::kUninstalledByNonUser:
      return crosapi::mojom::Readiness::kUninstalledByNonUser;
    case apps::Readiness::kDisabledByLocalSettings:
      return crosapi::mojom::Readiness::kDisabledByLocalSettings;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::Readiness, apps::Readiness>::FromMojom(
    crosapi::mojom::Readiness input,
    apps::Readiness* output) {
  switch (input) {
    case crosapi::mojom::Readiness::kUnknown:
      *output = apps::Readiness::kUnknown;
      return true;
    case crosapi::mojom::Readiness::kReady:
      *output = apps::Readiness::kReady;
      return true;
    case crosapi::mojom::Readiness::kDisabledByBlocklist:
      *output = apps::Readiness::kDisabledByBlocklist;
      return true;
    case crosapi::mojom::Readiness::kDisabledByPolicy:
      *output = apps::Readiness::kDisabledByPolicy;
      return true;
    case crosapi::mojom::Readiness::kDisabledByUser:
      *output = apps::Readiness::kDisabledByUser;
      return true;
    case crosapi::mojom::Readiness::kTerminated:
      *output = apps::Readiness::kTerminated;
      return true;
    case crosapi::mojom::Readiness::kUninstalledByUser:
      *output = apps::Readiness::kUninstalledByUser;
      return true;
    case crosapi::mojom::Readiness::kRemoved:
      *output = apps::Readiness::kRemoved;
      return true;
    case crosapi::mojom::Readiness::kUninstalledByNonUser:
      *output = apps::Readiness::kUninstalledByNonUser;
      return true;
    case crosapi::mojom::Readiness::kDisabledByLocalSettings:
      *output = apps::Readiness::kDisabledByLocalSettings;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::IconUpdateVersionDataView::Tag UnionTraits<
    crosapi::mojom::IconUpdateVersionDataView,
    apps::IconKey::UpdateVersion>::GetTag(const apps::IconKey::UpdateVersion&
                                              r) {
  if (absl::holds_alternative<bool>(r)) {
    return crosapi::mojom::IconUpdateVersionDataView::Tag::kRawIconUpdated;
  }
  if (absl::holds_alternative<int32_t>(r)) {
    return crosapi::mojom::IconUpdateVersionDataView::Tag::kTimeline;
  }
  NOTREACHED_IN_MIGRATION();
  return crosapi::mojom::IconUpdateVersionDataView::Tag::kRawIconUpdated;
}

bool UnionTraits<crosapi::mojom::IconUpdateVersionDataView,
                 apps::IconKey::UpdateVersion>::
    Read(crosapi::mojom::IconUpdateVersionDataView data,
         apps::IconKey::UpdateVersion* out) {
  switch (data.tag()) {
    case crosapi::mojom::IconUpdateVersionDataView::Tag::kRawIconUpdated: {
      *out = data.raw_icon_updated();
      return true;
    }
    case crosapi::mojom::IconUpdateVersionDataView::Tag::kTimeline: {
      *out = data.timeline();
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<crosapi::mojom::IconKeyDataView, apps::IconKeyPtr>::Read(
    crosapi::mojom::IconKeyDataView data,
    apps::IconKeyPtr* out) {
  apps::IconKey::UpdateVersion update_version;
  if (!data.ReadUpdateVersion(&update_version)) {
    return false;
  }

  *out = std::make_unique<apps::IconKey>(apps::IconKey::kInvalidResourceId,
                                         data.icon_effects());
  (*out)->update_version = std::move(update_version);

  return true;
}

crosapi::mojom::InstallReason
EnumTraits<crosapi::mojom::InstallReason, apps::InstallReason>::ToMojom(
    apps::InstallReason input) {
  switch (input) {
    case apps::InstallReason::kUnknown:
      return crosapi::mojom::InstallReason::kUnknown;
    case apps::InstallReason::kSystem:
      return crosapi::mojom::InstallReason::kSystem;
    case apps::InstallReason::kPolicy:
      return crosapi::mojom::InstallReason::kPolicy;
    case apps::InstallReason::kSubApp:
      return crosapi::mojom::InstallReason::kSubApp;
    case apps::InstallReason::kOem:
      return crosapi::mojom::InstallReason::kOem;
    case apps::InstallReason::kDefault:
      return crosapi::mojom::InstallReason::kDefault;
    case apps::InstallReason::kSync:
      return crosapi::mojom::InstallReason::kSync;
    case apps::InstallReason::kUser:
      return crosapi::mojom::InstallReason::kUser;
    case apps::InstallReason::kKiosk:
      return crosapi::mojom::InstallReason::kKiosk;
    case apps::InstallReason::kCommandLine:
      return crosapi::mojom::InstallReason::kCommandLine;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::InstallReason, apps::InstallReason>::FromMojom(
    crosapi::mojom::InstallReason input,
    apps::InstallReason* output) {
  switch (input) {
    case crosapi::mojom::InstallReason::kUnknown:
      *output = apps::InstallReason::kUnknown;
      return true;
    case crosapi::mojom::InstallReason::kSystem:
      *output = apps::InstallReason::kSystem;
      return true;
    case crosapi::mojom::InstallReason::kPolicy:
      *output = apps::InstallReason::kPolicy;
      return true;
    case crosapi::mojom::InstallReason::kOem:
      *output = apps::InstallReason::kOem;
      return true;
    case crosapi::mojom::InstallReason::kDefault:
      *output = apps::InstallReason::kDefault;
      return true;
    case crosapi::mojom::InstallReason::kSync:
      *output = apps::InstallReason::kSync;
      return true;
    case crosapi::mojom::InstallReason::kUser:
      *output = apps::InstallReason::kUser;
      return true;
    case crosapi::mojom::InstallReason::kSubApp:
      *output = apps::InstallReason::kSubApp;
      return true;
    case crosapi::mojom::InstallReason::kKiosk:
      *output = apps::InstallReason::kKiosk;
      return true;
    case crosapi::mojom::InstallReason::kCommandLine:
      *output = apps::InstallReason::kCommandLine;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<crosapi::mojom::IntentFilterDataView, apps::IntentFilterPtr>::
    Read(crosapi::mojom::IntentFilterDataView data,
         apps::IntentFilterPtr* out) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  apps::Conditions conditions;
  if (!data.ReadConditions(&conditions))
    return false;
  std::optional<std::string> activity_name;
  if (!data.ReadActivityName(&activity_name))
    return false;
  std::optional<std::string> activity_label;
  if (!data.ReadActivityLabel(&activity_label))
    return false;

  intent_filter->conditions = std::move(conditions);
  intent_filter->activity_name = activity_name;
  intent_filter->activity_label = activity_label;
  *out = std::move(intent_filter);
  return true;
}

bool StructTraits<crosapi::mojom::ConditionDataView, apps::ConditionPtr>::Read(
    crosapi::mojom::ConditionDataView data,
    apps::ConditionPtr* out) {
  apps::ConditionType condition_type;
  if (!data.ReadConditionType(&condition_type))
    return false;
  apps::ConditionValues condition_values;
  if (!data.ReadConditionValues(&condition_values))
    return false;

  *out = std::make_unique<apps::Condition>(condition_type,
                                           std::move(condition_values));
  return true;
}

crosapi::mojom::ConditionType
EnumTraits<crosapi::mojom::ConditionType, apps::ConditionType>::ToMojom(
    apps::ConditionType input) {
  switch (input) {
    case apps::ConditionType::kScheme:
      return crosapi::mojom::ConditionType::kScheme;
    case apps::ConditionType::kAuthority:
      return crosapi::mojom::ConditionType::kAuthority;
    case apps::ConditionType::kPath:
      return crosapi::mojom::ConditionType::kPath;
    case apps::ConditionType::kAction:
      return crosapi::mojom::ConditionType::kAction;
    case apps::ConditionType::kMimeType:
      return crosapi::mojom::ConditionType::kMimeType;
    case apps::ConditionType::kFile:
      return crosapi::mojom::ConditionType::kFile;
  }

  NOTREACHED_IN_MIGRATION();
}

bool StructTraits<
    crosapi::mojom::ConditionValueDataView,
    apps::ConditionValuePtr>::Read(crosapi::mojom::ConditionValueDataView data,
                                   apps::ConditionValuePtr* out) {
  std::string value;
  if (!data.ReadValue(&value))
    return false;
  apps::PatternMatchType match_type;
  if (!data.ReadMatchType(&match_type))
    return false;

  *out = std::make_unique<apps::ConditionValue>(value, match_type);
  return true;
}

bool EnumTraits<crosapi::mojom::ConditionType, apps::ConditionType>::FromMojom(
    crosapi::mojom::ConditionType input,
    apps::ConditionType* output) {
  switch (input) {
    case crosapi::mojom::ConditionType::kScheme:
      *output = apps::ConditionType::kScheme;
      return true;
    case crosapi::mojom::ConditionType::kAuthority:
      *output = apps::ConditionType::kAuthority;
      return true;
    case crosapi::mojom::ConditionType::kPath:
      *output = apps::ConditionType::kPath;
      return true;
    case crosapi::mojom::ConditionType::kAction:
      *output = apps::ConditionType::kAction;
      return true;
    case crosapi::mojom::ConditionType::kMimeType:
      *output = apps::ConditionType::kMimeType;
      return true;
    case crosapi::mojom::ConditionType::kFileExtension:
    case crosapi::mojom::ConditionType::kFile:
      *output = apps::ConditionType::kFile;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::PatternMatchType
EnumTraits<crosapi::mojom::PatternMatchType, apps::PatternMatchType>::ToMojom(
    apps::PatternMatchType input) {
  switch (input) {
    case apps::PatternMatchType::kLiteral:
      return crosapi::mojom::PatternMatchType::kLiteral;
    case apps::PatternMatchType::kPrefix:
      return crosapi::mojom::PatternMatchType::kPrefix;
    case apps::PatternMatchType::kGlob:
      return crosapi::mojom::PatternMatchType::kGlob;
    case apps::PatternMatchType::kMimeType:
      return crosapi::mojom::PatternMatchType::kMimeType;
    case apps::PatternMatchType::kFileExtension:
      return crosapi::mojom::PatternMatchType::kFileExtension;
    case apps::PatternMatchType::kIsDirectory:
      return crosapi::mojom::PatternMatchType::kIsDirectory;
    case apps::PatternMatchType::kSuffix:
      return crosapi::mojom::PatternMatchType::kSuffix;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::PatternMatchType, apps::PatternMatchType>::
    FromMojom(crosapi::mojom::PatternMatchType input,
              apps::PatternMatchType* output) {
  switch (input) {
    case crosapi::mojom::PatternMatchType::kNone:
    case crosapi::mojom::PatternMatchType::kLiteral:
      *output = apps::PatternMatchType::kLiteral;
      return true;
    case crosapi::mojom::PatternMatchType::kPrefix:
      *output = apps::PatternMatchType::kPrefix;
      return true;
    case crosapi::mojom::PatternMatchType::kGlob:
      *output = apps::PatternMatchType::kGlob;
      return true;
    case crosapi::mojom::PatternMatchType::kMimeType:
      *output = apps::PatternMatchType::kMimeType;
      return true;
    case crosapi::mojom::PatternMatchType::kFileExtension:
      *output = apps::PatternMatchType::kFileExtension;
      return true;
    case crosapi::mojom::PatternMatchType::kIsDirectory:
      *output = apps::PatternMatchType::kIsDirectory;
      return true;
    case crosapi::mojom::PatternMatchType::kSuffix:
      *output = apps::PatternMatchType::kSuffix;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::UninstallSource
EnumTraits<crosapi::mojom::UninstallSource, apps::UninstallSource>::ToMojom(
    apps::UninstallSource input) {
  switch (input) {
    case apps::UninstallSource::kUnknown:
      return crosapi::mojom::UninstallSource::kUnknown;
    case apps::UninstallSource::kAppList:
      return crosapi::mojom::UninstallSource::kAppList;
    case apps::UninstallSource::kAppManagement:
      return crosapi::mojom::UninstallSource::kAppManagement;
    case apps::UninstallSource::kShelf:
      return crosapi::mojom::UninstallSource::kShelf;
    case apps::UninstallSource::kMigration:
      return crosapi::mojom::UninstallSource::kMigration;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::UninstallSource, apps::UninstallSource>::
    FromMojom(crosapi::mojom::UninstallSource input,
              apps::UninstallSource* output) {
  switch (input) {
    case crosapi::mojom::UninstallSource::kUnknown:
      *output = apps::UninstallSource::kUnknown;
      return true;
    case crosapi::mojom::UninstallSource::kAppList:
      *output = apps::UninstallSource::kAppList;
      return true;
    case crosapi::mojom::UninstallSource::kAppManagement:
      *output = apps::UninstallSource::kAppManagement;
      return true;
    case crosapi::mojom::UninstallSource::kShelf:
      *output = apps::UninstallSource::kShelf;
      return true;
    case crosapi::mojom::UninstallSource::kMigration:
      *output = apps::UninstallSource::kMigration;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
crosapi::mojom::OptionalBool StructTraits<
    crosapi::mojom::CapabilityAccessDataView,
    apps::CapabilityAccessPtr>::camera(const apps::CapabilityAccessPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->camera);
}

// static
crosapi::mojom::OptionalBool StructTraits<
    crosapi::mojom::CapabilityAccessDataView,
    apps::CapabilityAccessPtr>::microphone(const apps::CapabilityAccessPtr& r) {
  return ConvertOptionalBoolToMojomOptionalBool(r->microphone);
}

bool StructTraits<crosapi::mojom::CapabilityAccessDataView,
                  apps::CapabilityAccessPtr>::
    Read(crosapi::mojom::CapabilityAccessDataView data,
         apps::CapabilityAccessPtr* out) {
  std::string app_id;
  if (!data.ReadAppId(&app_id))
    return false;

  crosapi::mojom::OptionalBool camera;
  if (!data.ReadCamera(&camera))
    return false;

  crosapi::mojom::OptionalBool microphone;
  if (!data.ReadMicrophone(&microphone))
    return false;

  auto capability_access = std::make_unique<apps::CapabilityAccess>(app_id);
  capability_access->camera = ConvertMojomOptionalBoolToOptionalBool(camera);
  capability_access->microphone =
      ConvertMojomOptionalBoolToOptionalBool(microphone);
  *out = std::move(capability_access);
  return true;
}

crosapi::mojom::IconType
EnumTraits<crosapi::mojom::IconType, apps::IconType>::ToMojom(
    apps::IconType input) {
  switch (input) {
    case apps::IconType::kUnknown:
      return crosapi::mojom::IconType::kUnknown;
    case apps::IconType::kUncompressed:
      return crosapi::mojom::IconType::kUncompressed;
    case apps::IconType::kCompressed:
      return crosapi::mojom::IconType::kCompressed;
    case apps::IconType::kStandard:
      return crosapi::mojom::IconType::kStandard;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::IconType, apps::IconType>::FromMojom(
    crosapi::mojom::IconType input,
    apps::IconType* output) {
  switch (input) {
    case crosapi::mojom::IconType::kUnknown:
      *output = apps::IconType::kUnknown;
      return true;
    case crosapi::mojom::IconType::kUncompressed:
      *output = apps::IconType::kUncompressed;
      return true;
    case crosapi::mojom::IconType::kCompressed:
      *output = apps::IconType::kCompressed;
      return true;
    case crosapi::mojom::IconType::kStandard:
      *output = apps::IconType::kStandard;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<crosapi::mojom::IconValueDataView, apps::IconValuePtr>::Read(
    crosapi::mojom::IconValueDataView data,
    apps::IconValuePtr* out) {
  apps::IconType icon_type;
  if (!data.ReadIconType(&icon_type))
    return false;

  gfx::ImageSkia uncompressed;
  if (!data.ReadUncompressed(&uncompressed))
    return false;

  std::vector<uint8_t> compressed;
  if (!data.ReadCompressed(&compressed))
    return false;

  auto icon_value = std::make_unique<apps::IconValue>();
  icon_value->icon_type = icon_type;
  icon_value->uncompressed = std::move(uncompressed);
  icon_value->compressed = std::move(compressed);
  icon_value->is_placeholder_icon = data.is_placeholder_icon();
  icon_value->is_maskable_icon = data.is_maskable_icon();
  *out = std::move(icon_value);
  return true;
}

crosapi::mojom::WindowMode
EnumTraits<crosapi::mojom::WindowMode, apps::WindowMode>::ToMojom(
    apps::WindowMode input) {
  switch (input) {
    case apps::WindowMode::kUnknown:
      return crosapi::mojom::WindowMode::kUnknown;
    case apps::WindowMode::kWindow:
      return crosapi::mojom::WindowMode::kWindow;
    case apps::WindowMode::kBrowser:
      return crosapi::mojom::WindowMode::kBrowser;
    case apps::WindowMode::kTabbedWindow:
      return crosapi::mojom::WindowMode::kTabbedWindow;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::WindowMode, apps::WindowMode>::FromMojom(
    crosapi::mojom::WindowMode input,
    apps::WindowMode* output) {
  switch (input) {
    case crosapi::mojom::WindowMode::kUnknown:
      *output = apps::WindowMode::kUnknown;
      return true;
    case crosapi::mojom::WindowMode::kWindow:
      *output = apps::WindowMode::kWindow;
      return true;
    case crosapi::mojom::WindowMode::kBrowser:
      *output = apps::WindowMode::kBrowser;
      return true;
    case crosapi::mojom::WindowMode::kTabbedWindow:
      *output = apps::WindowMode::kTabbedWindow;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::LaunchSource
EnumTraits<crosapi::mojom::LaunchSource, apps::LaunchSource>::ToMojom(
    apps::LaunchSource input) {
  switch (input) {
    case apps::LaunchSource::kUnknown:
      return crosapi::mojom::LaunchSource::kUnknown;
    case apps::LaunchSource::kFromAppListGrid:
      return crosapi::mojom::LaunchSource::kFromAppListGrid;
    case apps::LaunchSource::kFromAppListGridContextMenu:
      return crosapi::mojom::LaunchSource::kFromAppListGridContextMenu;
    case apps::LaunchSource::kFromAppListQuery:
      return crosapi::mojom::LaunchSource::kFromAppListQuery;
    case apps::LaunchSource::kFromAppListQueryContextMenu:
      return crosapi::mojom::LaunchSource::kFromAppListQueryContextMenu;
    case apps::LaunchSource::kFromAppListRecommendation:
      return crosapi::mojom::LaunchSource::kFromAppListRecommendation;
    case apps::LaunchSource::kFromParentalControls:
      return crosapi::mojom::LaunchSource::kFromParentalControls;
    case apps::LaunchSource::kFromShelf:
      return crosapi::mojom::LaunchSource::kFromShelf;
    case apps::LaunchSource::kFromFileManager:
      return crosapi::mojom::LaunchSource::kFromFileManager;
    case apps::LaunchSource::kFromLink:
      return crosapi::mojom::LaunchSource::kFromLink;
    case apps::LaunchSource::kFromOmnibox:
      return crosapi::mojom::LaunchSource::kFromOmnibox;
    case apps::LaunchSource::kFromChromeInternal:
      return crosapi::mojom::LaunchSource::kFromChromeInternal;
    case apps::LaunchSource::kFromKeyboard:
      return crosapi::mojom::LaunchSource::kFromKeyboard;
    case apps::LaunchSource::kFromOtherApp:
      return crosapi::mojom::LaunchSource::kFromOtherApp;
    case apps::LaunchSource::kFromMenu:
      return crosapi::mojom::LaunchSource::kFromMenu;
    case apps::LaunchSource::kFromInstalledNotification:
      return crosapi::mojom::LaunchSource::kFromInstalledNotification;
    case apps::LaunchSource::kFromTest:
      return crosapi::mojom::LaunchSource::kFromTest;
    case apps::LaunchSource::kFromArc:
      return crosapi::mojom::LaunchSource::kFromArc;
    case apps::LaunchSource::kFromSharesheet:
      return crosapi::mojom::LaunchSource::kFromSharesheet;
    case apps::LaunchSource::kFromReleaseNotesNotification:
      return crosapi::mojom::LaunchSource::kFromReleaseNotesNotification;
    case apps::LaunchSource::kFromFullRestore:
      return crosapi::mojom::LaunchSource::kFromFullRestore;
    case apps::LaunchSource::kFromSmartTextContextMenu:
      return crosapi::mojom::LaunchSource::kFromSmartTextContextMenu;
    case apps::LaunchSource::kFromDiscoverTabNotification:
      return crosapi::mojom::LaunchSource::kFromDiscoverTabNotification;
    case apps::LaunchSource::kFromManagementApi:
      return crosapi::mojom::LaunchSource::kFromManagementApi;
    case apps::LaunchSource::kFromKiosk:
      return crosapi::mojom::LaunchSource::kFromKiosk;
    case apps::LaunchSource::kFromNewTabPage:
      return crosapi::mojom::LaunchSource::kFromNewTabPage;
    case apps::LaunchSource::kFromIntentUrl:
      return crosapi::mojom::LaunchSource::kFromIntentUrl;
    case apps::LaunchSource::kFromOsLogin:
      return crosapi::mojom::LaunchSource::kFromOsLogin;
    case apps::LaunchSource::kFromProtocolHandler:
      return crosapi::mojom::LaunchSource::kFromProtocolHandler;
    case apps::LaunchSource::kFromUrlHandler:
      return crosapi::mojom::LaunchSource::kFromUrlHandler;
    case apps::LaunchSource::kFromSysTrayCalendar:
      return crosapi::mojom::LaunchSource::kFromSysTrayCalendar;
    case apps::LaunchSource::kFromInstaller:
      return crosapi::mojom::LaunchSource::kFromInstaller;
    case apps::LaunchSource::kFromFirstRun:
      return crosapi::mojom::LaunchSource::kFromFirstRun;
    case apps::LaunchSource::kFromWelcomeTour:
      return crosapi::mojom::LaunchSource::kFromWelcomeTour;
    case apps::LaunchSource::kFromFocusMode:
      return crosapi::mojom::LaunchSource::kFromFocusMode;
    case apps::LaunchSource::kFromSparky:
      return crosapi::mojom::LaunchSource::kFromSparky;
    case apps::LaunchSource::kFromNavigationCapturing:
      return crosapi::mojom::LaunchSource::kFromNavigationCapturing;
    // TODO(crbug.com/40852514): Make lock screen apps use Lacros browser.
    case apps::LaunchSource::kFromLockScreen:
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromAppHomePage:
    case apps::LaunchSource::kFromReparenting:
    case apps::LaunchSource::kFromProfileMenu:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::LaunchSource::kUnknown;
  }
  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::LaunchSource, apps::LaunchSource>::FromMojom(
    crosapi::mojom::LaunchSource input,
    apps::LaunchSource* output) {
  switch (input) {
    case crosapi::mojom::LaunchSource::kUnknown:
      *output = apps::LaunchSource::kUnknown;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListGrid:
      *output = apps::LaunchSource::kFromAppListGrid;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListGridContextMenu:
      *output = apps::LaunchSource::kFromAppListGridContextMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListQuery:
      *output = apps::LaunchSource::kFromAppListQuery;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListQueryContextMenu:
      *output = apps::LaunchSource::kFromAppListQueryContextMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListRecommendation:
      *output = apps::LaunchSource::kFromAppListRecommendation;
      return true;
    case crosapi::mojom::LaunchSource::kFromParentalControls:
      *output = apps::LaunchSource::kFromParentalControls;
      return true;
    case crosapi::mojom::LaunchSource::kFromShelf:
      *output = apps::LaunchSource::kFromShelf;
      return true;
    case crosapi::mojom::LaunchSource::kFromFileManager:
      *output = apps::LaunchSource::kFromFileManager;
      return true;
    case crosapi::mojom::LaunchSource::kFromLink:
      *output = apps::LaunchSource::kFromLink;
      return true;
    case crosapi::mojom::LaunchSource::kFromOmnibox:
      *output = apps::LaunchSource::kFromOmnibox;
      return true;
    case crosapi::mojom::LaunchSource::kFromChromeInternal:
      *output = apps::LaunchSource::kFromChromeInternal;
      return true;
    case crosapi::mojom::LaunchSource::kFromKeyboard:
      *output = apps::LaunchSource::kFromKeyboard;
      return true;
    case crosapi::mojom::LaunchSource::kFromOtherApp:
      *output = apps::LaunchSource::kFromOtherApp;
      return true;
    case crosapi::mojom::LaunchSource::kFromMenu:
      *output = apps::LaunchSource::kFromMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromInstalledNotification:
      *output = apps::LaunchSource::kFromInstalledNotification;
      return true;
    case crosapi::mojom::LaunchSource::kFromTest:
      *output = apps::LaunchSource::kFromTest;
      return true;
    case crosapi::mojom::LaunchSource::kFromArc:
      *output = apps::LaunchSource::kFromArc;
      return true;
    case crosapi::mojom::LaunchSource::kFromSharesheet:
      *output = apps::LaunchSource::kFromSharesheet;
      return true;
    case crosapi::mojom::LaunchSource::kFromReleaseNotesNotification:
      *output = apps::LaunchSource::kFromReleaseNotesNotification;
      return true;
    case crosapi::mojom::LaunchSource::kFromFullRestore:
      *output = apps::LaunchSource::kFromFullRestore;
      return true;
    case crosapi::mojom::LaunchSource::kFromSmartTextContextMenu:
      *output = apps::LaunchSource::kFromSmartTextContextMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromDiscoverTabNotification:
      *output = apps::LaunchSource::kFromDiscoverTabNotification;
      return true;
    case crosapi::mojom::LaunchSource::kFromManagementApi:
      *output = apps::LaunchSource::kFromManagementApi;
      return true;
    case crosapi::mojom::LaunchSource::kFromKiosk:
      *output = apps::LaunchSource::kFromKiosk;
      return true;
    case crosapi::mojom::LaunchSource::kFromNewTabPage:
      *output = apps::LaunchSource::kFromNewTabPage;
      return true;
    case crosapi::mojom::LaunchSource::kFromIntentUrl:
      *output = apps::LaunchSource::kFromIntentUrl;
      return true;
    case crosapi::mojom::LaunchSource::kFromOsLogin:
      *output = apps::LaunchSource::kFromOsLogin;
      return true;
    case crosapi::mojom::LaunchSource::kFromProtocolHandler:
      *output = apps::LaunchSource::kFromProtocolHandler;
      return true;
    case crosapi::mojom::LaunchSource::kFromUrlHandler:
      *output = apps::LaunchSource::kFromUrlHandler;
      return true;
    case crosapi::mojom::LaunchSource::kFromSysTrayCalendar:
      *output = apps::LaunchSource::kFromSysTrayCalendar;
      return true;
    case crosapi::mojom::LaunchSource::kFromInstaller:
      *output = apps::LaunchSource::kFromInstaller;
      return true;
    case crosapi::mojom::LaunchSource::kFromFirstRun:
      *output = apps::LaunchSource::kFromFirstRun;
      return true;
    case crosapi::mojom::LaunchSource::kFromWelcomeTour:
      *output = apps::LaunchSource::kFromWelcomeTour;
      return true;
    case crosapi::mojom::LaunchSource::kFromFocusMode:
      *output = apps::LaunchSource::kFromFocusMode;
      return true;
    case crosapi::mojom::LaunchSource::kFromSparky:
      *output = apps::LaunchSource::kFromSparky;
      return true;
    case crosapi::mojom::LaunchSource::kFromNavigationCapturing:
      *output = apps::LaunchSource::kFromNavigationCapturing;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<crosapi::mojom::PermissionDataView, apps::PermissionPtr>::
    Read(crosapi::mojom::PermissionDataView data, apps::PermissionPtr* out) {
  apps::PermissionType permission_type;
  if (!data.ReadPermissionType(&permission_type))
    return false;

  apps::Permission::PermissionValue value;
  if (!data.ReadValue(&value))
    return false;

  *out = std::make_unique<apps::Permission>(permission_type, std::move(value),
                                            data.is_managed());
  return true;
}

crosapi::mojom::PermissionType
EnumTraits<crosapi::mojom::PermissionType, apps::PermissionType>::ToMojom(
    apps::PermissionType input) {
  switch (input) {
    case apps::PermissionType::kUnknown:
      return crosapi::mojom::PermissionType::kUnknown;
    case apps::PermissionType::kCamera:
      return crosapi::mojom::PermissionType::kCamera;
    case apps::PermissionType::kLocation:
      return crosapi::mojom::PermissionType::kLocation;
    case apps::PermissionType::kMicrophone:
      return crosapi::mojom::PermissionType::kMicrophone;
    case apps::PermissionType::kNotifications:
      return crosapi::mojom::PermissionType::kNotifications;
    case apps::PermissionType::kContacts:
      return crosapi::mojom::PermissionType::kContacts;
    case apps::PermissionType::kStorage:
      return crosapi::mojom::PermissionType::kStorage;
    case apps::PermissionType::kFileHandling:
      return crosapi::mojom::PermissionType::kFileHandling;
    case apps::PermissionType::kPrinting:
      NOTREACHED_IN_MIGRATION();
      return crosapi::mojom::PermissionType::kUnknown;
  }
}

bool EnumTraits<crosapi::mojom::PermissionType,
                apps::PermissionType>::FromMojom(crosapi::mojom::PermissionType
                                                     input,
                                                 apps::PermissionType* output) {
  switch (input) {
    case crosapi::mojom::PermissionType::kUnknown:
      *output = apps::PermissionType::kUnknown;
      return true;
    case crosapi::mojom::PermissionType::kCamera:
      *output = apps::PermissionType::kCamera;
      return true;
    case crosapi::mojom::PermissionType::kLocation:
      *output = apps::PermissionType::kLocation;
      return true;
    case crosapi::mojom::PermissionType::kMicrophone:
      *output = apps::PermissionType::kMicrophone;
      return true;
    case crosapi::mojom::PermissionType::kNotifications:
      *output = apps::PermissionType::kNotifications;
      return true;
    case crosapi::mojom::PermissionType::kContacts:
      *output = apps::PermissionType::kContacts;
      return true;
    case crosapi::mojom::PermissionType::kStorage:
      *output = apps::PermissionType::kStorage;
      return true;
    case crosapi::mojom::PermissionType::kFileHandling:
      *output = apps::PermissionType::kFileHandling;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::TriState
EnumTraits<crosapi::mojom::TriState, apps::TriState>::ToMojom(
    apps::TriState input) {
  switch (input) {
    case apps::TriState::kAllow:
      return crosapi::mojom::TriState::kAllow;
    case apps::TriState::kBlock:
      return crosapi::mojom::TriState::kBlock;
    case apps::TriState::kAsk:
      return crosapi::mojom::TriState::kAsk;
  }

  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::TriState, apps::TriState>::FromMojom(
    crosapi::mojom::TriState input,
    apps::TriState* output) {
  switch (input) {
    case crosapi::mojom::TriState::kAllow:
      *output = apps::TriState::kAllow;
      return true;
    case crosapi::mojom::TriState::kBlock:
      *output = apps::TriState::kBlock;
      return true;
    case crosapi::mojom::TriState::kAsk:
      *output = apps::TriState::kAsk;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::PermissionValueDataView::Tag
UnionTraits<crosapi::mojom::PermissionValueDataView,
            apps::Permission::PermissionValue>::
    GetTag(const apps::Permission::PermissionValue& r) {
  if (absl::holds_alternative<bool>(r)) {
    return crosapi::mojom::PermissionValueDataView::Tag::kBoolValue;
  }
  if (absl::holds_alternative<apps::TriState>(r)) {
    return crosapi::mojom::PermissionValueDataView::Tag::kTristateValue;
  }
  NOTREACHED_IN_MIGRATION();
  return crosapi::mojom::PermissionValueDataView::Tag::kBoolValue;
}

bool UnionTraits<crosapi::mojom::PermissionValueDataView,
                 apps::Permission::PermissionValue>::
    Read(crosapi::mojom::PermissionValueDataView data,
         apps::Permission::PermissionValue* out) {
  switch (data.tag()) {
    case crosapi::mojom::PermissionValueDataView::Tag::kBoolValue: {
      *out = data.bool_value();
      return true;
    }
    case crosapi::mojom::PermissionValueDataView::Tag::kTristateValue: {
      apps::TriState tristate_value;
      if (!data.ReadTristateValue(&tristate_value))
        return false;
      *out = tristate_value;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool StructTraits<crosapi::mojom::PreferredAppDataView, apps::PreferredAppPtr>::
    Read(crosapi::mojom::PreferredAppDataView data,
         apps::PreferredAppPtr* out) {
  apps::IntentFilterPtr intent_filter;
  if (!data.ReadIntentFilter(&intent_filter))
    return false;

  std::string app_id;
  if (!data.ReadAppId(&app_id))
    return false;

  *out = std::make_unique<apps::PreferredApp>(std::move(intent_filter), app_id);
  return true;
}

bool StructTraits<crosapi::mojom::PreferredAppChangesDataView,
                  apps::PreferredAppChangesPtr>::
    Read(crosapi::mojom::PreferredAppChangesDataView data,
         apps::PreferredAppChangesPtr* out) {
  base::flat_map<std::string, apps::IntentFilters> added_filters;
  if (!data.ReadAddedFilters(&added_filters))
    return false;

  base::flat_map<std::string, apps::IntentFilters> removed_filters;
  if (!data.ReadRemovedFilters(&removed_filters))
    return false;

  auto preferred_app_changes = std::make_unique<apps::PreferredAppChanges>();
  preferred_app_changes->added_filters = std::move(added_filters);
  preferred_app_changes->removed_filters = std::move(removed_filters);
  *out = std::move(preferred_app_changes);
  return true;
}

apps::IconKeyPtr
StructTraits<crosapi::mojom::AppShortcutDataView, apps::ShortcutPtr>::icon_key(
    const apps::ShortcutPtr& r) {
  return ConvertOptionalIconKeyToIconKeyPtr(r->icon_key);
}

bool StructTraits<crosapi::mojom::AppShortcutDataView, apps::ShortcutPtr>::Read(
    crosapi::mojom::AppShortcutDataView data,
    apps::ShortcutPtr* out) {
  std::string host_app_id;
  if (!data.ReadHostAppId(&host_app_id)) {
    return false;
  }

  std::string local_id;
  if (!data.ReadLocalId(&local_id)) {
    return false;
  }

  std::optional<std::string> name;
  if (!data.ReadName(&name)) {
    return false;
  }

  apps::IconKeyPtr icon_key;
  if (!data.ReadIconKey(&icon_key)) {
    return false;
  }

  auto shortcut = std::make_unique<apps::Shortcut>(host_app_id, local_id);
  shortcut->name = name;
  // Currently all shortcuts are User created, will add this field on crosapi
  // when we support developer created shortcuts.
  shortcut->shortcut_source = apps::ShortcutSource::kUser;
  if (icon_key) {
    shortcut->icon_key = std::move(*icon_key);
  }
  shortcut->allow_removal = data.allow_removal();

  *out = std::move(shortcut);
  return true;
}

// static
crosapi::mojom::PackageIdType
StructTraits<crosapi::mojom::PackageIdDataView, apps::PackageId>::package_type(
    const apps::PackageId& r) {
  switch (r.package_type()) {
    case apps::PackageType::kArc:
      return crosapi::mojom::PackageIdType::kArc;
    case apps::PackageType::kWeb:
      return crosapi::mojom::PackageIdType::kWeb;
    case apps::PackageType::kUnknown:
    case apps::PackageType::kBorealis:
    case apps::PackageType::kChromeApp:
    case apps::PackageType::kGeForceNow:
    case apps::PackageType::kSystem:
    case apps::PackageType::kWebsite:
      return crosapi::mojom::PackageIdType::kUnknown;
  }
}

bool StructTraits<crosapi::mojom::PackageIdDataView, apps::PackageId>::Read(
    crosapi::mojom::PackageIdDataView data,
    apps::PackageId* out) {
  crosapi::mojom::PackageIdType mojom_package_type = data.package_type();

  std::string identifier;
  if (!data.ReadIdentifier(&identifier) || identifier.empty()) {
    return false;
  }

  apps::PackageType package_type = ([&mojom_package_type]() {
    switch (mojom_package_type) {
      case crosapi::mojom::PackageIdType::kUnknown:
        return apps::PackageType::kUnknown;
      case crosapi::mojom::PackageIdType::kArc:
        return apps::PackageType::kArc;
      case crosapi::mojom::PackageIdType::kWeb:
        return apps::PackageType::kWeb;
    }
  })();

  *out = apps::PackageId(package_type, identifier);

  return true;
}

}  // namespace mojo
