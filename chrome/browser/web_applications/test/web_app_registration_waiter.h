// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_

#include "base/run_loop.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "url/gurl.h"

namespace web_app {

class WebAppRegistrationWaiter {
 public:
  explicit WebAppRegistrationWaiter(PendingAppManager* manager);
  ~WebAppRegistrationWaiter();

  void AwaitNextRegistration(const GURL& launch_url,
                             RegistrationResultCode code);

 private:
  PendingAppManager* const manager_;
  base::RunLoop run_loop_;
  GURL launch_url_;
  RegistrationResultCode code_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_REGISTRATION_WAITER_H_
