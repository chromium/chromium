// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GATHER_MIGRATION_SOURCE_INFO_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GATHER_MIGRATION_SOURCE_INFO_JOB_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/gather_migration_source_info_job_result.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WithAppResources;

// This job gathers information from a source application that is needed to
// migrate it to a new application. This includes the install state, user
// display mode, run on OS login mode, and shortcut locations.
class GatherMigrationSourceInfoJob {
 public:
  using Callback = base::OnceCallback<void(
      std::optional<GatherMigrationSourceInfoJobResult>)>;

  GatherMigrationSourceInfoJob(WithAppResources& lock,
                               const webapps::AppId& source_app_id,
                               const webapps::AppId& destination_app_id,
                               Callback callback);
  ~GatherMigrationSourceInfoJob();

  void Start();

 private:
  void OnShortcutInfoRetrieved(std::unique_ptr<ShortcutInfo> shortcut_info);
  void OnShortcutLocationGathered(ShortcutLocations locations);

  const raw_ref<WithAppResources> lock_;
  const webapps::AppId source_app_id_;
  const webapps::AppId destination_app_id_;
  Callback callback_;
  GatherMigrationSourceInfoJobResult result_;

  base::WeakPtrFactory<GatherMigrationSourceInfoJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GATHER_MIGRATION_SOURCE_INFO_JOB_H_
