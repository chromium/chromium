// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"
#endif  //  BUILDFLAG(IS_CHROMEOS)

namespace {

// New fields must be added to BuildIndexJson().
constexpr char kInstalledWebApps[] = "InstalledWebApps";
constexpr char kPreinstalledWebAppConfigs[] = "PreinstalledWebAppConfigs";
constexpr char kUserUninstalledPreinstalledWebAppPrefs[] =
    "UserUninstalledPreinstalledWebAppPrefs";
constexpr char kWebAppPreferences[] = "WebAppPreferences";
constexpr char kWebAppIphPreferences[] = "WebAppIphPreferences";
constexpr char kWebAppMlPreferences[] = "WebAppMlPreferences";
constexpr char kWebAppIphLcPreferences[] = "WebAppIPHLinkCapturingPreferences";
constexpr char kShouldGarbageCollectStoragePartitions[] =
    "ShouldGarbageCollectStoragePartitions";
constexpr char kLockManager[] = "LockManager";
constexpr char kCommandManager[] = "CommandManager";
constexpr char kDatabaseLog[] = "DatabaseLog";
constexpr char kIconErrorLog[] = "IconErrorLog";
#if BUILDFLAG(IS_MAC)
constexpr char kAppShimRegistryLocalStorage[] = "AppShimRegistryLocalStorage";
#endif
constexpr char kWebAppDirectoryDiskState[] = "WebAppDirectoryDiskState";
constexpr char kIsolatedWebAppUpdateManager[] = "IsolatedWebAppUpdateManager";
constexpr char kIsolatedWebAppPolicyManager[] = "IsolatedWebAppPolicyManager";
constexpr char kIwaKeyDistributionInfoProvider[] =
    "IwaKeyDistributionInfoProvider";
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kIwaBundleCacheManager[] = "IwaBundleCacheManager";
#endif  //  BUILDFLAG(IS_CHROMEOS)
constexpr char kNavigationCapturing[] = "NavigationCapturing";

constexpr char kNeedsRecordWebAppDebugInfo[] =
    "No debugging info available! Please enable: "
    "chrome://flags/#record-web-app-debug-info";

base::DictValue BuildIndexJson() {
  return base::DictValue().Set(
      "Index", base::ListValue()
                   // App state
                   .Append(kInstalledWebApps)
#if BUILDFLAG(IS_MAC)
                   .Append(kAppShimRegistryLocalStorage)
#endif
                   // Core component logs.
                   .Append(kLockManager)
                   .Append(kCommandManager)
                   .Append(kDatabaseLog)
                   .Append(kNavigationCapturing)
                   .Append(kIconErrorLog)
                   // Preferences.
                   .Append(kPreinstalledWebAppConfigs)
                   .Append(kUserUninstalledPreinstalledWebAppPrefs)
                   .Append(kWebAppPreferences)
                   .Append(kWebAppIphPreferences)
                   .Append(kWebAppMlPreferences)
                   .Append(kWebAppIphLcPreferences)
                   // Isolated Web App Systems.
                   .Append(kShouldGarbageCollectStoragePartitions)
                   .Append(kIsolatedWebAppUpdateManager)
                   .Append(kIsolatedWebAppPolicyManager)
                   .Append(kIwaKeyDistributionInfoProvider)
#if BUILDFLAG(IS_CHROMEOS)
                   .Append(kIwaBundleCacheManager)
#endif  //  BUILDFLAG(IS_CHROMEOS)
        // Disk state is at the end because it is populated asynchronously.
                   .Append(kWebAppDirectoryDiskState));
}

base::DictValue BuildInstalledWebAppsJson(web_app::WebAppProvider& provider) {
  return base::DictValue().Set(kInstalledWebApps,
                               provider.registrar_unsafe().AsDebugValue());
}

base::DictValue BuildPreinstalledWebAppConfigsJson(
    web_app::WebAppProvider& provider) {
  base::DictValue root;

  const web_app::PreinstalledWebAppManager::DebugInfo* debug_info =
      provider.preinstalled_web_app_manager().debug_info();
  if (!debug_info) {
    root.Set(kPreinstalledWebAppConfigs, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  auto config_to_dict = [](const auto& config) {
    return base::DictValue()
        .Set("!Reason", config.second)
        .Set("Config", config.first.AsDebugValue());
  };

  root.Set(
      kPreinstalledWebAppConfigs,
      base::DictValue()
          .Set("ConfigParseErrors", base::ToValueList(debug_info->parse_errors))
          .Set("UninstallConfigs",
               base::ToValueList(debug_info->uninstall_configs, config_to_dict))
          .Set("InstallConfigs",
               base::ToValueList(debug_info->install_configs, config_to_dict))
          .Set("IgnoreConfigs",
               base::ToValueList(debug_info->ignore_configs, config_to_dict))
          .Set("InstallResults",
               base::ToValueList(
                   debug_info->install_results,
                   [](const auto& install_result) {
                     return base::DictValue()
                         .Set("InstallUrl", install_result.first.spec())
                         .Set("ResultCode",
                              base::ToString(install_result.second.code))
                         .Set("DidUninstallAndReplace",
                              install_result.second.did_uninstall_and_replace);
                   }))
          .Set("IsStartUpTaskComplete", debug_info->is_start_up_task_complete)
          .Set("UninstallResults",
               base::ToValueList(
                   debug_info->uninstall_results,
                   [](const auto& uninstall_result) {
                     return base::DictValue()
                         .Set("InstallUrl", uninstall_result.first.spec())
                         .Set("Success",
                              base::ToString(uninstall_result.second));
                   })));

  return root;
}

base::DictValue BuildUserUninstalledPreinstalledWebAppPrefsJson(
    Profile* profile) {
  return base::DictValue().Set(
      kUserUninstalledPreinstalledWebAppPrefs,
      profile->GetPrefs()
          ->GetDict(prefs::kUserUninstalledPreinstalledWebAppPref)
          .Clone());
}

base::DictValue BuildWebAppsPrefsJson(Profile* profile) {
  return base::DictValue().Set(
      kWebAppPreferences,
      profile->GetPrefs()->GetDict(prefs::kWebAppsPreferences).Clone());
}

base::DictValue BuildWebAppIphPrefsJson(Profile* profile) {
  return base::DictValue().Set(
      kWebAppIphPreferences,
      profile->GetPrefs()->GetDict(prefs::kWebAppsAppAgnosticIphState).Clone());
}

base::DictValue BuildWebAppMlPrefsJson(Profile* profile) {
  return base::DictValue().Set(
      kWebAppMlPreferences,
      profile->GetPrefs()->GetDict(prefs::kWebAppsAppAgnosticMlState).Clone());
}

base::DictValue BuildWebAppLinkCapturingIphPrefsJson(Profile* profile) {
  return base::DictValue().Set(
      kWebAppIphLcPreferences,
      profile->GetPrefs()
          ->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState)
          .Clone());
}

base::DictValue BuildShouldGarbageCollectStoragePartitionsPrefsJson(
    Profile* profile) {
  return base::DictValue().Set(
      kShouldGarbageCollectStoragePartitions,
      profile->GetPrefs()->GetBoolean(
          prefs::kShouldGarbageCollectStoragePartitions));
}

base::DictValue BuildLockManagerJson(web_app::WebAppProvider& provider) {
  return base::DictValue().Set(
      kLockManager, provider.command_manager().lock_manager().ToDebugValue());
}

base::DictValue BuildCommandManagerJson(web_app::WebAppProvider& provider) {
  return base::DictValue().Set(kCommandManager,
                               provider.command_manager().ToDebugValue());
}

base::DictValue BuildDatabaseLogJson(web_app::WebAppProvider& provider) {
  const web_app::PersistableLog* log =
      provider.sync_bridge_unsafe().database_log();
  if (!log) {
    return base::DictValue();
  }
  return base::DictValue().Set(kDatabaseLog, log->CloneToList());
}

base::DictValue BuildIconErrorLogJson(web_app::WebAppProvider& provider) {
  base::DictValue root;

  const std::vector<std::string>* error_log =
      provider.icon_manager().error_log();

  if (!error_log) {
    root.Set(kIconErrorLog, kNeedsRecordWebAppDebugInfo);
    return root;
  }

  root.Set(kIconErrorLog, base::ToValueList(*error_log));

  return root;
}

#if BUILDFLAG(IS_MAC)
base::DictValue BuildAppShimRegistryLocalStorageJson() {
  return base::DictValue().Set(kAppShimRegistryLocalStorage,
                               AppShimRegistry::Get()->AsDebugDict().Clone());
}
#endif

base::Value BuildIsolatedWebAppUpdaterManagerJson(
    web_app::WebAppProvider& provider) {
  return base::Value(base::DictValue().Set(
      kIsolatedWebAppUpdateManager,
      provider.isolated_web_app_update_manager().AsDebugValue()));
}

base::Value BuildIsolatedWebAppPolicyManagerJson(
    web_app::WebAppProvider& provider) {
  return base::Value(base::DictValue().Set(
      kIsolatedWebAppPolicyManager,
      provider.isolated_web_app_policy_manager().GetDebugValue()));
}

base::Value BuildIwaKeyDistributionInfoProviderJson(
    base::PassKey<WebAppInternalsHandler> pass_key) {
  return base::Value(base::DictValue().Set(
      kIwaKeyDistributionInfoProvider,
      web_app::IwaKeyDistributionInfoProvider::GetInstance(pass_key)
          .AsDebugValue()));
}

#if BUILDFLAG(IS_CHROMEOS)
base::Value BuildIwaCacheManagerJson(web_app::WebAppProvider& provider) {
  return base::Value(base::DictValue().Set(
      kIwaBundleCacheManager,
      provider.isolated_web_app_cache_manager().GetDebugValue()));
}
#endif  //  BUILDFLAG(IS_CHROMEOS)

void BuildDirectoryState(base::FilePath file_or_folder,
                         base::DictValue* folder) {
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

  base::DictValue contents;
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
                                     base::ListValue root) {
  base::DictValue contents;
  BuildDirectoryState(root_directory, &contents);

  root.Append(
      base::DictValue().Set(kWebAppDirectoryDiskState, std::move(contents)));
  return base::Value(std::move(root));
}

base::DictValue BuildNavigationCapturingLog(web_app::WebAppProvider& provider) {
  return base::DictValue().Set(kNavigationCapturing,
                               provider.navigation_capturing_log().GetLog());
}

}  // namespace

// static
void WebAppInternalsHandler::BuildDebugInfo(
    Profile* profile,
    base::OnceCallback<void(base::Value root)> callback) {
  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);

  base::ListValue root =
      base::ListValue()
          .Append(BuildIndexJson())
          // App state
          .Append(BuildInstalledWebAppsJson(*provider))
#if BUILDFLAG(IS_MAC)
          .Append(BuildAppShimRegistryLocalStorageJson())
#endif
          // Core components
          .Append(BuildLockManagerJson(*provider))
          .Append(BuildNavigationCapturingLog(*provider))
          .Append(BuildCommandManagerJson(*provider))
          .Append(BuildDatabaseLogJson(*provider))
          .Append(BuildIconErrorLogJson(*provider))
          // Preferences
          .Append(BuildPreinstalledWebAppConfigsJson(*provider))
          .Append(BuildUserUninstalledPreinstalledWebAppPrefsJson(profile))
          .Append(BuildWebAppsPrefsJson(profile))
          .Append(BuildWebAppIphPrefsJson(profile))
          .Append(BuildWebAppMlPrefsJson(profile))
          .Append(BuildWebAppLinkCapturingIphPrefsJson(profile))
          // Isolated Web App Systems.
          .Append(BuildShouldGarbageCollectStoragePartitionsPrefsJson(profile))
          .Append(BuildIsolatedWebAppUpdaterManagerJson(*provider))
          .Append(BuildIsolatedWebAppPolicyManagerJson(*provider))
#if BUILDFLAG(IS_CHROMEOS)
          .Append(BuildIwaCacheManagerJson(*provider))
#endif  //  BUILDFLAG(IS_CHROMEOS)
          .Append(BuildIwaKeyDistributionInfoProviderJson(
              base::PassKey<WebAppInternalsHandler>()));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&BuildWebAppDiskStateJson,
                     web_app::GetWebAppsRootDirectory(profile),
                     std::move(root)),
      std::move(callback));
}

WebAppInternalsHandler::WebAppInternalsHandler(
    content::WebUI* web_ui,
    mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver)
    : web_ui_(raw_ref<content::WebUI>::from_ptr(web_ui)),
      profile_(raw_ref<Profile>::from_ptr(Profile::FromBrowserContext(
          web_ui_->GetWebContents()->GetBrowserContext()))),
      receiver_(this, std::move(receiver)) {
  if (content::AreIsolatedWebAppsEnabled(&*profile_)) {
    iwa_handler_.emplace(*web_ui_, *profile_);
  }
}

WebAppInternalsHandler::~WebAppInternalsHandler() = default;

void WebAppInternalsHandler::GetDebugInfoAsJsonString(
    GetDebugInfoAsJsonStringCallback callback) {
  auto* provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(&profile_.get());
  if (!provider) {
    return std::move(callback).Run("Web app system not enabled for profile.");
  }

  auto value_to_string =
      base::BindOnce([](base::Value value) { return value.DebugString(); });

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&WebAppInternalsHandler::BuildDebugInfo, &profile_.get(),
                     std::move(value_to_string).Then(std::move(callback))));
}

void WebAppInternalsHandler::InstallIsolatedWebAppFromDevProxy(
    const GURL& url,
    InstallIsolatedWebAppFromDevProxyCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->InstallIsolatedWebAppFromDevProxy(url, std::move(callback));
  }
}

void WebAppInternalsHandler::ParseUpdateManifestFromUrl(
    const GURL& update_manifest_url,
    ParseUpdateManifestFromUrlCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->ParseUpdateManifestFromUrl(update_manifest_url,
                                             std::move(callback));
  }
}

void WebAppInternalsHandler::InstallIsolatedWebAppFromBundleUrl(
    mojom::InstallFromBundleUrlParamsPtr params,
    InstallIsolatedWebAppFromBundleUrlCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                                     std::move(callback));
  }
}

void WebAppInternalsHandler::SelectFileAndInstallIsolatedWebAppFromDevBundle(
    SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->SelectFileAndInstallIsolatedWebAppFromDevBundle(
        std::move(callback));
  }
}

void WebAppInternalsHandler::SelectFileAndUpdateIsolatedWebAppFromDevBundle(
    const webapps::AppId& app_id,
    SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->SelectFileAndUpdateIsolatedWebAppFromDevBundle(
        app_id, std::move(callback));
  }
}

void WebAppInternalsHandler::SearchForIsolatedWebAppUpdates(
    SearchForIsolatedWebAppUpdatesCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->SearchForIsolatedWebAppUpdates(std::move(callback));
  }
}

void WebAppInternalsHandler::GetIsolatedWebAppDevModeAppInfo(
    GetIsolatedWebAppDevModeAppInfoCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->GetIsolatedWebAppDevModeAppInfo(std::move(callback));
  }
}

void WebAppInternalsHandler::UpdateDevProxyIsolatedWebApp(
    const webapps::AppId& app_id,
    UpdateDevProxyIsolatedWebAppCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->UpdateDevProxyIsolatedWebApp(app_id, std::move(callback));
  }
}

void WebAppInternalsHandler::RotateKey(const std::string& web_bundle_id,
                                       const std::vector<uint8_t>& public_key) {
  if (iwa_handler_) {
    iwa_handler_->RotateKey(web_bundle_id, public_key);
  }
}

void WebAppInternalsHandler::UpdateManifestInstalledIsolatedWebApp(
    const webapps::AppId& app_id,
    UpdateManifestInstalledIsolatedWebAppCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->UpdateManifestInstalledIsolatedWebApp(app_id,
                                                        std::move(callback));
  }
}

void WebAppInternalsHandler::SetUpdateChannelForIsolatedWebApp(
    const webapps::AppId& app_id,
    const std::string& update_channel,
    SetUpdateChannelForIsolatedWebAppCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->SetUpdateChannelForIsolatedWebApp(app_id, update_channel,
                                                    std::move(callback));
  }
}

void WebAppInternalsHandler::SetPinnedVersionForIsolatedWebApp(
    const webapps::AppId& app_id,
    const std::string& pinned_version,
    SetPinnedVersionForIsolatedWebAppCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->SetPinnedVersionForIsolatedWebApp(app_id, pinned_version,
                                                    std::move(callback));
  }
}

void WebAppInternalsHandler::ResetPinnedVersionForIsolatedWebApp(
    const webapps::AppId& app_id) {
  if (iwa_handler_) {
    iwa_handler_->ResetPinnedVersionForIsolatedWebApp(app_id);
  }
}

void WebAppInternalsHandler::SetAllowDowngradesForIsolatedWebApp(
    bool allow_downgrades,
    const webapps::AppId& app_id) {
  if (iwa_handler_) {
    iwa_handler_->SetAllowDowngradesForIsolatedWebApp(allow_downgrades, app_id);
  }
}

void WebAppInternalsHandler::DeleteIsolatedWebApp(
    const webapps::AppId& app_id,
    DeleteIsolatedWebAppCallback callback) {
  if (iwa_handler_) {
    iwa_handler_->DeleteIsolatedWebApp(app_id, std::move(callback));
  }
}
