// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/external_app_registration_waiter.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

ExternalAppRegistrationWaiter::ExternalAppRegistrationWaiter(
    ExternallyManagedAppManager* manager)
    : manager_(manager) {
  manager_->SetRegistrationCallbackForTesting(base::BindLambdaForTesting(
      [this](const GURL& install_url, RegistrationResultCode code) {
        ASSERT_EQ(install_url_, install_url);
        if (code_)
          ASSERT_EQ(code_, code);
        else
          ASSERT_NE(code, RegistrationResultCode::kTimeout);
        run_loop_.Quit();
      }));
  manager_->SetRegistrationsCompleteCallbackForTesting(
      complete_run_loop_.QuitClosure());
}

ExternalAppRegistrationWaiter::~ExternalAppRegistrationWaiter() {
  manager_->ClearRegistrationCallbackForTesting();
}

void ExternalAppRegistrationWaiter::AwaitNextRegistration(
    const GURL& install_url,
    RegistrationResultCode code) {
  install_url_ = install_url;
  code_ = code;
  run_loop_.Run();
}

void ExternalAppRegistrationWaiter::AwaitNextNonFailedRegistration(
    const GURL& install_url) {
  install_url_ = install_url;
  code_ = std::nullopt;
  run_loop_.Run();
}

void ExternalAppRegistrationWaiter::AwaitRegistrationsComplete() {
  complete_run_loop_.Run();
}

}  // namespace web_app
