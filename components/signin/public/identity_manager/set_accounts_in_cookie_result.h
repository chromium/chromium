// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SET_ACCOUNTS_IN_COOKIE_RESULT_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SET_ACCOUNTS_IN_COOKIE_RESULT_H_

namespace signin {

// Result of a "Set Accounts" cookie operation.
enum class SetAccountsInCookieResult {
  // The request succeeded.
  kSuccess,
  // The request failed, and can be retried.
  kTransientError,
  // The request failed and should not be retried.
  kPersistentError,
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_SET_ACCOUNTS_IN_COOKIE_RESULT_H_
