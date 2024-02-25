// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_DEDUPE_INSTALL_URLS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_DEDUPE_INSTALL_URLS_COMMAND_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class RemoveInstallUrlJob;

// See WebAppCommandScheduler::ScheduleDedupeInstallUrls() for documentation.
class DedupeInstallUrlsCommand : public WebAppCommand<AllAppsLock> {
 public:
  static base::AutoReset<bool> ScopedSuppressForTesting();

  explicit DedupeInstallUrlsCommand(Profile& profile,
                                    base::OnceClosure completed_callback);
  ~DedupeInstallUrlsCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  void ProcessPendingJobsOrComplete();
  void JobComplete(webapps::UninstallResultCode code);
  void RecordMetrics();

  const raw_ref<Profile> profile_;

  std::unique_ptr<AllAppsLock> lock_;

  base::flat_map<GURL, base::flat_set<webapps::AppId>> install_url_to_apps_;
  base::flat_map<GURL, webapps::AppId> dedupe_choices_;

  std::vector<std::unique_ptr<RemoveInstallUrlJob>> pending_jobs_;
  std::unique_ptr<RemoveInstallUrlJob> active_job_;
  bool any_errors_ = false;

  base::WeakPtrFactory<DedupeInstallUrlsCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_DEDUPE_INSTALL_URLS_COMMAND_H_
