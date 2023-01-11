// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"

#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

namespace {

// New fields must be added to BuildIndexJson().
constexpr char kInstalledWebApps[] = "InstalledWebApps";
constexpr char kPreinstalledWebAppConfigs[] = "PreinstalledWebAppConfigs";
constexpr char kUserUninstalledPreinstalledWebAppPrefs[] =
    "UserUninstalledPreinstalledWebAppPrefs";
constexpr char kExternallyManagedWebAppPrefs[] = "ExternallyManagedWebAppPrefs";
constexpr char kCommandManager[] = "CommandManager";
constexpr char kIconErrorLog[] = "IconErrorLog";
constexpr char kInstallationProcessErrorLog[] = "InstallationProcessErrorLog";
#if BUILDFLAG(IS_MAC)
constexpr char kAppShimRegistryLocalStorage[] = "AppShimRegistryLocalStorage";
#endif
constexpr char kWebAppDirectoryDiskState[] = "WebAppDirectoryDiskState";

constexpr char kNeedsRecordWebAppDebugInfo[] =
    "No debugging info available! Please enable: "
    "chrome://flags/#record-web-app-debug-info";

template <typename T>
std::string ConvertToString(const T& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

base::Value::Dict BuildIndexJson() {
  base::Value::Dict root;
  base::Value::List& index = *root.EnsureList("Index");

  index.Append(kInstalledWebApps);
  index.Append(kPreinstalledWebAppConfigs);
  index.Append(kUserUninstalledPreinstalledWebAppPrefs);
  index.Append(kExternallyManagedWebAppPrefs);
  index.Append(kCommandManager);
  index.Append(kIconErrorLog);
  index.Append(kInstallationProcessErrorLog);
#if BUILDFLAG(IS_MAC)
  index.Append(kAppShimRegistryLocalStorage);
#endif
  index.Append(kWebAppDirectoryDiskState);

  return root;
}

base::Value::Dict BuildInstalledWebAppsJson(web_app::WebAppProvider& provider) {
  base::Value::Dict root;

  base::Value::Dict& installed_web_apps = *root.EnsureDict(kInstalledWebApps);

  std::vector<const web_app::WebApp*> web_apps;
  for (const web_app::WebApp& web_app :
       provider.registrar_unsafe().GetAppsIncludingStubs()) {
    web_apps.push_back(&web_app);
  }
  base::ranges::sort(web_apps, {}, &web_app::WebApp::untranslated_name);

  // Prefix with a ! so this appears at the top when serialized.
  base::Value::Dict& index = *installed_web_apps.EnsureDict("!Index");
  for (const web_app::WebApp* web_app : web_apps) {
    const std::string& key = web_app->untranslated_name();
    base::Value* existing_entry = index.Find(key);
    if (!existing_entry) {
      index.Set(key, web_app->app_id());
      continue;
    }
    // If any web apps share identical names then collect a list of app IDs.
    const std::string* existing_id = existing_entry->GetIfString();
    if (existing_id) {
      base::Value::List id_list;
      id_list.Append(*existing_id);
      index.Set(key, std::move(id_list));
    }
    index.FindList(key)->Append(web_app->app_id());
  }

  base::Value::List& web_app_details =
      *installed_web_apps.EnsureList("Details");
  for (const web_app::WebApp* web_app : web_apps) {
    web_app_details.Append(web_app->AsDebugValue());
  }

  return root;
}

base::Value::Dict BuildPreinstalledWebAppConfigsJson(
    web_app::WebAppProvider& provider) {
  base::Value::Dict root;

  const web_app::PreinstalledWebAppManager::DebugInfo* debug_info =
      provider.preinstalled_web_app_manager().debug_info();
  if (!debug_info) {
    root.Set(kPreinstalledWebAppConfigs, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  base::Value::Dict& preinstalled_web_app_configs =
      *root.EnsureDict(kPreinstalledWebAppConfigs);

  base::Value::List& config_parse_errors =
      *preinstalled_web_app_configs.EnsureList("ConfigParseErrors");
  for (const std::string& parse_error : debug_info->parse_errors) {
    config_parse_errors.Append(parse_error);
  }

  base::Value::List& configs_enabled =
      *preinstalled_web_app_configs.EnsureList("ConfigsEnabled");
  for (const web_app::ExternalInstallOptions& enabled_config :
       debug_info->enabled_configs) {
    configs_enabled.Append(enabled_config.AsDebugValue());
  }

  base::Value::List& configs_disabled =
      *preinstalled_web_app_configs.EnsureList("ConfigsDisabled");
  for (const std::pair<web_app::ExternalInstallOptions, std::string>&
           disabled_config : debug_info->disabled_configs) {
    base::Value::Dict entry;
    entry.Set("!Reason", disabled_config.second);
    entry.Set("Config", disabled_config.first.AsDebugValue());
    configs_disabled.Append(std::move(entry));
  }

  base::Value::List& install_results =
      *preinstalled_web_app_configs.EnsureList("InstallResults");
  for (std::pair<const GURL&,
                 const web_app::ExternallyManagedAppManager::InstallResult&>
           install_result : debug_info->install_results) {
    base::Value::Dict entry;
    entry.Set("InstallUrl", install_result.first.spec());
    entry.Set("ResultCode", ConvertToString(install_result.second.code));
    entry.Set("DidUninstallAndReplace",
              install_result.second.did_uninstall_and_replace);
    install_results.Append(std::move(entry));
  }

  preinstalled_web_app_configs.Set("IsStartUpTaskComplete",
                                   debug_info->is_start_up_task_complete);

  base::Value::List& uninstall_results =
      *preinstalled_web_app_configs.EnsureList("UninstallResults");
  for (std::pair<const GURL&, const bool&> uninstall_result :
       debug_info->uninstall_results) {
    base::Value::Dict entry;
    entry.Set("InstallUrl", uninstall_result.first.spec());
    entry.Set("Success", uninstall_result.second);
    uninstall_results.Append(std::move(entry));
  }

  return root;
}

base::Value::Dict BuildExternallyManagedWebAppPrefsJson(Profile* profile) {
  base::Value::Dict root;
  root.Set(kExternallyManagedWebAppPrefs,
           profile->GetPrefs()->GetDict(prefs::kWebAppsExtensionIDs).Clone());
  return root;
}

base::Value::Dict BuildUserUninstalledPreinstalledWebAppPrefsJson(
    Profile* profile) {
  base::Value::Dict root;
  root.Set(kUserUninstalledPreinstalledWebAppPrefs,
           profile->GetPrefs()
               ->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref)
               .Clone());
  return root;
}

base::Value::Dict BuildCommandManagerJson(web_app::WebAppProvider& provider) {
  base::Value::Dict root;
  root.Set(kCommandManager, provider.command_manager().ToDebugValue());
  return root;
}

base::Value::Dict BuildIconErrorLogJson(web_app::WebAppProvider& provider) {
  base::Value::Dict root;

  const std::vector<std::string>* error_log =
      provider.icon_manager().error_log();

  if (!error_log) {
    root.Set(kIconErrorLog, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  base::Value::List& icon_error_log = *root.EnsureList(kIconErrorLog);
  for (const std::string& error : *error_log) {
    icon_error_log.Append(error);
  }

  return root;
}

base::Value::Dict BuildInstallProcessErrorLogJson(
    web_app::WebAppProvider& provider) {
  base::Value::Dict root;

  const web_app::WebAppInstallManager::ErrorLog* error_log =
      provider.install_manager().error_log();

  if (!error_log) {
    root.Set(kInstallationProcessErrorLog, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  base::Value::List& installation_process_error_log =
      *root.EnsureList(kInstallationProcessErrorLog);
  for (const base::Value& error : *error_log) {
    installation_process_error_log.Append(error.Clone());
  }

  return root;
}

#if BUILDFLAG(IS_MAC)
base::Value::Dict BuildAppShimRegistryLocalStorageJson() {
  base::Value::Dict root;
  root.Set(kAppShimRegistryLocalStorage,
           AppShimRegistry::Get()->AsDebugDict().Clone());
  return root;
}
#endif

void BuildDirectoryState(base::FilePath file_or_folder,
                         base::Value::Dict* folder) {
  base::File::Info info;
  bool success = base::GetFileInfo(file_or_folder, &info);
  if (!success) {
    folder->Set(file_or_folder.AsUTF8Unsafe(), "Invalid file or folder");
    return;
  }
  // The path of files is fully printed to allow easy copy-paste for developer
  // reference.
  if (!info.is_directory) {
    folder->Set(file_or_folder.AsUTF8Unsafe(),
                base::StrCat({base::NumberToString(info.size), " bytes"}));
    return;
  }

  base::Value::Dict contents;
  base::FileEnumerator files(
      file_or_folder, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath current = files.Next(); !current.empty();
       current = files.Next()) {
    BuildDirectoryState(current, &contents);
  }
  folder->Set(file_or_folder.BaseName().AsUTF8Unsafe(), std::move(contents));
}

base::Value BuildWebAppDiskStateJson(base::FilePath root_directory,
                                     base::Value::List root) {
  base::Value::Dict contents;
  BuildDirectoryState(root_directory, &contents);

  base::Value::Dict section;
  section.Set(kWebAppDirectoryDiskState, std::move(contents));
  root.Append(std::move(section));
  return base::Value(std::move(root));
}

}  // namespace

// static
void WebAppInternalsHandler::BuildDebugInfo(
    Profile* profile,
    base::OnceCallback<void(base::Value root)> callback) {
  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);

  base::Value::List root;
  root.Append(BuildIndexJson());
  root.Append(BuildInstalledWebAppsJson(*provider));
  root.Append(BuildPreinstalledWebAppConfigsJson(*provider));
  root.Append(BuildUserUninstalledPreinstalledWebAppPrefsJson(profile));
  root.Append(BuildExternallyManagedWebAppPrefsJson(profile));
  root.Append(BuildCommandManagerJson(*provider));
  root.Append(BuildIconErrorLogJson(*provider));
  root.Append(BuildInstallProcessErrorLogJson(*provider));
#if BUILDFLAG(IS_MAC)
  root.Append(BuildAppShimRegistryLocalStorageJson());
#endif
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&BuildWebAppDiskStateJson,
                     web_app::GetWebAppsRootDirectory(profile),
                     std::move(root)),
      std::move(callback));
}

WebAppInternalsHandler::WebAppInternalsHandler(
    Profile* profile,
    mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {}

WebAppInternalsHandler::~WebAppInternalsHandler() = default;

void WebAppInternalsHandler::GetDebugInfoAsJsonString(
    GetDebugInfoAsJsonStringCallback callback) {
  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile_);
  if (!provider) {
    return std::move(callback).Run("Web app system not enabled for profile.");
  }

  auto value_to_string =
      base::BindOnce([](base::Value value) { return value.DebugString(); });

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&WebAppInternalsHandler::BuildDebugInfo, profile_,
                     std::move(value_to_string).Then(std::move(callback))));
}
