// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_TYPES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_TYPES_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "url/gurl.h"

namespace actor_login {

enum CredentialType {
  kPassword,
};

struct Credential {
  // The username associated with the credential.
  // This could be an email address or a username used to identify the user
  // during the login process. It is unique for this `source_site_or_app`.
  // It may be an empty string if the credential has no associated username.
  // This field may be presented to the user.
  // TODO(crbug.com/427171031): Clarify how to deal with empty usernames.
  // We should either provide display and non-display values, or let the caller
  // format strings to display.
  std::u16string username;
  // The original website or application for which this credential was saved in
  // GPM. This filed may be presented to the user.
  // TODO(crbug.com/427171031): Clarify the format.
  // We should probably provide display and non-display values, or let the
  // caller format strings to display.
  std::u16string source_site_or_app;
  // The type of the credential used for the login process.
  // It may be presented to a user if mapped to a user-friendly localized
  // descriptor string.
  CredentialType type = kPassword;
  // Signal of whether any sign-in fields were seen on the page, or if APIs
  // associated with this `CredentialType` report that this login is available
  // on the provided Tab.
  bool immediatelyAvailableToLogin = false;

#if defined(UNIT_TEST)
  // An exact equality comparison of all the fields is only useful for tests.
  friend bool operator==(const Credential&, const Credential&) = default;
#endif
};

enum ActorLoginError {
  // Only one request at a time is allowed per `WebContents` (i.e per tab)
  kServiceBusy,
  // The provided `TabInterface` was invalid (e.g. no associated `WebContents`
  // was loaded, or tab is no longer present)
  kInvalidTabInterface,
  // There was an error of unknown type.
  kUnknown,
};

using CredentialsOrError =
    base::expected<std::vector<Credential>, ActorLoginError>;
using CredentialsOrErrorReply = base::OnceCallback<void(CredentialsOrError)>;

enum class LoginStatusResult {
  kSuccessUsernameAndPasswordFilled,
  kErrorNoSigninForm,
};

using LoginStatusResultOrError =
    base::expected<LoginStatusResult, ActorLoginError>;
using LoginStatusResultOrErrorReply =
    base::OnceCallback<void(LoginStatusResultOrError)>;

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_TYPES_H_
