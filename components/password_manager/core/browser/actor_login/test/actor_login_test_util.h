// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_ACTOR_LOGIN_TEST_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_ACTOR_LOGIN_TEST_UTIL_H_

#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "url/gurl.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace actor_login {

// Creates a password`Credential` with the given `username` and `url`.
// The other fields are set to default values.
Credential CreateTestCredential(const std::u16string& username,
                                const GURL& url);

// Creates a `FormData` with two fields: username and password for the given
// `url`.
autofill::FormData CreateSigninFormData(const GURL& url);

// Creates a `FormData` with two fields: new password and confirm password for
// the given `url`.
autofill::FormData CreateChangePasswordFormData(const GURL& url);

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_ACTOR_LOGIN_TEST_UTIL_H_
