// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MANIFEST_UPDATE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MANIFEST_UPDATE_JOB_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/jobs/finalize_update_job.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job_result.h"
#include "chrome/browser/web_applications/model/web_app_comparison.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class DictValue;
}

namespace content {
class WebContents;
}

class Profile;

namespace web_app {

class ManifestToWebAppInstallInfoJob;
class WithAppResources;
class WebAppDataRetriever;
struct WebAppInstallInfo;

// Checks whether there is any update to the manifest of the web app, and if so,
// performs the update.
//
// This job compares the fetched manifest with the current state of the app.
// - If there are no changes, the job completes with kNoUpdateNeeded.
// - If there are changes that can be applied silently (e.g. non-identity
//   changes, or identity changes if allowed by policy/options), the app is
//   updated and the job completes with kSilentlyUpdated.
// - If there are identity changes (name, icon) that require user confirmation,
//   the job writes the new info to the pending_update_info field in the
//   database and completes with kPendingUpdateRecorded_*.
//
// This job does NOT prompt the user for applying the pending update. It only
// prepares and saves the pending update (if there is one).
class ManifestUpdateJob {
 public:
  using Result = ManifestUpdateJobResult;

  struct Options {
    Options() = default;
    // If true, the job will not download icons if the manifest icons metadata
    // has not changed from the current install state.
    bool skip_icon_download_if_no_manifest_change = true;
    // The last time a silent non-visually-different icon update was applied.
    // If set, then the job will only change a different but
    // non-visually-different icon update to a silent update if the last time
    // this happened was more than 24 hours ago.
    std::optional<base::Time> previous_time_for_silent_icon_update;
    // If true, the job will not generate icons if there are not icon urls
    // present.
    bool bypass_icon_generation_if_no_url = false;
    // If true, the job will fail the operation if any icon download fails.
    bool fail_if_any_icon_download_fails = false;
    // If true, the job will record UMA metrics on icon download results.
    bool record_icon_results_on_update = false;
    // If true, the job will consider all manifest icons as trusted icons,
    // instead of selecting one primary icon.
    bool use_manifest_icons_as_trusted = false;
    // If true, the job will silently update everything, including the name &
    // icon.
    bool force_silent_update_identity = false;
  };

  // Creates the job and starts it right away. The `callback` is guaranteed to
  // be called asynchronously when the job completes.
  static std::unique_ptr<ManifestUpdateJob> CreateAndStart(
      Profile& profile,
      Lock* lock,
      WithAppResources* lock_resources,
      content::WebContents* web_contents,
      base::DictValue* debug_value,
      blink::mojom::ManifestPtr manifest,
      WebAppDataRetriever* data_retriever,
      base::Clock* clock,
      ManifestUpdateJobCallback callback,
      Options options);

  ~ManifestUpdateJob();

 private:
  ManifestUpdateJob(Profile& profile,
                    Lock* lock,
                    WithAppResources* lock_resources,
                    content::WebContents* web_contents,
                    base::DictValue* debug_value,
                    blink::mojom::ManifestPtr manifest,
                    WebAppDataRetriever* data_retriever,
                    base::Clock* clock,
                    ManifestUpdateJobCallback callback,
                    Options options);
  void Start();

  void OnWebAppInfoCreated(std::unique_ptr<WebAppInstallInfo> install_info);
  void OnExistingIconsLoaded(WebAppIconManager::WebAppBitmaps icon_bitmaps);
  void OnExistingShortcutsMenuIconsLoaded(
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);
  void ContinueToFetchIcons();
  void OnIconsFetched();
  void FinalizeUpdateIfSilentChangesExist();
  void OnInstallUpdateJobFinished(FinalizeUpdateJob* job,
                                  InstallFinalizedCallback callback,
                                  const webapps::AppId& app_id,
                                  webapps::InstallResultCode code);
  void UpdateFinalizedWritePendingInfo(
      std::optional<proto::PendingUpdateInfo> pending_update_info,
      bool silent_icon_update_happened,
      const webapps::AppId& app_id,
      webapps::InstallResultCode code);
  void WritePendingUpdateInfoThenComplete(
      std::optional<proto::PendingUpdateInfo> pending_update,
      Result result);
  void WritePendingUpdateToWebAppUpdateObservers(
      std::optional<proto::PendingUpdateInfo> pending_update);
  void Complete(Result result);
  bool IsWebContentsDestroyed();

  const raw_ref<Profile> profile_;
  const raw_ref<Lock> lock_;
  const raw_ref<WithAppResources> lock_resources_;
  const base::WeakPtr<content::WebContents> web_contents_;
  const raw_ref<base::DictValue> debug_value_;
  const blink::mojom::ManifestPtr manifest_;
  const raw_ref<WebAppDataRetriever> data_retriever_;
  const raw_ref<base::Clock> clock_;
  const webapps::AppId app_id_;
  ManifestUpdateJobCallback callback_;
  const Options options_;

  std::unique_ptr<WebAppInstallInfo> new_install_info_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  std::unique_ptr<FinalizeUpdateJob> install_update_job_;

  WebAppComparison web_app_comparison_;
  IconBitmaps existing_manifest_icon_bitmaps_;
  IconBitmaps existing_trusted_icon_bitmaps_;
  ShortcutsMenuIconBitmaps existing_shortcuts_menu_icon_bitmaps_;

  IconBitmaps pending_trusted_icon_bitmaps_;
  IconBitmaps pending_manifest_icon_bitmaps_;

  bool silent_update_required_ = false;
  bool silently_update_app_identity_ = false;
  std::optional<base::Time> time_for_icon_diff_check_;

  base::WeakPtrFactory<ManifestUpdateJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MANIFEST_UPDATE_JOB_H_
