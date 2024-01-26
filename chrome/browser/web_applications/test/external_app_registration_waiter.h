// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_EXTERNAL_APP_REGISTRATION_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_EXTERNAL_APP_REGISTRATION_WAITER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "url/gurl.h"

namespace web_app {

// Awaits service worker registrations for externally-managed web app installs.
class ExternalAppRegistrationWaiter {
 public:
  explicit ExternalAppRegistrationWaiter(ExternallyManagedAppManager* manager);
  ~ExternalAppRegistrationWaiter();

  void AwaitNextRegistration(const GURL& install_url,
                             RegistrationResultCode code);

  void AwaitNextNonFailedRegistration(const GURL& install_url);

  void AwaitRegistrationsComplete();

 private:
  const raw_ptr<ExternallyManagedAppManager> manager_;
  base::RunLoop run_loop_;
  GURL install_url_;
  // If unset then check for any non failure result.
  std::optional<RegistrationResultCode> code_;

  base::RunLoop complete_run_loop_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_EXTERNAL_APP_REGISTRATION_WAITER_H_
