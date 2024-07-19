// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace web_app {

struct WebAppInstallInfo;

// Documentation: docs/webapps/manifest_update_process.md
//
// Checks whether the installed web app associated with a given WebContents has
// out of date manifest data and what to update it to.
//
// High level procedure for this command:
// - Download new manifest data from site including external resources (such as
//   icon bitmaps).
// - Load existing manifest data from disk including external resources.
// - Diff manifest data.
// - Resolve any changes to app identity by confirming the change with the user,
//   silently allowing them, or reverting them.
// - Return back to the caller to schedule applying the changes back to disk.
class ManifestUpdateCheckCommand
    : public WebAppCommand<AppLock,
                           ManifestUpdateCheckResult,
                           std::unique_ptr<WebAppInstallInfo>>,
      public content::WebContentsObserver {
 public:
  // TODO(crbug.com/40254036): Merge ManifestUpdateDataFetchCommand and
  // ManifestUpdateFinalizeCommand into one so we don't have to return optional
  // early exit results to the caller.
  using CompletedCallback = base::OnceCallback<void(
      ManifestUpdateCheckResult check_result,
      std::unique_ptr<WebAppInstallInfo> new_install_info)>;

  ManifestUpdateCheckCommand(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> web_contents,
      CompletedCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<WebAppIconDownloader> icon_downloader);

  ~ManifestUpdateCheckCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Stage: Download the new manifest data
  // (ManifestUpdateCheckStage::kDownloadingNewManifestData).
  void DownloadNewManifestData(base::OnceClosure next_step_callback);
  void DownloadNewManifestJson(
      WebAppDataRetriever::CheckInstallabilityCallback next_step_callback);
  void StashNewManifestJson(base::OnceClosure next_step_callback,
                            blink::mojom::ManifestPtr opt_manifest,
                            bool valid_manifest_for_web_app,
                            webapps::InstallableStatusCode installable_status);
  void DownloadNewIconBitmaps(
      WebAppIconDownloader::WebAppIconDownloaderCallback next_step_callback);
  void StashNewIconBitmaps(base::OnceClosure next_step_callback,
                           IconsDownloadedResult result,
                           IconsMap icons_map,
                           DownloadedIconsHttpResults icons_http_results);

  void ValidateNewScopeExtensions(
      OnDidGetWebAppOriginAssociations next_step_callback);
  void StashValidatedScopeExtensions(
      base::OnceClosure next_step_callback,
      ScopeExtensions validated_scope_extensions);

  // Stage: Loading existing manifest data from disk.
  // (ManifestUpdateCheckStage::kLoadingExistingManifestData)
  void LoadExistingManifestData(base::OnceClosure next_step_callback);
  void LoadExistingAppIcons(
      WebAppIconManager::ReadIconBitmapsCallback next_step_callback);
  void StashExistingAppIcons(base::OnceClosure next_step_callback,
                             IconBitmaps icon_bitmaps);
  void LoadExistingShortcutsMenuIcons(
      WebAppIconManager::ReadShortcutsMenuIconsCallback next_step_callback);
  void StashExistingShortcutsMenuIcons(
      base::OnceClosure next_step_callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  // Stage: Comparing the existing and new manifest data.
  // (ManifestUpdateCheckStage::kComparingManifestData)
  void CompareManifestData(base::OnceClosure next_step_callback);

  // Stage: Resolving identity changes to app name and icons, deciding whether
  // to silently accept, require a user prompt or revert changes.
  // (ManifestUpdateCheckStage::kResolvingIdentityChanges)
  void ResolveIdentityChanges(base::OnceClosure next_step_callback);
  IdentityUpdateDecision MakeAppNameIdentityUpdateDecision() const;
  IdentityUpdateDecision MakeAppIconIdentityUpdateDecision() const;
  void RevertIdentityChangesIfNeeded();
  void ConfirmAppIdentityUpdate(base::OnceClosure next_step_callback);
  void OnIdentityUpdateConfirmationComplete(
      base::OnceClosure next_step_callback,
      AppIdentityUpdate app_identity_update_allowed);

  // Stage: Update check complete.
  // (ManifestUpdateCheckStage::kComplete)
  void CheckComplete();

  const WebApp& GetWebApp() const;

  bool IsWebContentsDestroyed();
  void CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult check_result);

  base::WeakPtr<ManifestUpdateCheckCommand> GetWeakPtr() {
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
  ManifestUpdateCheckStage stage_ = ManifestUpdateCheckStage::kPendingAppLock;

  base::WeakPtrFactory<ManifestUpdateCheckCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_H_
