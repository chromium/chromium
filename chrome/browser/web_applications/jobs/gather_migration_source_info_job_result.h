// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GATHER_MIGRATION_SOURCE_INFO_JOB_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GATHER_MIGRATION_SOURCE_INFO_JOB_RESULT_H_

#include "base/values.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

struct GatherMigrationSourceInfoJobResult {
  GatherMigrationSourceInfoJobResult();
  ~GatherMigrationSourceInfoJobResult();
  GatherMigrationSourceInfoJobResult(const GatherMigrationSourceInfoJobResult&);
  GatherMigrationSourceInfoJobResult& operator=(
      const GatherMigrationSourceInfoJobResult&);
  GatherMigrationSourceInfoJobResult(GatherMigrationSourceInfoJobResult&&);
  GatherMigrationSourceInfoJobResult& operator=(
      GatherMigrationSourceInfoJobResult&&);

  base::Value ToDebugValue() const;

  proto::InstallState install_state;
  mojom::UserDisplayMode user_display_mode;
  RunOnOsLoginMode run_on_os_login_mode;
  ShortcutLocations shortcut_locations;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_GATHER_MIGRATION_SOURCE_INFO_JOB_RESULT_H_
