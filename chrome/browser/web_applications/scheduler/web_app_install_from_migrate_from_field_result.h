// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

enum class WebAppInstallFromMigrateFromFieldResult {
  // The target app was already installed.
  kAlreadyInstalled,
  // The target app was not installed, and it was successfully installed with
  // the SUGGESTED_FROM_MIGRATION state.
  kSuccessInstalled,
  // No migration source app from the manifest's migrate_from field is
  // currently installed.
  kNoSourceAppInstalled,
  // The manifest could not be converted to WebAppInstallInfo.
  kManifestToWebAppInstallInfoError,
  // The install from info job failed.
  kInstallFromInfoFailed,
  // The manifest update job failed.
  kUpdateFailed,
  // The web contents was destroyed before the command could complete.
  kWebContentsWasDestroyed,
  // The user navigated away from the page before the command could complete.
  kUserNavigated,
  // The system is shutting down.
  kSystemShutdown,
};

std::ostream& operator<<(std::ostream& os,
                         WebAppInstallFromMigrateFromFieldResult result);

using WebAppInstallFromMigrateFromFieldCallback =
    base::OnceCallback<void(WebAppInstallFromMigrateFromFieldResult result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_RESULT_H_
