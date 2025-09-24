// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
namespace proto {
class PendingUpdateInfo;
}  // namespace proto

class AppLock;
class NoopLock;
class ManifestToWebAppInstallInfoJob;

// Not actually used in production logic. This is just for debugging output.
enum class ManifestSilentUpdateCommandStage {
  kNotStarted,
  kFetchingNewManifestData,
  kLoadingExistingManifestData,
  kAcquiringAppLock,
  kComparingManifestData,
  kFinalizingSilentManifestChanges,
  kWritingPendingUpdateIconBitmapsToDisk,
};

// This enum is recorded by UMA, the numeric values must not change.
enum class ManifestSilentUpdateCheckResult {
  kAppNotInstalled = 0,
  kAppUpdateFailedDuringInstall = 1,
  kSystemShutdown = 2,
  kAppSilentlyUpdated = 3,
  kAppUpToDate = 4,
  kIconReadFromDiskFailed = 5,
  kWebContentsDestroyed = 6,
  kAppOnlyHasSecurityUpdate = 7,
  kAppHasNonSecurityAndSecurityChanges = 8,
  kPendingIconWriteToDiskFailed = 9,
  kInvalidManifest = 10,
  kInvalidPendingUpdateInfo = 11,
  kUserNavigated = 12,
  kMaxValue = kUserNavigated,
};

// Declare the logging operator before the command declaration, so the templated
// completion method can use it to log the result.
std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult stage);

// Downloads a currently linked manifest in the given web contents. Non-security
// -sensitive manifest members are updated immediately. Security sensitive
// changes are saved in the WebApp's PendingUpdateInfo.
//
// Invariants:
// - This command assumes that the load for the given web contents has been
//  completed, and the manifest is already linked.
//
// High level procedure for this command:
// - Download new manifest data from site.
// - Load existing manifest data from disk including external resources.
// - Diff the non-security sensitive manifest data. This includes all fields of
//   the manifest excluding icons and app name.
// - Update non-security sensitive fields silently.
// - Choose two golden icons (one each from the new and existing manifest).
// - Compare their icon's URL which determines a silent update of the icon (<10%
//   image diff) or store it as a PendingUpdateInfo (>10% image diff).
// - Finalize silent update of icon (if needed) and destroy command.
class ManifestSilentUpdateCommand
    : public WebAppCommand<NoopLock, ManifestSilentUpdateCheckResult>,
      public content::WebContentsObserver {
 public:
  using CompletedCallback =
      base::OnceCallback<void(ManifestSilentUpdateCheckResult check_result)>;

  ManifestSilentUpdateCommand(content::WebContents& web_contents,
                              CompletedCallback callback);

  ~ManifestSilentUpdateCommand() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<NoopLock> lock) override;

 private:
  void SetStage(ManifestSilentUpdateCommandStage stage);

  void OnManifestFetchedAcquireAppLock(
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode installable_status);

  void StartManifestToInstallInfoJob(blink::mojom::ManifestPtr opt_manifest);

  // The `install_info` will have icons populated if they were found in the
  // manifest.
  void OnWebAppInfoCreatedFromManifest(
      std::unique_ptr<WebAppInstallInfo> install_info);

  void FinalizeUpdateIfSilentChangesExist();

  void UpdateFinalizedWritePendingInfoIfNeeded(
      std::optional<proto::PendingUpdateInfo> pending_update_info,
      const webapps::AppId& app_id,
      webapps::InstallResultCode code);

  void CompleteCommandAndSelfDestruct(
      ManifestSilentUpdateCheckResult check_result);

  bool IsWebContentsDestroyed();

  base::WeakPtr<ManifestSilentUpdateCommand> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Loads `existing_trusted_icon_bitmaps_`, `existing_manifest_icon_bitmaps_`,
  // and `existing_shortcuts_menu_icon_bitmaps_`, and then calls `on_complete`.
  void LoadExistingAppAndShortcutIcons(base::OnceClosure on_complete);
  void OnAppIconsLoaded(WebAppIconManager::WebAppBitmaps icon_bitmaps);
  void OnShortcutIconsLoaded(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  // Manifest update check request parameters.
  webapps::AppId app_id_;

  // Populated when the command should fail, but the command hasn't started yet.
  // Used for when the attached page is navigated or changed, so the manifest
  // cannot be loaded from here.
  std::optional<ManifestSilentUpdateCheckResult> failed_before_start_;
  // Resources and helpers used to fetch manifest data.
  std::unique_ptr<NoopLock> lock_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<AppLock> app_lock_;

  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  std::optional<apps::IconInfo> new_manifest_trusted_icon_metadata_;
  std::optional<apps::IconInfo> existing_manifest_trusted_icon_metadata_;

  // Temporary variables stored here while the update check progresses
  // asynchronously.
  std::unique_ptr<WebAppInstallInfo> new_install_info_;
  IconBitmaps existing_manifest_icon_bitmaps_;
  IconBitmaps existing_trusted_icon_bitmaps_;
  IconBitmaps pending_trusted_icon_bitmaps_;
  IconBitmaps pending_manifest_icon_bitmaps_;
  ShortcutsMenuIconBitmaps existing_shortcuts_menu_icon_bitmaps_;
  bool has_icon_url_changed_ = false;
  bool silent_update_required_ = false;
  bool pending_updated_added_ = false;

  // Debug info.
  ManifestSilentUpdateCommandStage stage_ =
      ManifestSilentUpdateCommandStage::kFetchingNewManifestData;

  base::WeakPtrFactory<ManifestSilentUpdateCommand> weak_factory_{this};
};

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult stage);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
