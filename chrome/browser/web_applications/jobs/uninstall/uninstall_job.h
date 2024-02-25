// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_UNINSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_UNINSTALL_JOB_H_

#include "base/functional/callback_forward.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

class AllAppsLock;

// Common interface for uninstall related jobs used by WebAppUninstallCommand.
class UninstallJob {
 public:
  using Callback = base::OnceCallback<void(webapps::UninstallResultCode)>;

  virtual ~UninstallJob();

  virtual void Start(AllAppsLock& lock, Callback) = 0;
  virtual webapps::WebappUninstallSource uninstall_source() const = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_UNINSTALL_JOB_H_
