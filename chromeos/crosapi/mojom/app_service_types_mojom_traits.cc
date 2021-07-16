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

  apps::mojom::InstallSource install_source;
  if (!data.ReadInstallSource(&install_source))
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
  app->install_source = install_source;
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
    case apps::mojom::AppType::kStandaloneBrowserExtension:
      return crosapi::mojom::AppType::kStandaloneBrowserExtension;
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kCrostini:
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
    case crosapi::mojom::AppType::kStandaloneBrowserExtension:
      *output = apps::mojom::AppType::kStandaloneBrowserExtension;
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

crosapi::mojom::InstallSource
EnumTraits<crosapi::mojom::InstallSource, apps::mojom::InstallSource>::ToMojom(
    apps::mojom::InstallSource input) {
  switch (input) {
    case apps::mojom::InstallSource::kUnknown:
      return crosapi::mojom::InstallSource::kUnknown;
    case apps::mojom::InstallSource::kSystem:
      return crosapi::mojom::InstallSource::kSystem;
    case apps::mojom::InstallSource::kPolicy:
      return crosapi::mojom::InstallSource::kPolicy;
    case apps::mojom::InstallSource::kOem:
      return crosapi::mojom::InstallSource::kOem;
    case apps::mojom::InstallSource::kDefault:
      return crosapi::mojom::InstallSource::kDefault;
    case apps::mojom::InstallSource::kSync:
      return crosapi::mojom::InstallSource::kSync;
    case apps::mojom::InstallSource::kUser:
      return crosapi::mojom::InstallSource::kUser;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::InstallSource, apps::mojom::InstallSource>::
    FromMojom(crosapi::mojom::InstallSource input,
              apps::mojom::InstallSource* output) {
  switch (input) {
    case crosapi::mojom::InstallSource::kUnknown:
      *output = apps::mojom::InstallSource::kUnknown;
      return true;
    case crosapi::mojom::InstallSource::kSystem:
      *output = apps::mojom::InstallSource::kSystem;
      return true;
    case crosapi::mojom::InstallSource::kPolicy:
      *output = apps::mojom::InstallSource::kPolicy;
      return true;
    case crosapi::mojom::InstallSource::kOem:
      *output = apps::mojom::InstallSource::kOem;
      return true;
    case crosapi::mojom::InstallSource::kDefault:
      *output = apps::mojom::InstallSource::kDefault;
      return true;
    case crosapi::mojom::InstallSource::kSync:
      *output = apps::mojom::InstallSource::kSync;
      return true;
    case crosapi::mojom::InstallSource::kUser:
      *output = apps::mojom::InstallSource::kUser;
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
EnumTraits<crosapi::mojom::IconType, apps::mojom::IconType>::ToMojom(
    apps::mojom::IconType input) {
  switch (input) {
    case apps::mojom::IconType::kUnknown:
      return crosapi::mojom::IconType::kUnknown;
    case apps::mojom::IconType::kUncompressed:
      return crosapi::mojom::IconType::kUncompressed;
    case apps::mojom::IconType::kCompressed:
      return crosapi::mojom::IconType::kCompressed;
    case apps::mojom::IconType::kStandard:
      return crosapi::mojom::IconType::kStandard;
  }

  NOTREACHED();
}

bool EnumTraits<crosapi::mojom::IconType, apps::mojom::IconType>::FromMojom(
    crosapi::mojom::IconType input,
    apps::mojom::IconType* output) {
  switch (input) {
    case crosapi::mojom::IconType::kUnknown:
      *output = apps::mojom::IconType::kUnknown;
      return true;
    case crosapi::mojom::IconType::kUncompressed:
      *output = apps::mojom::IconType::kUncompressed;
      return true;
    case crosapi::mojom::IconType::kCompressed:
      *output = apps::mojom::IconType::kCompressed;
      return true;
    case crosapi::mojom::IconType::kStandard:
      *output = apps::mojom::IconType::kStandard;
      return true;
  }

  NOTREACHED();
  return false;
}

bool StructTraits<
    crosapi::mojom::IconValueDataView,
    apps::mojom::IconValuePtr>::Read(crosapi::mojom::IconValueDataView data,
                                     apps::mojom::IconValuePtr* out) {
  apps::mojom::IconType icon_type;
  if (!data.ReadIconType(&icon_type))
    return false;

  gfx::ImageSkia uncompressed;
  if (!data.ReadUncompressed(&uncompressed))
    return false;

  absl::optional<std::vector<uint8_t>> compressed;
  if (!data.ReadCompressed(&compressed))
    return false;

  auto icon_value = apps::mojom::IconValue::New();
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

}  // namespace mojo
