// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_registration_waiter.h"

#include "base/test/bind_test_util.h"

namespace web_app {

WebAppRegistrationWaiter::WebAppRegistrationWaiter(PendingAppManager* manager)
    : manager_(manager) {
  manager_->SetRegistrationCallbackForTesting(base::BindLambdaForTesting(
      [this](const GURL& launch_url, RegistrationResultCode code) {
        CHECK_EQ(launch_url_, launch_url);
        CHECK_EQ(code_, code);
        run_loop_.Quit();
      }));
}

WebAppRegistrationWaiter::~WebAppRegistrationWaiter() {
  manager_->ClearRegistrationCallbackForTesting();
}

void WebAppRegistrationWaiter::AwaitNextRegistration(
    const GURL& launch_url,
    RegistrationResultCode code) {
  launch_url_ = launch_url;
  code_ = code;
  run_loop_.Run();
}

}  // namespace web_app
