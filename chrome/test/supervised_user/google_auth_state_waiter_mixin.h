// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_GOOGLE_AUTH_STATE_WAITER_MIXIN_H_
#define CHROME_TEST_SUPERVISED_USER_GOOGLE_AUTH_STATE_WAITER_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/supervised_user/core/browser/child_account_service.h"

class InProcessBrowserTest;

namespace supervised_user {

// This mixin is responsible for waiting for the Google auth state of the user
// to reach the expected value.
class GoogleAuthStateWaiterMixin : public InProcessBrowserTestMixin {
 public:
  GoogleAuthStateWaiterMixin() = delete;
  GoogleAuthStateWaiterMixin(
      InProcessBrowserTestMixinHost& test_mixin_host,
      InProcessBrowserTest* test_base,
      ChildAccountService::AuthState expected_auth_state);

  GoogleAuthStateWaiterMixin(const GoogleAuthStateWaiterMixin&) = delete;
  GoogleAuthStateWaiterMixin& operator=(const GoogleAuthStateWaiterMixin&) =
      delete;
  ~GoogleAuthStateWaiterMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

 private:
  raw_ptr<InProcessBrowserTest> test_base_;
  ChildAccountService::AuthState expected_auth_state_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_GOOGLE_AUTH_STATE_WAITER_MIXIN_H_
