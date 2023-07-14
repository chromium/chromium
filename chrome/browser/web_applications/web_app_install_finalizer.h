// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#endif

class Profile;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class WebApp;
class WebAppProvider;

// An finalizer for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers an app.
class WebAppInstallFinalizer {
 public:
  using InstallFinalizedCallback =
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              OsHooksErrors os_hooks_errors)>;
  using UninstallWebAppCallback =
      base::OnceCallback<void(webapps::UninstallResultCode code)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const AppId& app_id,
                                   webapps::UninstallResultCode code)>;

  struct FinalizeOptions {
    explicit FinalizeOptions(webapps::WebappInstallSource install_surface);
    ~FinalizeOptions();
    FinalizeOptions(const FinalizeOptions&);

    const WebAppManagement::Type source;
    const webapps::WebappInstallSource install_surface;
    bool locally_installed = true;
    bool overwrite_existing_manifest_fields = true;
    bool skip_icon_writes_on_download_failure = false;

    absl::optional<WebAppChromeOsData> chromeos_data;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    absl::optional<ash::SystemWebAppData> system_web_app_data;
#endif

    // If set, will set `WebApp::IsolationData` with the given location, as well
    // as the version from `WebAppInstallInfo::isolated_web_app_version`. Will
    // `CHECK` if `web_app_info.isolated_web_app_version` is invalid.
    absl::optional<web_app::IsolatedWebAppLocation> isolated_web_app_location;

    // If true, OsIntegrationManager::InstallOsHooks won't be called at all,
    // meaning that all other OS Hooks related parameters below will be ignored.
    bool bypass_os_hooks = false;

    // These OS shortcut fields can't be true if |locally_installed| is false.
    // They only have an effect when |bypass_os_hooks| is false.
    bool add_to_applications_menu = true;
    bool add_to_desktop = true;
    bool add_to_quick_launch_bar = true;

    // Controls fetching and validation of web_app_origin_association data
    // from web origins found in manifest scope_extensions entries. If true,
    // do not validate even if scope_extensions has valid entries.
    bool skip_origin_association_validation = false;
  };

  explicit WebAppInstallFinalizer(Profile* profile);
  WebAppInstallFinalizer(const WebAppInstallFinalizer&) = delete;
  WebAppInstallFinalizer& operator=(const WebAppInstallFinalizer&) = delete;
  virtual ~WebAppInstallFinalizer();

  // Write the WebApp data to disk and register the app.
  void FinalizeInstall(const WebAppInstallInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback);

  // Write the new WebApp data to disk and update the app.
  // TODO(https://crbug.com/1196051): Chrome fails to update the manifest
  // if the app window needing update closes at the same time as Chrome.
  // Therefore, the manifest may not always update as expected.
  // Virtual for testing.
  virtual void FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                              InstallFinalizedCallback callback);

  bool CanReparentTab(const AppId& app_id, bool shortcut_created) const;
  void ReparentTab(const AppId& app_id,
                   bool shortcut_created,
                   content::WebContents* web_contents);

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  Profile* profile() { return profile_; }

  // Writes external config data to the web_app DB, mapped per source.
  void WriteExternalConfigMapInfo(
      WebApp& web_app,
      WebAppManagement::Type source,
      bool is_placeholder,
      GURL install_url,
      std::vector<std::string> additional_policy_ids);

 private:
  using CommitCallback = base::OnceCallback<void(bool success)>;

  void OnMaybeRegisterOsUninstall(const AppId& app_id,
                                  WebAppManagement::Type source,
                                  UninstallWebAppCallback callback,
                                  OsHooksErrors os_hooks_errors);

  void UpdateIsolationDataAndResetPendingUpdateInfo(
      WebApp* web_app,
      const IsolatedWebAppLocation& location,
      const base::Version& version);

  void SetWebAppManifestFieldsAndWriteData(
      const WebAppInstallInfo& web_app_info,
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback,
      bool skip_icon_writes_on_download_failure);

  void WriteTranslations(
      const AppId& app_id,
      const base::flat_map<std::string, blink::Manifest::TranslationItem>&
          translations,
      CommitCallback commit_callback,
      bool success);

  void CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                          CommitCallback commit_callback,
                          bool success);

  void OnOriginAssociationValidated(WebAppInstallInfo web_app_info,
                                    FinalizeOptions options,
                                    InstallFinalizedCallback callback,
                                    AppId app_id,
                                    ScopeExtensions validated_scope_extensions);

  void OnDatabaseCommitCompletedForInstall(InstallFinalizedCallback callback,
                                           AppId app_id,
                                           FinalizeOptions finalize_options,
                                           bool success);

  void OnInstallHooksFinished(InstallFinalizedCallback callback,
                              AppId app_id,
                              OsHooksErrors os_hooks_errors);
  void NotifyWebAppInstalledWithOsHooks(AppId app_id);

  bool ShouldUpdateOsHooks(const AppId& app_id);

  void OnDatabaseCommitCompletedForUpdate(
      InstallFinalizedCallback callback,
      AppId app_id,
      std::string old_name,
      FileHandlerUpdateAction file_handlers_need_os_update,
      const WebAppInstallInfo& web_app_info,
      bool success);

  void OnUpdateHooksFinished(InstallFinalizedCallback callback,
                             AppId app_id,
                             std::string old_name,
                             OsHooksErrors os_hooks_errors);

  // Returns a value indicating whether the file handlers registered with the OS
  // should be updated. Used to avoid unnecessary updates. TODO(estade): why
  // does this optimization exist when other OS hooks don't have similar
  // optimizations?
  FileHandlerUpdateAction GetFileHandlerUpdateAction(
      const AppId& app_id,
      const WebAppInstallInfo& new_web_app_info);

  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool started_ = false;

  base::WeakPtrFactory<WebAppInstallFinalizer> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_FINALIZER_H_
