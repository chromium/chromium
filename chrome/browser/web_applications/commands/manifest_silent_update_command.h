// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Not actually used in production logic. This is just for debugging output.
enum class ManifestSilentUpdateCommandStage {
  kFetchingNewManifestData,
  kLoadingExistingManifestData,
  kAcquiringAppLock,
  kComparingManifestData,
  kFinalizingSilentManifestChanges,
  kCompleteCommand,
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
  kMaxValue = kAppHasNonSecurityAndSecurityChanges,
};

struct WebAppInstallInfo;

// Documentation: docs/webapps/manifest_update_process.md
//
// Checks whether the installed web app associated with a given WebContents has
// out of date manifest data and what to update it to.
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

  ManifestSilentUpdateCommand(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      CompletedCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<WebAppIconDownloader> icon_downloader);

  ~ManifestSilentUpdateCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<NoopLock> lock) override;

 private:
  // Stage: Upgrade NoopLock to AppLock
  // (ManifestSilentUpdateCommandStage::kAcquiringAppLock).
  void OnManifestFetchedAcquireAppLock(
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode installable_status);

  // Stage: Starting to fetch new manifest data
  // (ManifestSilentUpdateCommandStage::kFetchingNewManifestData).
  void StartManifestToInstallInfoJob(blink::mojom::ManifestPtr opt_manifest);

  // The `install_info` will have icons populated if they were found in the
  // manifest.
  void OnWebAppInfoCreatedFromManifest(
      std::unique_ptr<WebAppInstallInfo> install_info);
  void StashValidatedScopeExtensionsAndLoadExistingManifest(
      ScopeExtensions validated_scope_extensions);

  // Stage: Loading existing manifest data from disk.
  // (ManifestSilentUpdateCommandStage::kLoadingExistingManifestData)
  void StashExistingAppIcons(IconBitmaps icon_bitmaps);

  // Stage: Comparing manifest data and exiting update if no changes detected.
  // (ManifestSilentUpdateCommandStage::kComparingManifestData)
  void StashExistingShortcutsMenuIconsFinalizeUpdateIfNeeded(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  // Stage: Finalize silent changes to web app.
  // (ManifestSilentUpdateCommandStage::kFinalizingSilentManifestChanges)
  void NonSecuritySensitiveFieldsApplied(
      bool silent_update_applied,
      std::optional<proto::PendingUpdateInfo> pending_update_info,
      const webapps::AppId& app_id,
      webapps::InstallResultCode code);

  // Stage: Update check complete.
  // (ManifestSilentUpdateCommandStage::kCompleteCommand)
  void CompleteCommandAndSelfDestruct(
      ManifestSilentUpdateCheckResult check_result);

  bool IsWebContentsDestroyed();
  void AbortCommandOnWebContentsDestruction();

  base::WeakPtr<ManifestSilentUpdateCommand> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Manifest update check request parameters.
  const GURL url_;
  webapps::AppId app_id_;

  // Resources and helpers used to fetch manifest data.
  std::unique_ptr<NoopLock> lock_;
  std::unique_ptr<AppLock> app_lock_;
  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppIconDownloader> icon_downloader_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  std::optional<apps::IconInfo> new_manifest_trusted_icon_;
  std::optional<apps::IconInfo> existing_manifest_trusted_icon_;
  bool has_icon_url_changed_;

  // Temporary variables stored here while the update check progresses
  // asynchronously.
  std::unique_ptr<WebAppInstallInfo> new_install_info_;
  IconBitmaps existing_app_icon_bitmaps_;
  ShortcutsMenuIconBitmaps existing_shortcuts_menu_icon_bitmaps_;

  // Debug info.
  ManifestSilentUpdateCommandStage stage_ =
      ManifestSilentUpdateCommandStage::kFetchingNewManifestData;

  base::WeakPtrFactory<ManifestSilentUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
