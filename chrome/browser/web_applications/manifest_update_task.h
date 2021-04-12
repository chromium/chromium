// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_

#include <map>

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/manifest/manifest.h"

struct WebApplicationInfo;

namespace webapps {
struct InstallableData;
}

namespace web_app {

// Checks for whether file handlers have changed. Ignores differences in names,
// which aren't stored in the apps::FileHandlers, and ordering, which may
// change after being inserted into a set or map.
bool HaveFileHandlersChanged(
    const apps::FileHandlers* old_handlers,
    const std::vector<blink::Manifest::FileHandler>& new_handlers);

class AppIconManager;
class AppRegistrar;
class WebAppUiManager;
class InstallManager;
class OsIntegrationManager;
enum class InstallResultCode;

// This enum is recorded by UMA, the numeric values must not change.
enum ManifestUpdateResult {
  kNoAppInScope = 0,
  kThrottled = 1,
  kWebContentsDestroyed = 2,
  kAppUninstalled = 3,
  kAppIsPlaceholder = 4,
  kAppUpToDate = 5,
  kAppNotEligible = 6,
  kAppUpdateFailed = 7,
  kAppUpdated = 8,
  kAppIsSystemWebApp = 9,
  kIconDownloadFailed = 10,
  kIconReadFromDiskFailed = 11,
  kAppIdMismatch = 12,
  kAppAssociationsUpdateFailed = 13,
  kAppAssociationsUpdated = 14,
  kMaxValue = kAppAssociationsUpdated,
};

// Checks whether the installed web app associated with a given WebContents has
// out of date manifest data and triggers an update if so.
// Owned and managed by |ManifestUpdateManager|.
//
// High level check procedure:
//  - Wait for page to load.
//  - Load the page's manifest. Bail if none found.
//  - Check a hard coded set of manifest fields for differences to what's stored
//    locally. Bail if no differences.
//  - Ignore changes to the app name (needs user approval to prevent phishing).
//  - Ignore changes to the start_url (this would change the app's ID which
//    would play havoc with device sync, we need to decouple the app ID from
//    start_url to enable start_url updating).
//  - Wait for all app windows to be closed.
//  - Reinstall the web app using the fetched data.
class ManifestUpdateTask final
    : public base::SupportsWeakPtr<ManifestUpdateTask>,
      public content::WebContentsObserver {
 public:
  using StoppedCallback = base::OnceCallback<void(const ManifestUpdateTask&,
                                                  ManifestUpdateResult result)>;

  ManifestUpdateTask(const GURL& url,
                     const AppId& app_id,
                     content::WebContents* web_contents,
                     StoppedCallback stopped_callback,
                     bool hang_for_testing,
                     const AppRegistrar& registrar,
                     const AppIconManager& icon_manager,
                     WebAppUiManager* ui_manager,
                     InstallManager* install_manager,
                     OsIntegrationManager& os_integration_manager);

  ~ManifestUpdateTask() override;

  const GURL& url() const { return url_; }
  const AppId& app_id() const { return app_id_; }

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

 private:
  enum class Stage {
    kPendingPageLoad,
    kPendingInstallableData,
    kPendingIconDownload,
    kPendingIconReadFromDisk,
    kPendingWindowsClosed,
    kPendingMaybeReadExistingIcons,
    kPendingInstallation,
    kPendingAssociationsUpdate,
  };

  void OnDidGetInstallableData(const webapps::InstallableData& data);
  bool IsUpdateNeededForManifest() const;
  void LoadAndCheckIconContents();
  void OnIconsDownloaded(bool success, IconsMap icons_map);
  void OnAllIconsRead(IconsMap downloaded_icons_map,
                      IconBitmaps disk_icon_bitmaps);
  bool IsUpdateNeededForIconContents(
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
  void OnInstallationComplete(
      const AppId& app_id,
      InstallResultCode code);
  void DestroySelf(ManifestUpdateResult result);

  const AppRegistrar& registrar_;
  const AppIconManager& icon_manager_;
  WebAppUiManager& ui_manager_;
  InstallManager& install_manager_;
  OsIntegrationManager& os_integration_manager_;

  Stage stage_;
  base::Optional<WebApplicationInfo> web_application_info_;
  base::Optional<WebAppIconDownloader> icon_downloader_;

  const GURL url_;
  const AppId app_id_;
  StoppedCallback stopped_callback_;
  bool hang_for_testing_ = false;

#if DCHECK_IS_ON()
  bool* destructor_called_ptr_ = nullptr;
#endif
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_
