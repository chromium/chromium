// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/browser/web_contents_observer.h"
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

struct IconDiff;

// This enum is recorded by UMA, the numeric values must not change.
enum ManifestUpdateResult {
  kNoAppInScope = 0,
  kThrottled = 1,
  kWebContentsDestroyed = 2,
  kAppUninstalling = 3,
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

enum IconDiffResult : uint32_t {
  NO_CHANGE_DETECTED = 0,

  // A mismatch was detected between what was downloaded and what is on disk.
  // This might mean that a size has been removed or added, and it could mean
  // both.
  MISMATCHED_IMAGE_SIZES = 1 << 1,

  // At least one icon was found to have changed. Note: Used only if the diff
  // process stops when it encounters the first mismatch. If, instead, it is
  // allowed to continue, a more detailed results will be returned (see flags
  // below).
  ONE_OR_MORE_ICONS_CHANGED = 1 << 2,

  // The launcher icon is changing. Note: that the launcher icon size is
  // platform-specific and that this flag is only set if the diff process is
  // allowed to continue to the end (doesn't stop as soon as it finds a
  // change).
  LAUNCHER_ICON_CHANGED = 1 << 3,

  // The launcher icon is changing. Note: that the install icon size is
  // platform-specific and that this flag is only set if the diff process is
  // allowed to continue to the end (doesn't stop as soon as it finds a
  // change).
  INSTALL_ICON_CHANGED = 1 << 4,

  // An icon, other than the launcher/install icon changed. Note: that this flag
  // is only set if the diff process is allowed to continue to the end (doesn't
  // stop as soon as it finds a change).
  UNIMPORTANT_ICON_CHANGED = 1 << 5,
};

// A structure to keep track of the differences found while comparing icons
// on disk to what has been downloaded.
struct IconDiff {
 public:
  IconDiff() = default;
  explicit IconDiff(uint32_t results) { diff_results = results; }
  IconDiff(const SkBitmap& before_icon,
           const SkBitmap& after_icon,
           uint32_t results) {
    before = before_icon;
    after = after_icon;
    diff_results = results;
  }

  // Returns true iff an icon change was detected (not matter how
  // insignificant).
  bool mismatch() const { return diff_results != NO_CHANGE_DETECTED; }

  // Returns true iff the mismatch should result in app identity dlg being
  // shown.
  bool requires_app_identity_check() const {
    return ((diff_results & LAUNCHER_ICON_CHANGED) != 0) ||
           ((diff_results & INSTALL_ICON_CHANGED) != 0);
  }

  // Keeps track of all the differences discovered in the icon set.
  uint32_t diff_results = NO_CHANGE_DETECTED;

  // The original image. Only valid if a single icon is changing.
  SkBitmap before;

  // The changed image. Only valid if a single icon is changing.
  SkBitmap after;
};

// Returns whether any differences were found in the images on disk and what has
// been downloaded. The |disk_icon_bitmaps| and |disk_icon_info| parameters
// represent the bits on disk and the associated size info (respectively). Same
// with |downloaded_icon_bitmaps| and |downloaded_icon_info|, which covers the
// downloaded icon set. If |end_when_mismatch_detected| is true, the diff
// process will stop when it encounters the first mismatch. Otherwise, it the
// IconDiff returned will cover all the differences found.
IconDiff HaveIconBitmapsChanged(
    const IconBitmaps& disk_icon_bitmaps,
    const IconBitmaps& downloaded_icon_bitmaps,
    const std::vector<apps::IconInfo>& disk_icon_info,
    const std::vector<apps::IconInfo>& downloaded_icon_info,
    bool end_when_mismatch_detected);

// Checks whether the installed web app associated with a given WebContents has
// out of date manifest data and triggers an update if so.
// Owned and managed by |ManifestUpdateManager|.
//
// High level check procedure:
//  - Wait for page to load.
//  - Load the page's manifest. Abort if none found.
//  - Check a hard coded set of manifest fields for differences to what's stored
//    locally. Abort if no differences.
//  - Check if the app ID has changed, abort if so.
//  - Require user confirmation for changes to the app name.
//  - Wait for all app windows to be closed.
//  - Reinstall the web app using the fetched data.
class ManifestUpdateTask final
    : public base::SupportsWeakPtr<ManifestUpdateTask>,
      public content::WebContentsObserver {
 public:
  using UpdatePendingCallback = base::OnceCallback<void(const GURL& url)>;
  using StoppedCallback = base::OnceCallback<void(const ManifestUpdateTask&,
                                                  ManifestUpdateResult result)>;
  // Sets a |callback| for testing code to get notified when a manifest update
  // is needed and there is a PWA window preventing the update from proceeding.
  // Only called once, iff the update process determines that waiting is needed.
  static void SetUpdatePendingCallbackForTesting(
      UpdatePendingCallback callback);

  static bool& BypassWindowCloseWaitingForTesting();

  ManifestUpdateTask(const GURL& url,
                     const AppId& app_id,
                     content::WebContents* web_contents,
                     StoppedCallback stopped_callback,
                     bool hang_for_testing,
                     WebAppRegistrar& registrar,
                     WebAppIconManager& icon_manager,
                     WebAppUiManager* ui_manager,
                     WebAppInstallFinalizer* install_finalizer,
                     OsIntegrationManager& os_integration_manager,
                     WebAppSyncBridge* sync_bridge);

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
    kPendingAppIdentityCheck,
    kPendingWindowsClosed,
    kPendingMaybeReadExistingIcons,
    kPendingInstallation,
    kPendingAssociationsUpdate,
  };

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

  WebAppRegistrar& registrar_;
  WebAppIconManager& icon_manager_;
  WebAppUiManager& ui_manager_;
  WebAppInstallFinalizer& install_finalizer_;
  OsIntegrationManager& os_integration_manager_;
  raw_ptr<WebAppSyncBridge> sync_bridge_ = nullptr;

  Stage stage_;
  absl::optional<WebAppInstallInfo> install_info_;
  absl::optional<WebAppIconDownloader> icon_downloader_;

  // Two KeepAlive objects, to make sure in progress manifest updates survive
  // during shutdown.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  const GURL url_;
  const AppId app_id_;
  StoppedCallback stopped_callback_;
  bool hang_for_testing_ = false;
  bool app_identity_update_allowed_ = false;

#if DCHECK_IS_ON()
  raw_ptr<bool> destructor_called_ptr_ = nullptr;
#endif
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_TASK_H_
