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
  // TODO(b/364009851): this currently doesn't work on Windows and some Mac
  // builders, because the network is not fully initialized by the point
  // `SetUpOnMainThread()` is called.
  // As a workaround, on these platforms there is a call to Wait in the test
  // body.
#if BUILDFLAG(IS_LINUX)
  ChildAccountService* child_account_service =
      ChildAccountServiceFactory::GetForProfile(
          test_base_->browser()->profile());
  WaitForGoogleAuthState(child_account_service, expected_auth_state_);
#else
  return;
#endif
}

// static
void GoogleAuthStateWaiterMixin::WaitForGoogleAuthState(
    ChildAccountService* child_account_service,
    ChildAccountService::AuthState expected_auth_state) {
  CHECK(child_account_service);

  // Handle the case where the auth state was already as expected.
  if (child_account_service->GetGoogleAuthState() == expected_auth_state) {
    return;
  }

  // Observe auth state changes.
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  base::CallbackListSubscription subscription =
      child_account_service->ObserveGoogleAuthState(
          base::BindLambdaForTesting([&]() {
            if (child_account_service->GetGoogleAuthState() ==
                expected_auth_state) {
              std::move(quit_closure).Run();
            }
          }));

  // Wait for the auth state to change.
  run_loop.Run();
}

}  // namespace supervised_user
