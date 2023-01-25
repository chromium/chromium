// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_DATA_FETCH_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_DATA_FETCH_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class WebAppDataRetriever;

enum class AppIdentityUpdate;
enum class IconsDownloadedResult;

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

  // An icon, other than the launcher/install icon changed. Note: that this
  // flag is only set if the diff process is allowed to continue to the end
  // (doesn't stop as soon as it finds a change).
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

// Returns whether any differences were found in the images on disk and what
// has been downloaded. The |disk_icon_bitmaps| and |disk_icon_info| parameters
// represent the bits on disk and the associated size info (respectively).
// Same with |downloaded_icon_bitmaps| and |downloaded_icon_info|, which covers
// the downloaded icon set. If |end_when_mismatch_detected| is true, the diff
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
//
// High level procedure for this command:
//  - Load the page's manifest. Abort if none found.
//  - Check a hard coded set of manifest fields for differences to what's stored
//    locally. Abort if no differences.
//  - Check if the app ID has changed, abort if so.
//  - Require user confirmation for changes to the app name.
class ManifestUpdateDataFetchCommand : public WebAppCommandTemplate<AppLock> {
 public:
  // If no `early_exit_result` is provided then the manifest should be updated
  // with `install_info`.
  // TODO(crbug.com/1409710): Merge ManifestUpdateDataFetchCommand and
  // ManifestUpdateFinalizeCommand into one so we don't have to return optional
  // early exit results to the caller.
  using ManifestFetchCallback = base::OnceCallback<void(
      absl::optional<ManifestUpdateResult> early_exit_result,
      absl::optional<WebAppInstallInfo> install_info,
      bool app_identity_update_allowed)>;

  ManifestUpdateDataFetchCommand(
      const GURL& url,
      const AppId& app_id,
      base::WeakPtr<content::WebContents> web_contents,
      ManifestFetchCallback fetch_callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever);

  ~ManifestUpdateDataFetchCommand() override;

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  void OnSyncSourceRemoved() override {}
  void OnShutdown() override;
  base::Value ToDebugValue() const override;
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

  base::WeakPtr<ManifestUpdateDataFetchCommand> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool IsWebContentsDestroyed();
  void OnDidGetInstallableData(blink::mojom::ManifestPtr opt_manifest,
                               const GURL& manifest_url,
                               bool valid_manifest_for_web_app,
                               webapps::InstallableStatusCode error_code);
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
  void NoManifestUpdateRequired();
  void OnExistingIconsRead(IconBitmaps icon_bitmaps);
  void CompleteCommand(absl::optional<ManifestUpdateResult> early_exit_result);

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  const GURL url_;
  const AppId app_id_;
  base::WeakPtr<content::WebContents> web_contents_;
  ManifestFetchCallback fetch_callback_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  ManifestUpdateStage stage_ = ManifestUpdateStage::kPendingInstallableData;
  absl::optional<WebAppInstallInfo> install_info_;
  absl::optional<WebAppIconDownloader> icon_downloader_;

  bool app_identity_update_allowed_ = false;
  base::Value::Dict debug_log_;

  base::WeakPtrFactory<ManifestUpdateDataFetchCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_DATA_FETCH_COMMAND_H_
