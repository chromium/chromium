// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job_result.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace content {
class WebContents;
class Page;
}  // namespace content

namespace web_app {

class AppLock;
class ManifestUpdateJob;
class NoopLock;
class WebAppDataRetriever;

// Not actually used in production logic. This is just for debugging output.
enum class ManifestSilentUpdateCommandStage {
  kNotStarted,
  kFetchingNewManifestData,
  kAcquiringAppLock,
  kManifestUpdateJob,
};

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
    : public WebAppCommand<NoopLock, ManifestSilentUpdateCompletionInfo>,
      public content::WebContentsObserver {
 public:
  using CompletedCallback = ManifestSilentUpdateCallback;

  ManifestSilentUpdateCommand(
      content::WebContents& web_contents,
      std::optional<base::Time> previous_time_for_silent_icon_update,
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

  void OnAppLockAcquired(blink::mojom::ManifestPtr manifest);
  void OnUpdateJobCompleted(ManifestUpdateJobResultWithTimestamp result_info);

  void CompleteCommandAndSelfDestruct(
      base::Location location,
      ManifestSilentUpdateCheckResult check_result);

  bool IsWebContentsDestroyed();

  base::WeakPtr<ManifestSilentUpdateCommand> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

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

  std::unique_ptr<ManifestUpdateJob> manifest_update_job_;

  // Debug info.
  ManifestSilentUpdateCommandStage stage_ =
      ManifestSilentUpdateCommandStage::kFetchingNewManifestData;

  // Stores the last time a silent icon update was triggered for `app_id_` if
  // that happened.
  std::optional<base::Time> previous_time_for_silent_icon_update_;
  ManifestSilentUpdateCompletionInfo completion_info_;

  base::WeakPtrFactory<ManifestSilentUpdateCommand> weak_factory_{this};
};

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult stage);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_MANIFEST_SILENT_UPDATE_COMMAND_H_
