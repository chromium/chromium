// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_V2_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_V2_H_

#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

struct WebAppInstallInfo;

// Used to uniquely identify an icon url for app updates.
struct SizeAndPurpose {
  gfx::Size size;
  IconPurpose purpose;

  bool operator<(const SizeAndPurpose& other) const;
  bool operator==(const SizeAndPurpose& other) const;

  struct absl_container_hash {
    using is_transparent = void;
    size_t operator()(const SizeAndPurpose& key) const;
  };
};

// Documentation: docs/webapps/manifest_update_process.md
//
// Checks whether the installed web app associated with a given WebContents has
// out of date manifest data and what to update it to.
//
// High level procedure for this command:
// - Download new manifest data from site including external resources (such as
//   icon bitmaps only if the url changes from the saved information).
// - Load existing manifest data from disk including external resources.
// - Diff manifest data.
// - Resolve any changes to app identity by confirming the change with the user,
//   silently allowing them, or reverting them.
// - Return back to the caller to schedule applying the changes back to disk.
//
// TODO(crbug.com/414851433): Rename this to ManifestUpdateCheckCommand and
// remove existing ManifestUpdateCheckCommand.
class ManifestUpdateCheckCommandV2
    : public WebAppCommand<AppLock,
                           ManifestUpdateCheckResult,
                           std::unique_ptr<WebAppInstallInfo>>,
      public content::WebContentsObserver {
 public:
  using CompletedCallback = base::OnceCallback<void(
      ManifestUpdateCheckResult check_result,
      std::unique_ptr<WebAppInstallInfo> new_install_info)>;

  ManifestUpdateCheckCommandV2(
      const GURL& url,
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> web_contents,
      CompletedCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<WebAppIconDownloader> icon_downloader);

  ~ManifestUpdateCheckCommandV2() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  // Keys an icon's size and purpose to its URL. This is only done for manifest
  // icons and do not include file handling, shortcut, or home tab icons. The
  // creation is done in the
  // ManifestUpdateCheckStage::kDownloadingNewManifestData and the
  // ManifestUpdateCheckStage::kLoadingExistingManifestData stage.
  absl::flat_hash_map<SizeAndPurpose, GURL> CreateIconSizeAndPurposeMap(
      const std::vector<apps::IconInfo>& icon_infos);

  // Stage: Download the new manifest data
  // (ManifestUpdateCheckStage::kDownloadingNewManifestData).
  void DownloadNewManifestData(base::OnceClosure next_step_callback);
  void DownloadNewManifestJson(
      WebAppDataRetriever::CheckInstallabilityCallback next_step_callback);
  void StashNewManifestJson(base::OnceClosure next_step_callback,
                            blink::mojom::ManifestPtr opt_manifest,
                            bool valid_manifest_for_web_app,
                            webapps::InstallableStatusCode installable_status);
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

  // Stage: Evaluates if icon bitmaps should be downloaded or if it already
  // exists from disk, then stashes the icon bitmaps.
  // (ManifestUpdateCheckStage::kDownloadingChangedIconUrlBitmaps)
  void DownloadChangedIconUrlBitmaps(base::OnceClosure next_step_callback);
  void DownloadNewIconBitmaps(
      WebAppIconDownloader::WebAppIconDownloaderCallback next_step_callback);
  void StashNewIconBitmaps(base::OnceClosure next_step_callback,
                           IconsDownloadedResult result,
                           IconsMap icons_map,
                           DownloadedIconsHttpResults icons_http_results);

  // Stage: Update check complete.
  // (ManifestUpdateCheckStage::kComplete)
  void CheckComplete();

  const WebApp& GetWebApp() const;

  bool IsWebContentsDestroyed();
  void CompleteCommandAndSelfDestruct(ManifestUpdateCheckResult check_result);

  base::WeakPtr<ManifestUpdateCheckCommandV2> GetWeakPtr() {
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
  absl::flat_hash_map<SizeAndPurpose, GURL> new_icon_size_and_purpose_map_;
  absl::flat_hash_map<SizeAndPurpose, GURL> existing_icon_size_and_purpose_map_;
  ManifestDataChanges manifest_data_changes_;

  // Debug info.
  ManifestUpdateCheckStage stage_ = ManifestUpdateCheckStage::kPendingAppLock;

  base::WeakPtrFactory<ManifestUpdateCheckCommandV2> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_UPDATE_CHECK_COMMAND_V2_H_
