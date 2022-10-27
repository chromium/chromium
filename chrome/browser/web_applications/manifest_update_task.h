// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webapps {
struct InstallableData;
enum class InstallResultCode;
}

namespace web_app {

class WebAppIconManager;
class WebAppInstallFinalizer;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppUiManager;
class OsIntegrationManager;

enum class AppIdentityUpdate;
enum class IconsDownloadedResult;

// Checks whether the installed web app associated with a given WebContents
// has out of date manifest data and triggers an update if so.
// Owned and managed by |ManifestUpdateManager|.
//
// High level check procedure:
//  - Load the page's manifest. Abort if none found.
//  - Check a hard coded set of manifest fields for differences to what's stored
//    locally. Abort if no differences.
//  - Check if the app ID has changed, abort if so.
//  - Require user confirmation for changes to the app name.
//  - Wait for all app windows to be closed.
//  - Reinstall the web app using the fetched data.
class ManifestUpdateTask final
    : public base::SupportsWeakPtr<ManifestUpdateTask> {
 public:
  using UpdatePendingCallback = base::OnceCallback<void(const GURL& url)>;
  using StoppedCallback = base::OnceCallback<void(const ManifestUpdateTask&,
                                                  ManifestUpdateResult result)>;
  // Sets a |callback| for testing code to get notified when a manifest
  // update is needed and there is a PWA window preventing the update from
  // proceeding. Only called once, iff the update process determines that
  // waiting is needed.
  static void SetUpdatePendingCallbackForTesting(
      UpdatePendingCallback callback);

  static bool& BypassWindowCloseWaitingForTesting();

  ManifestUpdateTask(const GURL& url,
                     const AppId& app_id,
                     base::WeakPtr<content::WebContents> web_contents,
                     StoppedCallback stopped_callback,
                     WebAppRegistrar& registrar,
                     WebAppIconManager& icon_manager,
                     WebAppUiManager* ui_manager,
                     WebAppInstallFinalizer* install_finalizer,
                     OsIntegrationManager& os_integration_manager,
                     WebAppSyncBridge* sync_bridge);

  ~ManifestUpdateTask();
  void Start();

 private:
  // We perform this check for the following ManifestUpdateStage values:
  // kPendingInstallableData
  // kPendingIconDownload
  // kPendingIconReadFromDisk
  // kPendingAppIdentityCheck
  // For all other stages, we no longer need to observe the web_contents
  // and the upgrade can proceed as expected.
  bool IsWebContentsDestroyed();
  void OnDidGetInstallableData(const webapps::InstallableData& data);
  bool IsUpdateNeededForManifest() const;
  void LoadAndCheckIconContents();
  void OnIconsDownloaded(IconsDownloadedResult result,
                         IconsMap icons_map,
                         DownloadedIconsHttpResults icons_http_results);
  void OnAllIconsRead(IconsMap downloaded_icons_map,
                      IconBitmaps disk_icon_bitmaps);
  void OnPostAppIdentityUpdateCheck(
      AppIdentityUpdate app_identity_update_allowed);
  IconDiff IsUpdateNeededForIconContents(
      const IconBitmaps& disk_icon_bitmaps) const;
  void OnAllShortcutsMenuIconsRead(
      ShortcutsMenuIconBitmaps disk_shortcuts_menu_icons);
  bool IsUpdateNeededForShortcutsMenuIconsContents(
      const ShortcutsMenuIconBitmaps& disk_shortcuts_menu_icons) const;
  bool IsUpdateNeededForWebAppOriginAssociations() const;
  void NoManifestUpdateRequired();
  void OnWebAppOriginAssociationsUpdated(bool success);
  void UpdateAfterWindowsClose();
  void OnAllAppWindowsClosed();
  void OnExistingIconsRead(IconBitmaps icon_bitmaps);
  void OnInstallationComplete(const AppId& app_id,
                              webapps::InstallResultCode code,
                              OsHooksErrors os_hooks_errors);
  void DestroySelf(ManifestUpdateResult result);

  base::WeakPtr<content::WebContents> web_contents_;
  WebAppRegistrar& registrar_;
  WebAppIconManager& icon_manager_;
  WebAppUiManager& ui_manager_;
  WebAppInstallFinalizer& install_finalizer_;
  OsIntegrationManager& os_integration_manager_;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;

  ManifestUpdateStage stage_;
  absl::optional<WebAppInstallInfo> install_info_;
  absl::optional<WebAppIconDownloader> icon_downloader_;

  // Two KeepAlive objects, to make sure in progress manifest updates survive
  // during shutdown.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  const GURL url_;
  const AppId app_id_;
  StoppedCallback stopped_callback_;
  bool app_identity_update_allowed_ = false;

#if DCHECK_IS_ON()
  raw_ptr<bool> destructor_called_ptr_ = nullptr;
#endif
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_
