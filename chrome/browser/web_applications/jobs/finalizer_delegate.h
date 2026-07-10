// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZER_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZER_DELEGATE_H_

#include <optional>

#include "chrome/browser/web_applications/model/isolation_data.h"

namespace web_app {

class WebApp;
struct WebAppInstallInfo;

// Allows external subsystems to inject their own logic into the PWA core's
// FinalizeInstall and FinalizeUpdate jobs without adding a dependency to the
// PWA core.
class FinalizerDelegate {
 public:
  virtual ~FinalizerDelegate() = default;

  virtual void ConfigureCustomFields(WebApp* web_app,
                                     const WebAppInstallInfo& web_app_info) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZER_DELEGATE_H_
