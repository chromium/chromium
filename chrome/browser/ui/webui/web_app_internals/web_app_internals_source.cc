// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_source.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
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
#include "chrome/common/webui_url_constants.h"
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

base::Value BuildIndexJson() {
  base::Value root(base::Value::Type::DICTIONARY);
  base::Value& index =
      *root.SetKey("Index", base::Value(base::Value::Type::LIST));

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

base::Value BuildInstalledWebAppsJson(web_app::WebAppProvider& provider) {
  base::Value root(base::Value::Type::DICTIONARY);

  base::Value& installed_web_apps = *root.SetKey(
      kInstalledWebApps, base::Value(base::Value::Type::DICTIONARY));

  std::vector<const web_app::WebApp*> web_apps;
  for (const web_app::WebApp& web_app :
       provider.registrar().GetAppsIncludingStubs()) {
    web_apps.push_back(&web_app);
  }
  base::ranges::sort(web_apps, {}, &web_app::WebApp::untranslated_name);

  // Prefix with a ! so this appears at the top when serialized.
  base::Value& index = *installed_web_apps.SetKey(
      "!Index", base::Value(base::Value::Type::DICTIONARY));
  for (const web_app::WebApp* web_app : web_apps) {
    const std::string& key = web_app->untranslated_name();
    base::Value* existing_entry = index.FindKey(key);
    if (!existing_entry) {
      index.SetStringKey(key, web_app->app_id());
      continue;
    }
    // If any web apps share identical names then collect a list of app IDs.
    const std::string* existing_id = existing_entry->GetIfString();
    if (existing_id) {
      base::Value id_copy(*existing_id);
      index.SetKey(key, base::Value(base::Value::Type::LIST))
          ->Append(std::move(id_copy));
    }
    index.FindListKey(key)->Append(web_app->app_id());
  }

  base::Value& web_app_details = *installed_web_apps.SetKey(
      "Details", base::Value(base::Value::Type::LIST));
  for (const web_app::WebApp* web_app : web_apps)
    web_app_details.Append(web_app->AsDebugValue());

  return root;
}

base::Value BuildPreinstalledWebAppConfigsJson(
    web_app::WebAppProvider& provider) {
  base::Value root(base::Value::Type::DICTIONARY);

  const web_app::PreinstalledWebAppManager::DebugInfo* debug_info =
      provider.preinstalled_web_app_manager().debug_info();
  if (!debug_info) {
    root.SetStringKey(kPreinstalledWebAppConfigs, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  base::Value& preinstalled_web_app_configs = *root.SetKey(
      kPreinstalledWebAppConfigs, base::Value(base::Value::Type::DICTIONARY));

  base::Value& config_parse_errors = *preinstalled_web_app_configs.SetKey(
      "ConfigParseErrors", base::Value(base::Value::Type::LIST));
  for (const std::string& parse_error : debug_info->parse_errors)
    config_parse_errors.Append(parse_error);

  base::Value& configs_enabled = *preinstalled_web_app_configs.SetKey(
      "ConfigsEnabled", base::Value(base::Value::Type::LIST));
  for (const web_app::ExternalInstallOptions& enabled_config :
       debug_info->enabled_configs) {
    configs_enabled.Append(enabled_config.AsDebugValue());
  }

  base::Value& configs_disabled = *preinstalled_web_app_configs.SetKey(
      "ConfigsDisabled", base::Value(base::Value::Type::LIST));
  for (const std::pair<web_app::ExternalInstallOptions, std::string>&
           disabled_config : debug_info->disabled_configs) {
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey("!Reason", disabled_config.second);
    entry.SetKey("Config", disabled_config.first.AsDebugValue());
    configs_disabled.Append(std::move(entry));
  }

  base::Value& install_results = *preinstalled_web_app_configs.SetKey(
      "InstallResults", base::Value(base::Value::Type::LIST));
  for (std::pair<const GURL&,
                 const web_app::ExternallyManagedAppManager::InstallResult&>
           install_result : debug_info->install_results) {
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey("InstallUrl", install_result.first.spec());
    entry.SetStringKey("ResultCode",
                       ConvertToString(install_result.second.code));
    entry.SetBoolKey("DidUninstallAndReplace",
                     install_result.second.did_uninstall_and_replace);
    install_results.Append(std::move(entry));
  }

  preinstalled_web_app_configs.SetBoolKey(
      "IsStartUpTaskComplete", debug_info->is_start_up_task_complete);

  base::Value& uninstall_results = *preinstalled_web_app_configs.SetKey(
      "UninstallResults", base::Value(base::Value::Type::LIST));
  for (std::pair<const GURL&, const bool&> uninstall_result :
       debug_info->uninstall_results) {
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey("InstallUrl", uninstall_result.first.spec());
    entry.SetBoolKey("Success", uninstall_result.second);
    uninstall_results.Append(std::move(entry));
  }

  return root;
}

base::Value BuildExternallyManagedWebAppPrefsJson(Profile* profile) {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(
      kExternallyManagedWebAppPrefs,
      base::Value(
          profile->GetPrefs()->GetDict(prefs::kWebAppsExtensionIDs).Clone()));
  return root;
}

base::Value BuildUserUninstalledPreinstalledWebAppPrefsJson(Profile* profile) {
  base::Value::Dict root;
  root.Set(kUserUninstalledPreinstalledWebAppPrefs,
           profile->GetPrefs()
               ->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref)
               .Clone());
  return base::Value(std::move(root));
}

base::Value BuildCommandManagerJson(web_app::WebAppProvider& provider) {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey(kCommandManager, provider.command_manager().ToDebugValue());
  return root;
}

base::Value BuildIconErrorLogJson(web_app::WebAppProvider& provider) {
  base::Value root(base::Value::Type::DICTIONARY);

  const std::vector<std::string>* error_log =
      provider.icon_manager().error_log();

  if (!error_log) {
    root.SetStringKey(kIconErrorLog, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  base::Value& icon_error_log =
      *root.SetKey(kIconErrorLog, base::Value(base::Value::Type::LIST));
  for (const std::string& error : *error_log)
    icon_error_log.Append(error);

  return root;
}

base::Value BuildInstallProcessErrorLogJson(web_app::WebAppProvider& provider) {
  base::Value root(base::Value::Type::DICTIONARY);

  const web_app::WebAppInstallManager::ErrorLog* error_log =
      provider.install_manager().error_log();

  if (!error_log) {
    root.SetStringKey(kInstallationProcessErrorLog,
                      kNeedsRecordWebAppDebugInfo);
    return root;
  }

  base::Value& installation_process_error_log = *root.SetKey(
      kInstallationProcessErrorLog, base::Value(base::Value::Type::LIST));
  for (const base::Value& error : *error_log)
    installation_process_error_log.Append(error.Clone());

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
                                     base::Value root) {
  base::Value::Dict contents;
  BuildDirectoryState(root_directory, &contents);

  base::Value::Dict section;
  section.Set(kWebAppDirectoryDiskState, std::move(contents));
  root.Append(base::Value(std::move(section)));
  return root;
}

void BuildResponse(Profile* profile,
                   base::OnceCallback<void(base::Value root)> callback) {
  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return std::move(callback).Run(
        base::Value("Web app system not enabled for profile."));
  }

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&WebAppInternalsSource::BuildWebAppInternalsJson, profile,
                     std::move(callback)));
}

void ConvertValueToJsonData(content::URLDataSource::GotDataCallback callback,
                            base::Value value) {
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(value.DebugString()));
}

}  // namespace

// static
void WebAppInternalsSource::BuildWebAppInternalsJson(
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
                     base::Value(std::move(root))),
      std::move(callback));
}

WebAppInternalsSource::WebAppInternalsSource(Profile* profile)
    : profile_(profile) {}

WebAppInternalsSource::~WebAppInternalsSource() = default;

std::string WebAppInternalsSource::GetSource() {
  return chrome::kChromeUIWebAppInternalsHost;
}

std::string WebAppInternalsSource::GetMimeType(const GURL& url) {
  return "application/json";
}

void WebAppInternalsSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  BuildResponse(profile_,
                base::BindOnce(&ConvertValueToJsonData, std::move(callback)));
}
