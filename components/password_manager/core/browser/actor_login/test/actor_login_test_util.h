// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_ACTOR_LOGIN_TEST_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_ACTOR_LOGIN_TEST_UTIL_H_

#include <optional>

#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace actor_login {

// Creates the expected `ActorLoginQuality_FormData`
// from the given `PasswordForm`.
optimization_guide::proto::ActorLoginQuality_FormData CreateExpectedFormData(
    const password_manager::PasswordForm& form);

// Creates the expected `ActorLoginQuality_ParsedFormDetails`
// from the given `PasswordForm`.
optimization_guide::proto::ActorLoginQuality_ParsedFormDetails
CreateExpectedLoginFormDetails(
    const password_manager::PasswordForm& form,
    bool is_username_visible,
    bool is_password_visible,
    std::optional<int> async_check_time_ms = std::nullopt);

// Creates a password`Credential` with the given `username` and `url`.
// The other fields are set to default values.
Credential CreateTestCredential(const std::u16string& username,
                                const GURL& url,
                                const url::Origin& request_origin);

// Creates a `PasswordForm` for the given `url`, `username` and `password`,
// labeled as if saved in the account store.
password_manager::PasswordForm CreateSavedPasswordForm(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password = u"");

// Creates a `FormData` with two fields: username and password for the given
// `url`.
autofill::FormData CreateSigninFormData(const GURL& url);

// Creates a `FormData` with two fields: new password and confirm password for
// the given `url`.
autofill::FormData CreateChangePasswordFormData(const GURL& url);

// Creates a `FormData` with only a username field.
autofill::FormData CreateUsernameOnlyFormData(const GURL& url);

// Creates a `FormData` with only a password field.
autofill::FormData CreatePasswordOnlyFormData(const GURL& url);

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_ACTOR_LOGIN_TEST_UTIL_H_
