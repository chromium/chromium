// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Not actually used in production logic. This is just for debugging output.
enum class ManifestSilentUpdateCommandStage {
  kStartManifestDataFetching,
  kLoadingExistingManifestData,
  kCompleteCommand,
};

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCommandStage stage);

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
    : public WebAppCommand<AppLock,
                           ManifestUpdateCheckResult,
                           std::unique_ptr<WebAppInstallInfo>>,
      public content::WebContentsObserver {
 public:
  using CompletedCallback = base::OnceCallback<void(
      ManifestUpdateCheckResult check_result,
      std::unique_ptr<WebAppInstallInfo> new_install_info)>;

  ManifestSilentUpdateCommand(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> web_contents,
      CompletedCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<WebAppIconDownloader> icon_downloader);

  ~ManifestSilentUpdateCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  // Stage: Starting to fetch new manifest data
  // (ManifestSilentUpdateCommandStage::kStartManifestDataFetching).
  void StashNewManifestJson(blink::mojom::ManifestPtr opt_manifest,
                            bool valid_manifest_for_web_app,
                            webapps::InstallableStatusCode installable_status);
  void ValidateNewScopeExtensions();
  void StashValidatedScopeExtensions(
      ScopeExtensions validated_scope_extensions);

  // Stage: Loading existing manifest data from disk.
  // (ManifestSilentUpdateCommandStage::kLoadingExistingManifestData)
  void StashExistingAppIcons(IconBitmaps icon_bitmaps);
  void StashExistingShortcutsMenuIcons(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  // Stage: Update check complete.
  // (ManifestSilentUpdateCommandStage::kCompleteCommand)
  void CheckComplete();

  const WebApp& GetWebApp() const;

  bool IsWebContentsDestroyed();
  void CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult check_result);

  base::WeakPtr<ManifestSilentUpdateCommand> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Manifest update check request parameters.
  const GURL url_;
  const webapps::AppId app_id_;
  base::Time check_time_;

  // Resources and helpers used to fetch manifest data.
  std::unique_ptr<AppLock> lock_;
  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppIconDownloader> icon_downloader_;

  // Temporary variables stored here while the update check progresses
  // asynchronously.
  std::unique_ptr<WebAppInstallInfo> new_install_info_;
  IconBitmaps existing_app_icon_bitmaps_;
  ShortcutsMenuIconBitmaps existing_shortcuts_menu_icon_bitmaps_;
  ManifestDataChanges manifest_data_changes_;

  // Debug info.
  ManifestSilentUpdateCommandStage stage_ =
      ManifestSilentUpdateCommandStage::kStartManifestDataFetching;

  base::WeakPtrFactory<ManifestSilentUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
