// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/app_service_types_mojom_traits.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"

namespace mojo {

bool StructTraits<crosapi::mojom::AppDataView, apps::mojom::AppPtr>::Read(
    crosapi::mojom::AppDataView data,
    apps::mojom::AppPtr* out) {
  apps::mojom::AppType app_type;
  if (!data.ReadAppType(&app_type))
    return false;

  std::string app_id;
  if (!data.ReadAppId(&app_id))
    return false;

  apps::mojom::Readiness readiness;
  if (!data.ReadReadiness(&readiness))
    return false;

  absl::optional<std::string> name;
  if (!data.ReadName(&name))
    return false;

  absl::optional<std::string> short_name;
  if (!data.ReadShortName(&short_name))
    return false;

  absl::optional<std::string> publisher_id;
  if (!data.ReadPublisherId(&publisher_id))
    return false;

  absl::optional<std::string> description;
  if (!data.ReadDescription(&description))
    return false;

  absl::optional<std::string> version;
  if (!data.ReadVersion(&version))
    return false;

  std::vector<std::string> additional_search_terms;
  if (!data.ReadAdditionalSearchTerms(&additional_search_terms))
    return false;

  apps::mojom::IconKeyPtr icon_key;
  if (!data.ReadIconKey(&icon_key))
    return false;

  absl::optional<base::Time> last_launch_time;
  if (!data.ReadLastLaunchTime(&last_launch_time))
    return false;

  absl::optional<base::Time> install_time;
  if (!data.ReadInstallTime(&install_time))
    return false;

  apps::mojom::InstallReason install_reason;
  if (!data.ReadInstallReason(&install_reason))
    return false;

  absl::optional<std::string> policy_id;
  if (!data.ReadPolicyId(&policy_id))
    return false;

  apps::mojom::OptionalBool recommendable;
  if (!data.ReadRecommendable(&recommendable))
    return false;

  apps::mojom::OptionalBool searchable;
  if (!data.ReadSearchable(&searchable))
    return false;

  apps::mojom::OptionalBool show_in_launcher;
  if (!data.ReadShowInLauncher(&show_in_launcher))
    return false;

  apps::mojom::OptionalBool show_in_shelf;
  if (!data.ReadShowInShelf(&show_in_shelf))
    return false;

  apps::mojom::OptionalBool show_in_search;
  if (!data.ReadShowInSearch(&show_in_search))
    return false;

  apps::mojom::OptionalBool show_in_management;
  if (!data.ReadShowInManagement(&show_in_management))
    return false;

  apps::mojom::OptionalBool has_badge;
  if (!data.ReadHasBadge(&has_badge))
    return false;

  apps::mojom::OptionalBool paused;
  if (!data.ReadPaused(&paused))
    return false;

  std::vector<apps::mojom::IntentFilterPtr> intent_filters;
  if (!data.ReadIntentFilters(&intent_filters))
    return false;

  apps::mojom::WindowMode window_mode;
  if (!data.ReadWindowMode(&window_mode))
    return false;

  std::vector<apps::mojom::PermissionPtr> permissions;
  if (!data.ReadPermissions(&permissions))
    return false;

  apps::mojom::OptionalBool allow_uninstall;
  if (!data.ReadAllowUninstall(&allow_uninstall))
    return false;

  apps::mojom::OptionalBool handles_intents;
  if (!data.ReadHandlesIntents(&handles_intents))
    return false;

  auto app = apps::mojom::App::New();
  app->app_type = std::move(app_type);
  app->app_id = app_id;
  app->readiness = readiness;
  app->name = name;
  app->short_name = short_name;
  app->publisher_id = publisher_id;
  app->description = description;
  app->version = version;
  app->additional_search_terms = additional_search_terms;
  app->icon_key = std::move(icon_key);
  app->last_launch_time = last_launch_time;
  app->install_time = install_time;
  app->install_reason = install_reason;
  app->policy_id = policy_id;
  app->recommendable = recommendable;
  app->searchable = searchable;
  app->show_in_launcher = show_in_launcher;
  app->show_in_shelf = show_in_shelf;
  app->show_in_search = show_in_search;
  app->show_in_management = show_in_management;
  app->has_badge = has_badge;
  app->paused = paused;
  app->intent_filters = std::move(intent_filters);
  app->window_mode = window_mode;
  app->permissions = std::move(permissions);
  app->allow_uninstall = allow_uninstall;
  app->handles_intents = handles_intents;
  *out = std::move(app);
  return true;
}

crosapi::mojom::AppType
EnumTraits<crosapi::mojom::AppType, apps::mojom::AppType>::ToMojom(
    apps::mojom::AppType input) {
  switch (input) {
    case apps::mojom::AppType::kUnknown:
      return crosapi::mojom::AppType::kUnknown;
    case apps::mojom::AppType::kArc:
      return crosapi::mojom::AppType::kArc;
    case apps::mojom::AppType::kWeb:
      return crosapi::mojom::AppType::kWeb;
    case apps::mojom::AppType::kSystemWeb:
      return crosapi::mojom::AppType::kSystemWeb;
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return crosapi::mojom::AppType::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kChromeApp:
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kMacOs:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kStandaloneBrowser:
    case apps::mojom::AppType::kRemote:
    case apps::mojom::AppType::kBorealis:
      NOTREACHED();
      return crosapi::mojom::AppType::kUnknown;
  }
}

bool EnumTraits<crosapi::mojom::AppType, apps::mojom::AppType>::FromMojom(
    crosapi::mojom::AppType input,
    apps::mojom::AppType* output) {
  switch (input) {
    case crosapi::mojom::AppType::kUnknown:
      *output = apps::mojom::AppType::kUnknown;
      return true;
    case crosapi::mojom::AppType::kArc:
      *output = apps::mojom::AppType::kArc;
      return true;
    case crosapi::mojom::AppType::kWeb:
      *output = apps::mojom::AppType::kWeb;
      return true;
    case crosapi::mojom::AppType::kSystemWeb:
      *output = apps::mojom::AppType::kSystemWeb;
      return true;
    case crosapi::mojom::AppType::kStandaloneBrowserChromeApp:
      *output = apps::mojom::AppType::kStandaloneBrowserChromeApp;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::Readiness
EnumTraits<crosapi::mojom::Readiness, apps::mojom::Readiness>::ToMojom(
    apps::mojom::Readiness input) {
  switch (input) {
    case apps::mojom::Readiness::kUnknown:
      return crosapi::mojom::Readiness::kUnknown;
    case apps::mojom::Readiness::kReady:
      return crosapi::mojom::Readiness::kReady;
    case apps::mojom::Readiness::kDisabledByBlocklist:
      return crosapi::mojom::Readiness::kDisabledByBlocklist;
    case apps::mojom::Readiness::kDisabledByPolicy:
      return crosapi::mojom::Readiness::kDisabledByPolicy;
    case apps::mojom::Readiness::kDisabledByUser:
      return crosapi::mojom::Readiness::kDisabledByUser;
    case apps::mojom::Readiness::kTerminated:
      return crosapi::mojom::Readiness::kTerminated;
    case apps::mojom::Readiness::kUninstalledByUser:
      return crosapi::mojom::Readiness::kUninstalledByUser;
    case apps::mojom::Readiness::kRemoved:
      return crosapi::mojom::Readiness::kRemoved;
    case apps::mojom::Readiness::kUninstalledByMigration:
      return crosapi::mojom::Readiness::kUninstalledByMigration;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::Readiness, apps::mojom::Readiness>::FromMojom(
    crosapi::mojom::Readiness input,
    apps::mojom::Readiness* output) {
  switch (input) {
    case crosapi::mojom::Readiness::kUnknown:
      *output = apps::mojom::Readiness::kUnknown;
      return true;
    case crosapi::mojom::Readiness::kReady:
      *output = apps::mojom::Readiness::kReady;
      return true;
    case crosapi::mojom::Readiness::kDisabledByBlocklist:
      *output = apps::mojom::Readiness::kDisabledByBlocklist;
      return true;
    case crosapi::mojom::Readiness::kDisabledByPolicy:
      *output = apps::mojom::Readiness::kDisabledByPolicy;
      return true;
    case crosapi::mojom::Readiness::kDisabledByUser:
      *output = apps::mojom::Readiness::kDisabledByUser;
      return true;
    case crosapi::mojom::Readiness::kTerminated:
      *output = apps::mojom::Readiness::kTerminated;
      return true;
    case crosapi::mojom::Readiness::kUninstalledByUser:
      *output = apps::mojom::Readiness::kUninstalledByUser;
      return true;
    case crosapi::mojom::Readiness::kRemoved:
      *output = apps::mojom::Readiness::kRemoved;
      return true;
    case crosapi::mojom::Readiness::kUninstalledByMigration:
      *output = apps::mojom::Readiness::kUninstalledByMigration;
      return true;
  }

  NOTREACHED();
  return false;
}

bool StructTraits<crosapi::mojom::IconKeyDataView, apps::mojom::IconKeyPtr>::
    Read(crosapi::mojom::IconKeyDataView data, apps::mojom::IconKeyPtr* out) {
  auto icon_key = apps::mojom::IconKey::New();
  icon_key->timeline = data.timeline();
  icon_key->icon_effects = data.icon_effects();
  *out = std::move(icon_key);
  return true;
}

crosapi::mojom::InstallReason
EnumTraits<crosapi::mojom::InstallReason, apps::mojom::InstallReason>::ToMojom(
    apps::mojom::InstallReason input) {
  switch (input) {
    case apps::mojom::InstallReason::kUnknown:
      return crosapi::mojom::InstallReason::kUnknown;
    case apps::mojom::InstallReason::kSystem:
      return crosapi::mojom::InstallReason::kSystem;
    case apps::mojom::InstallReason::kPolicy:
      return crosapi::mojom::InstallReason::kPolicy;
    case apps::mojom::InstallReason::kSubApp:
      return crosapi::mojom::InstallReason::kSubApp;
    case apps::mojom::InstallReason::kOem:
      return crosapi::mojom::InstallReason::kOem;
    case apps::mojom::InstallReason::kDefault:
      return crosapi::mojom::InstallReason::kDefault;
    case apps::mojom::InstallReason::kSync:
      return crosapi::mojom::InstallReason::kSync;
    case apps::mojom::InstallReason::kUser:
      return crosapi::mojom::InstallReason::kUser;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::InstallReason, apps::mojom::InstallReason>::
    FromMojom(crosapi::mojom::InstallReason input,
              apps::mojom::InstallReason* output) {
  switch (input) {
    case crosapi::mojom::InstallReason::kUnknown:
      *output = apps::mojom::InstallReason::kUnknown;
      return true;
    case crosapi::mojom::InstallReason::kSystem:
      *output = apps::mojom::InstallReason::kSystem;
      return true;
    case crosapi::mojom::InstallReason::kPolicy:
      *output = apps::mojom::InstallReason::kPolicy;
      return true;
    case crosapi::mojom::InstallReason::kOem:
      *output = apps::mojom::InstallReason::kOem;
      return true;
    case crosapi::mojom::InstallReason::kDefault:
      *output = apps::mojom::InstallReason::kDefault;
      return true;
    case crosapi::mojom::InstallReason::kSync:
      *output = apps::mojom::InstallReason::kSync;
      return true;
    case crosapi::mojom::InstallReason::kUser:
      *output = apps::mojom::InstallReason::kUser;
      return true;
    case crosapi::mojom::InstallReason::kSubApp:
      *output = apps::mojom::InstallReason::kSubApp;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::OptionalBool
EnumTraits<crosapi::mojom::OptionalBool, apps::mojom::OptionalBool>::ToMojom(
    apps::mojom::OptionalBool input) {
  switch (input) {
    case apps::mojom::OptionalBool::kUnknown:
      return crosapi::mojom::OptionalBool::kUnknown;
    case apps::mojom::OptionalBool::kFalse:
      return crosapi::mojom::OptionalBool::kFalse;
    case apps::mojom::OptionalBool::kTrue:
      return crosapi::mojom::OptionalBool::kTrue;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::OptionalBool, apps::mojom::OptionalBool>::
    FromMojom(crosapi::mojom::OptionalBool input,
              apps::mojom::OptionalBool* output) {
  switch (input) {
    case crosapi::mojom::OptionalBool::kUnknown:
      *output = apps::mojom::OptionalBool::kUnknown;
      return true;
    case crosapi::mojom::OptionalBool::kFalse:
      *output = apps::mojom::OptionalBool::kFalse;
      return true;
    case crosapi::mojom::OptionalBool::kTrue:
      *output = apps::mojom::OptionalBool::kTrue;
      return true;
  }

  NOTREACHED();
  return false;
}

bool StructTraits<crosapi::mojom::IntentFilterDataView,
                  apps::mojom::IntentFilterPtr>::
    Read(crosapi::mojom::IntentFilterDataView data,
         apps::mojom::IntentFilterPtr* out) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  std::vector<apps::mojom::ConditionPtr> conditions;
  if (!data.ReadConditions(&conditions))
    return false;
  absl::optional<std::string> activity_name;
  if (!data.ReadActivityName(&activity_name))
    return false;
  absl::optional<std::string> activity_label;
  if (!data.ReadActivityLabel(&activity_label))
    return false;

  intent_filter->conditions = std::move(conditions);
  intent_filter->activity_name = activity_name;
  intent_filter->activity_label = activity_label;
  *out = std::move(intent_filter);
  return true;
}

bool StructTraits<
    crosapi::mojom::ConditionDataView,
    apps::mojom::ConditionPtr>::Read(crosapi::mojom::ConditionDataView data,
                                     apps::mojom::ConditionPtr* out) {
  auto condition = apps::mojom::Condition::New();

  apps::mojom::ConditionType condition_type;
  if (!data.ReadConditionType(&condition_type))
    return false;
  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  if (!data.ReadConditionValues(&condition_values))
    return false;

  condition->condition_type = condition_type;
  condition->condition_values = std::move(condition_values);
  *out = std::move(condition);
  return true;
}

crosapi::mojom::ConditionType
EnumTraits<crosapi::mojom::ConditionType, apps::mojom::ConditionType>::ToMojom(
    apps::mojom::ConditionType input) {
  switch (input) {
    case apps::mojom::ConditionType::kScheme:
      return crosapi::mojom::ConditionType::kScheme;
    case apps::mojom::ConditionType::kHost:
      return crosapi::mojom::ConditionType::kHost;
    case apps::mojom::ConditionType::kPattern:
      return crosapi::mojom::ConditionType::kPattern;
    case apps::mojom::ConditionType::kAction:
      return crosapi::mojom::ConditionType::kAction;
    case apps::mojom::ConditionType::kMimeType:
      return crosapi::mojom::ConditionType::kMimeType;
    case apps::mojom::ConditionType::kFile:
      return crosapi::mojom::ConditionType::kFile;
  }

  NOTREACHED();
}

bool StructTraits<crosapi::mojom::ConditionValueDataView,
                  apps::mojom::ConditionValuePtr>::
    Read(crosapi::mojom::ConditionValueDataView data,
         apps::mojom::ConditionValuePtr* out) {
  auto condition_value = apps::mojom::ConditionValue::New();

  std::string value;
  if (!data.ReadValue(&value))
    return false;
  apps::mojom::PatternMatchType match_type;
  if (!data.ReadMatchType(&match_type))
    return false;

  condition_value->value = value;
  condition_value->match_type = match_type;
  *out = std::move(condition_value);
  return true;
}

bool EnumTraits<crosapi::mojom::ConditionType, apps::mojom::ConditionType>::
    FromMojom(crosapi::mojom::ConditionType input,
              apps::mojom::ConditionType* output) {
  switch (input) {
    case crosapi::mojom::ConditionType::kScheme:
      *output = apps::mojom::ConditionType::kScheme;
      return true;
    case crosapi::mojom::ConditionType::kHost:
      *output = apps::mojom::ConditionType::kHost;
      return true;
    case crosapi::mojom::ConditionType::kPattern:
      *output = apps::mojom::ConditionType::kPattern;
      return true;
    case crosapi::mojom::ConditionType::kAction:
      *output = apps::mojom::ConditionType::kAction;
      return true;
    case crosapi::mojom::ConditionType::kMimeType:
      *output = apps::mojom::ConditionType::kMimeType;
      return true;
    case crosapi::mojom::ConditionType::kFileExtension:
    case crosapi::mojom::ConditionType::kFile:
      *output = apps::mojom::ConditionType::kFile;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::PatternMatchType
EnumTraits<crosapi::mojom::PatternMatchType, apps::mojom::PatternMatchType>::
    ToMojom(apps::mojom::PatternMatchType input) {
  switch (input) {
    case apps::mojom::PatternMatchType::kNone:
      return crosapi::mojom::PatternMatchType::kNone;
    case apps::mojom::PatternMatchType::kLiteral:
      return crosapi::mojom::PatternMatchType::kLiteral;
    case apps::mojom::PatternMatchType::kPrefix:
      return crosapi::mojom::PatternMatchType::kPrefix;
    case apps::mojom::PatternMatchType::kGlob:
      return crosapi::mojom::PatternMatchType::kGlob;
    case apps::mojom::PatternMatchType::kMimeType:
      return crosapi::mojom::PatternMatchType::kMimeType;
    case apps::mojom::PatternMatchType::kFileExtension:
      return crosapi::mojom::PatternMatchType::kFileExtension;
    case apps::mojom::PatternMatchType::kIsDirectory:
      return crosapi::mojom::PatternMatchType::kIsDirectory;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::PatternMatchType,
                apps::mojom::PatternMatchType>::
    FromMojom(crosapi::mojom::PatternMatchType input,
              apps::mojom::PatternMatchType* output) {
  switch (input) {
    case crosapi::mojom::PatternMatchType::kNone:
      *output = apps::mojom::PatternMatchType::kNone;
      return true;
    case crosapi::mojom::PatternMatchType::kLiteral:
      *output = apps::mojom::PatternMatchType::kLiteral;
      return true;
    case crosapi::mojom::PatternMatchType::kPrefix:
      *output = apps::mojom::PatternMatchType::kPrefix;
      return true;
    case crosapi::mojom::PatternMatchType::kGlob:
      *output = apps::mojom::PatternMatchType::kGlob;
      return true;
    case crosapi::mojom::PatternMatchType::kMimeType:
      *output = apps::mojom::PatternMatchType::kMimeType;
      return true;
    case crosapi::mojom::PatternMatchType::kFileExtension:
      *output = apps::mojom::PatternMatchType::kFileExtension;
      return true;
    case crosapi::mojom::PatternMatchType::kIsDirectory:
      *output = apps::mojom::PatternMatchType::kIsDirectory;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::UninstallSource EnumTraits<
    crosapi::mojom::UninstallSource,
    apps::mojom::UninstallSource>::ToMojom(apps::mojom::UninstallSource input) {
  switch (input) {
    case apps::mojom::UninstallSource::kUnknown:
      return crosapi::mojom::UninstallSource::kUnknown;
    case apps::mojom::UninstallSource::kAppList:
      return crosapi::mojom::UninstallSource::kAppList;
    case apps::mojom::UninstallSource::kAppManagement:
      return crosapi::mojom::UninstallSource::kAppManagement;
    case apps::mojom::UninstallSource::kShelf:
      return crosapi::mojom::UninstallSource::kShelf;
    case apps::mojom::UninstallSource::kMigration:
      return crosapi::mojom::UninstallSource::kMigration;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::UninstallSource, apps::mojom::UninstallSource>::
    FromMojom(crosapi::mojom::UninstallSource input,
              apps::mojom::UninstallSource* output) {
  switch (input) {
    case crosapi::mojom::UninstallSource::kUnknown:
      *output = apps::mojom::UninstallSource::kUnknown;
      return true;
    case crosapi::mojom::UninstallSource::kAppList:
      *output = apps::mojom::UninstallSource::kAppList;
      return true;
    case crosapi::mojom::UninstallSource::kAppManagement:
      *output = apps::mojom::UninstallSource::kAppManagement;
      return true;
    case crosapi::mojom::UninstallSource::kShelf:
      *output = apps::mojom::UninstallSource::kShelf;
      return true;
    case crosapi::mojom::UninstallSource::kMigration:
      *output = apps::mojom::UninstallSource::kMigration;
      return true;
  }

  NOTREACHED();
  return false;
}

bool StructTraits<crosapi::mojom::CapabilityAccessDataView,
                  apps::mojom::CapabilityAccessPtr>::
    Read(crosapi::mojom::CapabilityAccessDataView data,
         apps::mojom::CapabilityAccessPtr* out) {
  std::string app_id;
  if (!data.ReadAppId(&app_id))
    return false;

  apps::mojom::OptionalBool camera;
  if (!data.ReadCamera(&camera))
    return false;

  apps::mojom::OptionalBool microphone;
  if (!data.ReadMicrophone(&microphone))
    return false;

  auto capability_access = apps::mojom::CapabilityAccess::New();
  capability_access->app_id = std::move(app_id);
  capability_access->camera = std::move(camera);
  capability_access->microphone = std::move(microphone);
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

  NOTREACHED();
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

  NOTREACHED();
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
  *out = std::move(icon_value);
  return true;
}

crosapi::mojom::WindowMode
EnumTraits<crosapi::mojom::WindowMode, apps::mojom::WindowMode>::ToMojom(
    apps::mojom::WindowMode input) {
  switch (input) {
    case apps::mojom::WindowMode::kUnknown:
      return crosapi::mojom::WindowMode::kUnknown;
    case apps::mojom::WindowMode::kWindow:
      return crosapi::mojom::WindowMode::kWindow;
    case apps::mojom::WindowMode::kBrowser:
      return crosapi::mojom::WindowMode::kBrowser;
    case apps::mojom::WindowMode::kTabbedWindow:
      return crosapi::mojom::WindowMode::kTabbedWindow;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::WindowMode, apps::mojom::WindowMode>::FromMojom(
    crosapi::mojom::WindowMode input,
    apps::mojom::WindowMode* output) {
  switch (input) {
    case crosapi::mojom::WindowMode::kUnknown:
      *output = apps::mojom::WindowMode::kUnknown;
      return true;
    case crosapi::mojom::WindowMode::kWindow:
      *output = apps::mojom::WindowMode::kWindow;
      return true;
    case crosapi::mojom::WindowMode::kBrowser:
      *output = apps::mojom::WindowMode::kBrowser;
      return true;
    case crosapi::mojom::WindowMode::kTabbedWindow:
      *output = apps::mojom::WindowMode::kTabbedWindow;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::LaunchSource
EnumTraits<crosapi::mojom::LaunchSource, apps::mojom::LaunchSource>::ToMojom(
    apps::mojom::LaunchSource input) {
  switch (input) {
    case apps::mojom::LaunchSource::kUnknown:
      return crosapi::mojom::LaunchSource::kUnknown;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      return crosapi::mojom::LaunchSource::kFromAppListGrid;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      return crosapi::mojom::LaunchSource::kFromAppListGridContextMenu;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      return crosapi::mojom::LaunchSource::kFromAppListQuery;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      return crosapi::mojom::LaunchSource::kFromAppListQueryContextMenu;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      return crosapi::mojom::LaunchSource::kFromAppListRecommendation;
    case apps::mojom::LaunchSource::kFromParentalControls:
      return crosapi::mojom::LaunchSource::kFromParentalControls;
    case apps::mojom::LaunchSource::kFromShelf:
      return crosapi::mojom::LaunchSource::kFromShelf;
    case apps::mojom::LaunchSource::kFromFileManager:
      return crosapi::mojom::LaunchSource::kFromFileManager;
    case apps::mojom::LaunchSource::kFromLink:
      return crosapi::mojom::LaunchSource::kFromLink;
    case apps::mojom::LaunchSource::kFromOmnibox:
      return crosapi::mojom::LaunchSource::kFromOmnibox;
    case apps::mojom::LaunchSource::kFromChromeInternal:
      return crosapi::mojom::LaunchSource::kFromChromeInternal;
    case apps::mojom::LaunchSource::kFromKeyboard:
      return crosapi::mojom::LaunchSource::kFromKeyboard;
    case apps::mojom::LaunchSource::kFromOtherApp:
      return crosapi::mojom::LaunchSource::kFromOtherApp;
    case apps::mojom::LaunchSource::kFromMenu:
      return crosapi::mojom::LaunchSource::kFromMenu;
    case apps::mojom::LaunchSource::kFromInstalledNotification:
      return crosapi::mojom::LaunchSource::kFromInstalledNotification;
    case apps::mojom::LaunchSource::kFromTest:
      return crosapi::mojom::LaunchSource::kFromTest;
    case apps::mojom::LaunchSource::kFromArc:
      return crosapi::mojom::LaunchSource::kFromArc;
    case apps::mojom::LaunchSource::kFromSharesheet:
      return crosapi::mojom::LaunchSource::kFromSharesheet;
    case apps::mojom::LaunchSource::kFromReleaseNotesNotification:
      return crosapi::mojom::LaunchSource::kFromReleaseNotesNotification;
    case apps::mojom::LaunchSource::kFromFullRestore:
      return crosapi::mojom::LaunchSource::kFromFullRestore;
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      return crosapi::mojom::LaunchSource::kFromSmartTextContextMenu;
    case apps::mojom::LaunchSource::kFromDiscoverTabNotification:
      return crosapi::mojom::LaunchSource::kFromDiscoverTabNotification;
    case apps::mojom::LaunchSource::kFromManagementApi:
      return crosapi::mojom::LaunchSource::kFromManagementApi;
    case apps::mojom::LaunchSource::kFromKiosk:
      return crosapi::mojom::LaunchSource::kFromKiosk;
    case apps::mojom::LaunchSource::kFromNewTabPage:
      return crosapi::mojom::LaunchSource::kFromNewTabPage;
    case apps::mojom::LaunchSource::kFromIntentUrl:
      return crosapi::mojom::LaunchSource::kFromIntentUrl;
    case apps::mojom::LaunchSource::kFromOsLogin:
      return crosapi::mojom::LaunchSource::kFromOsLogin;
    case apps::mojom::LaunchSource::kFromProtocolHandler:
      return crosapi::mojom::LaunchSource::kFromProtocolHandler;
    case apps::mojom::LaunchSource::kFromUrlHandler:
      return crosapi::mojom::LaunchSource::kFromUrlHandler;
    case apps::mojom::LaunchSource::kFromCommandLine:
    case apps::mojom::LaunchSource::kFromBackgroundMode:
      NOTREACHED();
      return crosapi::mojom::LaunchSource::kUnknown;
  }
  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::LaunchSource, apps::mojom::LaunchSource>::
    FromMojom(crosapi::mojom::LaunchSource input,
              apps::mojom::LaunchSource* output) {
  switch (input) {
    case crosapi::mojom::LaunchSource::kUnknown:
      *output = apps::mojom::LaunchSource::kUnknown;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListGrid:
      *output = apps::mojom::LaunchSource::kFromAppListGrid;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListGridContextMenu:
      *output = apps::mojom::LaunchSource::kFromAppListGridContextMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListQuery:
      *output = apps::mojom::LaunchSource::kFromAppListQuery;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListQueryContextMenu:
      *output = apps::mojom::LaunchSource::kFromAppListQueryContextMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromAppListRecommendation:
      *output = apps::mojom::LaunchSource::kFromAppListRecommendation;
      return true;
    case crosapi::mojom::LaunchSource::kFromParentalControls:
      *output = apps::mojom::LaunchSource::kFromParentalControls;
      return true;
    case crosapi::mojom::LaunchSource::kFromShelf:
      *output = apps::mojom::LaunchSource::kFromShelf;
      return true;
    case crosapi::mojom::LaunchSource::kFromFileManager:
      *output = apps::mojom::LaunchSource::kFromFileManager;
      return true;
    case crosapi::mojom::LaunchSource::kFromLink:
      *output = apps::mojom::LaunchSource::kFromLink;
      return true;
    case crosapi::mojom::LaunchSource::kFromOmnibox:
      *output = apps::mojom::LaunchSource::kFromOmnibox;
      return true;
    case crosapi::mojom::LaunchSource::kFromChromeInternal:
      *output = apps::mojom::LaunchSource::kFromChromeInternal;
      return true;
    case crosapi::mojom::LaunchSource::kFromKeyboard:
      *output = apps::mojom::LaunchSource::kFromKeyboard;
      return true;
    case crosapi::mojom::LaunchSource::kFromOtherApp:
      *output = apps::mojom::LaunchSource::kFromOtherApp;
      return true;
    case crosapi::mojom::LaunchSource::kFromMenu:
      *output = apps::mojom::LaunchSource::kFromMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromInstalledNotification:
      *output = apps::mojom::LaunchSource::kFromInstalledNotification;
      return true;
    case crosapi::mojom::LaunchSource::kFromTest:
      *output = apps::mojom::LaunchSource::kFromTest;
      return true;
    case crosapi::mojom::LaunchSource::kFromArc:
      *output = apps::mojom::LaunchSource::kFromArc;
      return true;
    case crosapi::mojom::LaunchSource::kFromSharesheet:
      *output = apps::mojom::LaunchSource::kFromSharesheet;
      return true;
    case crosapi::mojom::LaunchSource::kFromReleaseNotesNotification:
      *output = apps::mojom::LaunchSource::kFromReleaseNotesNotification;
      return true;
    case crosapi::mojom::LaunchSource::kFromFullRestore:
      *output = apps::mojom::LaunchSource::kFromFullRestore;
      return true;
    case crosapi::mojom::LaunchSource::kFromSmartTextContextMenu:
      *output = apps::mojom::LaunchSource::kFromSmartTextContextMenu;
      return true;
    case crosapi::mojom::LaunchSource::kFromDiscoverTabNotification:
      *output = apps::mojom::LaunchSource::kFromDiscoverTabNotification;
      return true;
    case crosapi::mojom::LaunchSource::kFromManagementApi:
      *output = apps::mojom::LaunchSource::kFromManagementApi;
      return true;
    case crosapi::mojom::LaunchSource::kFromKiosk:
      *output = apps::mojom::LaunchSource::kFromKiosk;
      return true;
    case crosapi::mojom::LaunchSource::kFromNewTabPage:
      *output = apps::mojom::LaunchSource::kFromNewTabPage;
      return true;
    case crosapi::mojom::LaunchSource::kFromIntentUrl:
      *output = apps::mojom::LaunchSource::kFromIntentUrl;
      return true;
    case crosapi::mojom::LaunchSource::kFromOsLogin:
      *output = apps::mojom::LaunchSource::kFromOsLogin;
      return true;
    case crosapi::mojom::LaunchSource::kFromProtocolHandler:
      *output = apps::mojom::LaunchSource::kFromProtocolHandler;
      return true;
    case crosapi::mojom::LaunchSource::kFromUrlHandler:
      *output = apps::mojom::LaunchSource::kFromUrlHandler;
      return true;
  }

  NOTREACHED();
  return false;
}

bool StructTraits<
    crosapi::mojom::PermissionDataView,
    apps::mojom::PermissionPtr>::Read(crosapi::mojom::PermissionDataView data,
                                      apps::mojom::PermissionPtr* out) {
  apps::mojom::PermissionType permission_type;
  if (!data.ReadPermissionType(&permission_type))
    return false;

  apps::mojom::PermissionValuePtr value;
  if (!data.ReadValue(&value))
    return false;

  auto permission = apps::mojom::Permission::New();
  permission->permission_type = permission_type;
  permission->value = std::move(value);
  permission->is_managed = data.is_managed();
  *out = std::move(permission);
  return true;
}

crosapi::mojom::PermissionType EnumTraits<
    crosapi::mojom::PermissionType,
    apps::mojom::PermissionType>::ToMojom(apps::mojom::PermissionType input) {
  switch (input) {
    case apps::mojom::PermissionType::kUnknown:
      return crosapi::mojom::PermissionType::kUnknown;
    case apps::mojom::PermissionType::kCamera:
      return crosapi::mojom::PermissionType::kCamera;
    case apps::mojom::PermissionType::kLocation:
      return crosapi::mojom::PermissionType::kLocation;
    case apps::mojom::PermissionType::kMicrophone:
      return crosapi::mojom::PermissionType::kMicrophone;
    case apps::mojom::PermissionType::kNotifications:
      return crosapi::mojom::PermissionType::kNotifications;
    case apps::mojom::PermissionType::kContacts:
      return crosapi::mojom::PermissionType::kContacts;
    case apps::mojom::PermissionType::kStorage:
      return crosapi::mojom::PermissionType::kStorage;
    case apps::mojom::PermissionType::kPrinting:
      NOTREACHED();
      return crosapi::mojom::PermissionType::kUnknown;
  }
}

bool EnumTraits<crosapi::mojom::PermissionType, apps::mojom::PermissionType>::
    FromMojom(crosapi::mojom::PermissionType input,
              apps::mojom::PermissionType* output) {
  switch (input) {
    case crosapi::mojom::PermissionType::kUnknown:
      *output = apps::mojom::PermissionType::kUnknown;
      return true;
    case crosapi::mojom::PermissionType::kCamera:
      *output = apps::mojom::PermissionType::kCamera;
      return true;
    case crosapi::mojom::PermissionType::kLocation:
      *output = apps::mojom::PermissionType::kLocation;
      return true;
    case crosapi::mojom::PermissionType::kMicrophone:
      *output = apps::mojom::PermissionType::kMicrophone;
      return true;
    case crosapi::mojom::PermissionType::kNotifications:
      *output = apps::mojom::PermissionType::kNotifications;
      return true;
    case crosapi::mojom::PermissionType::kContacts:
      *output = apps::mojom::PermissionType::kContacts;
      return true;
    case crosapi::mojom::PermissionType::kStorage:
      *output = apps::mojom::PermissionType::kStorage;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::TriState
EnumTraits<crosapi::mojom::TriState, apps::mojom::TriState>::ToMojom(
    apps::mojom::TriState input) {
  switch (input) {
    case apps::mojom::TriState::kAllow:
      return crosapi::mojom::TriState::kAllow;
    case apps::mojom::TriState::kBlock:
      return crosapi::mojom::TriState::kBlock;
    case apps::mojom::TriState::kAsk:
      return crosapi::mojom::TriState::kAsk;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::TriState, apps::mojom::TriState>::FromMojom(
    crosapi::mojom::TriState input,
    apps::mojom::TriState* output) {
  switch (input) {
    case crosapi::mojom::TriState::kAllow:
      *output = apps::mojom::TriState::kAllow;
      return true;
    case crosapi::mojom::TriState::kBlock:
      *output = apps::mojom::TriState::kBlock;
      return true;
    case crosapi::mojom::TriState::kAsk:
      *output = apps::mojom::TriState::kAsk;
      return true;
  }

  NOTREACHED();
  return false;
}

crosapi::mojom::PermissionValueDataView::Tag
UnionTraits<crosapi::mojom::PermissionValueDataView,
            apps::mojom::PermissionValuePtr>::
    GetTag(const apps::mojom::PermissionValuePtr& r) {
  if (r->is_bool_value()) {
    return crosapi::mojom::PermissionValueDataView::Tag::BOOL_VALUE;
  } else if (r->is_tristate_value()) {
    return crosapi::mojom::PermissionValueDataView::Tag::TRISTATE_VALUE;
  }
  NOTREACHED();
  return crosapi::mojom::PermissionValueDataView::Tag::BOOL_VALUE;
}

bool UnionTraits<crosapi::mojom::PermissionValueDataView,
                 apps::mojom::PermissionValuePtr>::
    Read(crosapi::mojom::PermissionValueDataView data,
         apps::mojom::PermissionValuePtr* out) {
  *out = apps::mojom::PermissionValue::New();
  switch (data.tag()) {
    case crosapi::mojom::PermissionValueDataView::Tag::BOOL_VALUE: {
      (*out)->set_bool_value(data.bool_value());
      return true;
    }
    case crosapi::mojom::PermissionValueDataView::Tag::TRISTATE_VALUE: {
      apps::mojom::TriState tristate_value;
      if (!data.ReadTristateValue(&tristate_value))
        return false;
      (*out)->set_tristate_value(tristate_value);
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool StructTraits<crosapi::mojom::PreferredAppDataView,
                  apps::mojom::PreferredAppPtr>::
    Read(crosapi::mojom::PreferredAppDataView data,
         apps::mojom::PreferredAppPtr* out) {
  apps::mojom::IntentFilterPtr intent_filter;
  if (!data.ReadIntentFilter(&intent_filter))
    return false;

  std::string app_id;
  if (!data.ReadAppId(&app_id))
    return false;

  auto preferred_app = apps::mojom::PreferredApp::New();
  preferred_app->intent_filter = std::move(intent_filter);
  preferred_app->app_id = app_id;
  *out = std::move(preferred_app);
  return true;
}

bool StructTraits<crosapi::mojom::PreferredAppChangesDataView,
                  apps::mojom::PreferredAppChangesPtr>::
    Read(crosapi::mojom::PreferredAppChangesDataView data,
         apps::mojom::PreferredAppChangesPtr* out) {
  base::flat_map<std::string, std::vector<apps::mojom::IntentFilterPtr>>
      added_filters;
  if (!data.ReadAddedFilters(&added_filters))
    return false;

  base::flat_map<std::string, std::vector<apps::mojom::IntentFilterPtr>>
      removed_filters;
  if (!data.ReadRemovedFilters(&removed_filters))
    return false;

  auto preferred_app_changes = apps::mojom::PreferredAppChanges::New();
  preferred_app_changes->added_filters = std::move(added_filters);
  preferred_app_changes->removed_filters = std::move(removed_filters);
  *out = std::move(preferred_app_changes);
  return true;
}

}  // namespace mojo
