// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_SUB_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

struct SynchronizeOsOptions {
  // This option will always unregister all os integration, despite what may be
  // in the database. All other options will be ignored.
  // TODO(b/300628551): Investigate if this can be exposed as a separate API
  // instead of being called via Synchronize().
  bool force_unregister_os_integration = false;
  // Adds a shortcut to the desktop IFF this call to synchronize creates
  // shortcuts fresh for the given app (it's not an update).
  bool add_shortcut_to_desktop = false;
  // Adds a shortcut to the quick launch bar IFF this call to synchronize
  // creates shortcuts fresh for the given app (it's not an update).
  bool add_to_quick_launch_bar = false;
  // This is used for the "Create Shortcuts" option from App Home, allowing to
  // recreate shortcuts if they deleted them.
  bool force_create_shortcuts = false;
  // The reason synchronize is called, used to possibly show the location of the
  // shortcut to the user (this happen on Mac).
  ShortcutCreationReason reason = SHORTCUT_CREATION_AUTOMATED;
};

class OsIntegrationSubManager {
 public:
  OsIntegrationSubManager() = default;
  virtual ~OsIntegrationSubManager() = default;
  // desired_state can still be empty after the configure_done has completed
  // running.
  virtual void Configure(const webapps::AppId& app_id,
                         proto::WebAppOsIntegrationState& desired_state,
                         base::OnceClosure configure_done) = 0;
  virtual void Execute(
      const webapps::AppId& app_id,
      const absl::optional<SynchronizeOsOptions>& synchronize_options,
      const proto::WebAppOsIntegrationState& desired_state,
      const proto::WebAppOsIntegrationState& current_state,
      base::OnceClosure callback) = 0;

  // Only invoked if the app is not in the database and the caller set
  // force_unregister_os_integration to true. Intended to clean up stale OS
  // state that was left over from an unsuccessful uninstall or stale OS data.
  virtual void ForceUnregister(const webapps::AppId& app_id,
                               base::OnceClosure callback) = 0;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_SUB_MANAGER_H_
