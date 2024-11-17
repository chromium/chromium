// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/google_auth_state_waiter_mixin.h"

#include "base/test/bind.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/supervised_user/core/browser/child_account_service.h"

namespace supervised_user {

GoogleAuthStateWaiterMixin::GoogleAuthStateWaiterMixin(
    InProcessBrowserTestMixinHost& test_mixin_host,
    InProcessBrowserTest* test_base,
    ChildAccountService::AuthState expected_auth_state)
    : InProcessBrowserTestMixin(&test_mixin_host),
      test_base_(test_base),
      expected_auth_state_(expected_auth_state) {}

GoogleAuthStateWaiterMixin::~GoogleAuthStateWaiterMixin() = default;

void GoogleAuthStateWaiterMixin::SetUpOnMainThread() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ChildAccountService* child_account_service =
      ChildAccountServiceFactory::GetForProfile(
          test_base_->browser()->profile());

  // Handle the case where the auth state was already as expected.
  if (child_account_service->GetGoogleAuthState() == expected_auth_state_) {
    return;
  }

  // Observe auth state changes.
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      child_account_service->ObserveGoogleAuthState(
          base::BindLambdaForTesting([&]() {
            if (child_account_service->GetGoogleAuthState() ==
                expected_auth_state_) {
              run_loop.Quit();
            }
          }));

  // Wait for the auth state to change.
  run_loop.Run();
#endif
}

}  // namespace supervised_user
