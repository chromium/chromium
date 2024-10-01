// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"

#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_thread.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
constexpr char kIconErrorLog[] = "IconErrorLog";
constexpr char kInstallationProcessErrorLog[] = "InstallationProcessErrorLog";
#if BUILDFLAG(IS_MAC)
constexpr char kAppShimRegistryLocalStorage[] = "AppShimRegistryLocalStorage";
#endif
constexpr char kWebAppDirectoryDiskState[] = "WebAppDirectoryDiskState";
constexpr char kIsolatedWebAppUpdateManager[] = "IsolatedWebAppUpdateManager";
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kIsolatedWebAppPolicyManager[] = "IsolatedWebAppPolicyManager";
#endif  // BUILDFLAG(IS_CHROMEOS)
constexpr char kIwaKeyDistributionInfoProvider[] =
    "IwaKeyDistributionInfoProvider";
constexpr char kNavigationCapturing[] = "NavigationCapturing";

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
  index.Append(kWebAppPreferences);
  index.Append(kWebAppIphPreferences);
  index.Append(kWebAppMlPreferences);
  index.Append(kWebAppIphLcPreferences);
  index.Append(kShouldGarbageCollectStoragePartitions);
  index.Append(kLockManager);
  index.Append(kNavigationCapturing);
  index.Append(kCommandManager);
  index.Append(kIconErrorLog);
  index.Append(kInstallationProcessErrorLog);
#if BUILDFLAG(IS_MAC)
  index.Append(kAppShimRegistryLocalStorage);
#endif
  index.Append(kIsolatedWebAppUpdateManager);
#if BUILDFLAG(IS_CHROMEOS)
  index.Append(kIsolatedWebAppPolicyManager);
#endif  // BUILDFLAG(IS_CHROMEOS)
  index.Append(kWebAppDirectoryDiskState);

  return root;
}

base::Value::Dict BuildInstalledWebAppsJson(web_app::WebAppProvider& provider) {
  base::Value::Dict root;
  root.Set(kInstalledWebApps, provider.registrar_unsafe().AsDebugValue());
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

  base::Value::List& uninstall_configs =
      *preinstalled_web_app_configs.EnsureList("UninstallConfigs");
  for (const std::pair<web_app::ExternalInstallOptions, std::string>&
           uninstall_config : debug_info->uninstall_configs) {
    base::Value::Dict entry;
    entry.Set("!Reason", uninstall_config.second);
    entry.Set("Config", uninstall_config.first.AsDebugValue());
    uninstall_configs.Append(std::move(entry));
  }

  base::Value::List& install_configs =
      *preinstalled_web_app_configs.EnsureList("InstallConfigs");
  for (const std::pair<web_app::ExternalInstallOptions, std::string>&
           install_config : debug_info->install_configs) {
    base::Value::Dict entry;
    entry.Set("!Reason", install_config.second);
    entry.Set("Config", install_config.first.AsDebugValue());
    install_configs.Append(std::move(entry));
  }

  base::Value::List& ignore_configs =
      *preinstalled_web_app_configs.EnsureList("IgnoreConfigs");
  for (const std::pair<web_app::ExternalInstallOptions, std::string>&
           ignore_config : debug_info->ignore_configs) {
    base::Value::Dict entry;
    entry.Set("!Reason", ignore_config.second);
    entry.Set("Config", ignore_config.first.AsDebugValue());
    ignore_configs.Append(std::move(entry));
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
  for (std::pair<const GURL&, webapps::UninstallResultCode> uninstall_result :
       debug_info->uninstall_results) {
    base::Value::Dict entry;
    entry.Set("InstallUrl", uninstall_result.first.spec());
    entry.Set("Success", base::ToString(uninstall_result.second));
    uninstall_results.Append(std::move(entry));
  }

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

base::Value::Dict BuildWebAppsPrefsJson(Profile* profile) {
  base::Value::Dict root;
  root.Set(kWebAppPreferences,
           profile->GetPrefs()->GetDict(prefs::kWebAppsPreferences).Clone());
  return root;
}

base::Value::Dict BuildWebAppIphPrefsJson(Profile* profile) {
  base::Value::Dict root;
  root.Set(
      kWebAppIphPreferences,
      profile->GetPrefs()->GetDict(prefs::kWebAppsAppAgnosticIphState).Clone());
  return root;
}

base::Value::Dict BuildWebAppMlPrefsJson(Profile* profile) {
  base::Value::Dict root;
  root.Set(
      kWebAppMlPreferences,
      profile->GetPrefs()->GetDict(prefs::kWebAppsAppAgnosticMlState).Clone());
  return root;
}

base::Value::Dict BuildWebAppLinkCapturingIphPrefsJson(Profile* profile) {
  base::Value::Dict root;
  root.Set(kWebAppIphLcPreferences,
           profile->GetPrefs()
               ->GetDict(prefs::kWebAppsAppAgnosticIPHLinkCapturingState)
               .Clone());
  return root;
}

base::Value::Dict BuildShouldGarbageCollectStoragePartitionsPrefsJson(
    Profile* profile) {
  base::Value::Dict root;
  root.Set(kShouldGarbageCollectStoragePartitions,
           profile->GetPrefs()->GetBoolean(
               prefs::kShouldGarbageCollectStoragePartitions));
  return root;
}

base::Value::Dict BuildLockManagerJson(web_app::WebAppProvider& provider) {
  base::Value::Dict root;
  root.Set(kLockManager,
           provider.command_manager().lock_manager().ToDebugValue());
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

base::Value BuildIsolatedWebAppUpdaterManagerJson(
    web_app::WebAppProvider& provider) {
  return base::Value(
      base::Value::Dict().Set(kIsolatedWebAppUpdateManager,
                              provider.iwa_update_manager().AsDebugValue()));
}

#if BUILDFLAG(IS_CHROMEOS)
base::Value BuildIsolatedWebAppPolicyManagerJson(
    web_app::WebAppProvider& provider) {
  return base::Value(
      base::Value::Dict().Set(kIsolatedWebAppPolicyManager,
                              provider.iwa_policy_manager().GetDebugValue()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::Value BuildIwaKeyDistributionInfoProviderJson() {
  return base::Value(base::Value::Dict().Set(
      kIwaKeyDistributionInfoProvider,
      web_app::IwaKeyDistributionInfoProvider::GetInstance()->AsDebugValue()));
}

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

base::Value::Dict BuildNavigationCapturingLog(
    web_app::WebAppProvider& provider) {
  base::Value::Dict root;
  root.Set(kNavigationCapturing, provider.navigation_capturing_log().GetLog());
  return root;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class ObliterateStoragePartitionHelper
    : public base::RefCountedThreadSafe<ObliterateStoragePartitionHelper> {
 public:
  using Callback = mojom::WebAppInternalsHandler::
      ClearExperimentalWebAppIsolationDataCallback;

  explicit ObliterateStoragePartitionHelper(Callback callback)
      : callback_{std::move(callback)} {}

  void OnGcRequired() {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    CHECK(!callback_.is_null()) << "OnDone() is called before OnGcRequired";
    gc_required_ = true;
  }

  void OnDone() {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    std::move(callback_).Run(!gc_required_);
  }

 private:
  friend class base::RefCountedThreadSafe<ObliterateStoragePartitionHelper>;
  ~ObliterateStoragePartitionHelper() = default;

  Callback callback_;
  bool gc_required_ = false;
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
  root.Append(BuildWebAppsPrefsJson(profile));
  root.Append(BuildWebAppIphPrefsJson(profile));
  root.Append(BuildWebAppMlPrefsJson(profile));
  root.Append(BuildWebAppLinkCapturingIphPrefsJson(profile));
  root.Append(BuildShouldGarbageCollectStoragePartitionsPrefsJson(profile));
  root.Append(BuildLockManagerJson(*provider));
  root.Append(BuildNavigationCapturingLog(*provider));
  root.Append(BuildCommandManagerJson(*provider));
  root.Append(BuildIconErrorLogJson(*provider));
  root.Append(BuildInstallProcessErrorLogJson(*provider));
#if BUILDFLAG(IS_MAC)
  root.Append(BuildAppShimRegistryLocalStorageJson());
#endif
  root.Append(BuildIsolatedWebAppUpdaterManagerJson(*provider));
#if BUILDFLAG(IS_CHROMEOS)
  root.Append(BuildIsolatedWebAppPolicyManagerJson(*provider));
#endif  // BUILDFLAG(IS_CHROMEOS)
  root.Append(BuildIwaKeyDistributionInfoProviderJson());
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
      receiver_(this, std::move(receiver)),
      iwa_handler_(*web_ui_, *profile_) {}

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
  iwa_handler_.InstallIsolatedWebAppFromDevProxy(url, std::move(callback));
}

void WebAppInternalsHandler::ParseUpdateManifestFromUrl(
    const GURL& update_manifest_url,
    ParseUpdateManifestFromUrlCallback callback) {
  iwa_handler_.ParseUpdateManifestFromUrl(update_manifest_url,
                                          std::move(callback));
}

void WebAppInternalsHandler::InstallIsolatedWebAppFromBundleUrl(
    mojom::InstallFromBundleUrlParamsPtr params,
    InstallIsolatedWebAppFromBundleUrlCallback callback) {
  iwa_handler_.InstallIsolatedWebAppFromBundleUrl(std::move(params),
                                                  std::move(callback));
}

void WebAppInternalsHandler::SelectFileAndInstallIsolatedWebAppFromDevBundle(
    SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback) {
  iwa_handler_.SelectFileAndInstallIsolatedWebAppFromDevBundle(
      std::move(callback));
}

void WebAppInternalsHandler::SelectFileAndUpdateIsolatedWebAppFromDevBundle(
    const webapps::AppId& app_id,
    SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback) {
  iwa_handler_.SelectFileAndUpdateIsolatedWebAppFromDevBundle(
      app_id, std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WebAppInternalsHandler::ClearExperimentalWebAppIsolationData(
    ClearExperimentalWebAppIsolationDataCallback callback) {
  CHECK(base::FeatureList::IsEnabled(
      chromeos::features::kExperimentalWebAppStoragePartitionIsolation));

  // Remove app storage partitions.
  auto helper = base::MakeRefCounted<ObliterateStoragePartitionHelper>(
      std::move(callback));
  // It is a bit hard to work with AsyncObliterate...() since it takes two
  // separate callbacks. It is probably better to change it to only take a
  // "done" callback which has a "gc_required" param.
  profile_->AsyncObliterateStoragePartition(
      web_app::kExperimentalWebAppStorageParitionDomain,
      base::BindOnce(&ObliterateStoragePartitionHelper::OnGcRequired, helper),
      base::BindOnce(&ObliterateStoragePartitionHelper::OnDone, helper));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void WebAppInternalsHandler::SearchForIsolatedWebAppUpdates(
    SearchForIsolatedWebAppUpdatesCallback callback) {
  iwa_handler_.SearchForIsolatedWebAppUpdates(std::move(callback));
}

void WebAppInternalsHandler::GetIsolatedWebAppDevModeAppInfo(
    GetIsolatedWebAppDevModeAppInfoCallback callback) {
  iwa_handler_.GetIsolatedWebAppDevModeAppInfo(std::move(callback));
}

void WebAppInternalsHandler::UpdateDevProxyIsolatedWebApp(
    const webapps::AppId& app_id,
    UpdateDevProxyIsolatedWebAppCallback callback) {
  iwa_handler_.UpdateDevProxyIsolatedWebApp(app_id, std::move(callback));
}

void WebAppInternalsHandler::RotateKey(
    const std::string& web_bundle_id,
    const std::optional<std::vector<uint8_t>>& public_key) {
  iwa_handler_.RotateKey(web_bundle_id, public_key);
}

void WebAppInternalsHandler::UpdateManifestInstalledIsolatedWebApp(
    const webapps::AppId& app_id,
    UpdateManifestInstalledIsolatedWebAppCallback callback) {
  iwa_handler_.UpdateManifestInstalledIsolatedWebApp(app_id,
                                                     std::move(callback));
}
