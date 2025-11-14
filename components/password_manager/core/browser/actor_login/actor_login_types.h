// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_TYPES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_TYPES_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "base/types/strong_alias.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {

enum CredentialType {
  kPassword,
};

struct Credential {
  Credential();

  Credential(const Credential& other);
  Credential(Credential&& other);

  Credential& operator=(const Credential& credential);
  Credential& operator=(Credential&& credential);

  ~Credential();

  // A unique identifier for this credential. Used for internal tracking.
  // Should not be displayed to the user.
  using Id = base::IdType32<Credential>;
  Id id;

  // The username associated with the credential.
  // This could be an email address or a username used to identify the user
  // during the login process. It is unique for this `source_site_or_app`.
  // It may be an empty string if the credential has no associated username.
  // This field may be presented to the user.
  // TODO(crbug.com/441231848): Clarify how to deal with empty usernames.
  // We should either provide display and non-display values, or let the caller
  // format strings to display.
  std::u16string username;

  // The original website or application for which this credential was saved in
  // GPM. This filed may be presented to the user.
  // TODO(crbug.com/441231531): Clarify the format.
  // We should probably provide display and non-display values, or let the
  // caller format strings to display.
  std::u16string source_site_or_app;

  // The origin for which this credential was requested.
  url::Origin request_origin;

  // The type of the credential used for the login process.
  // It may be presented to a user if mapped to a user-friendly localized
  // descriptor string.
  CredentialType type = kPassword;

  // Signal of whether any sign-in fields were seen on the page, or if APIs
  // associated with this `CredentialType` report that this login is available
  // on the provided Tab.
  bool immediatelyAvailableToLogin = false;

  // Whether the user has granted persistent permission for this credential to
  // be used on `request_origin`.
  bool has_persistent_permission = false;

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
  // Filling is disallowed (e.g. because of a policy)
  kFillingNotAllowed,
  // There was an error of unknown type.
  kUnknown,
};

using CredentialsOrError =
    base::expected<std::vector<Credential>, ActorLoginError>;
using CredentialsOrErrorReply = base::OnceCallback<void(CredentialsOrError)>;

enum class LoginStatusResult {
  // Either there was only a username field in the form, or only
  // the username field was filled successfully.
  kSuccessUsernameFilled,
  // Either there was only a password field in the form, or only
  // the password field was filled successfully.
  kSuccessPasswordFilled,
  // Both username and password fields were filled successfully.
  kSuccessUsernameAndPasswordFilled,
  // The page has no signin form. Note: Cross-origin iframes aren't
  // supported.
  kErrorNoSigninForm,
  // The provided credential is not a saved match for the site on which
  // login was triggered.
  kErrorInvalidCredential,
  // Neither the username, nor the password field could be filled.
  kErrorNoFillableFields,
  // Returned if the task is in a background tab and filling requires device
  // reauth. The user needs to focus that tab first.
  kErrorDeviceReauthRequired,
  // Returned if the device re-authentication fails.
  kErrorDeviceReauthFailed,
};

using LoginStatusResultOrError =
    base::expected<LoginStatusResult, ActorLoginError>;
using LoginStatusResultOrErrorReply =
    base::OnceCallback<void(LoginStatusResultOrError)>;

// C++ enum copy of `GetCredentialsOutcome` in `actor_login.proto`.
enum class GetCredentialsOutcomeMqls {
  kUnspecified,
  kNoCredentials,
  kSignInFormExists,
  kNoSignInForm,
  kFillingNotAllowed,
};

optimization_guide::proto::
    ActorLoginQuality_GetCredentialsDetails_GetCredentialsOutcome
    OutcomeEnumToProtoType(GetCredentialsOutcomeMqls outcome);

enum class PermissionDetailsMqls {
  kUnknown,
  kHasPermanentPermission,
  kNoPermanentPermission,
};

optimization_guide::proto::
    ActorLoginQuality_GetCredentialsDetails_PermissionDetails
    PermissionEnumToProtoType(PermissionDetailsMqls permission);

// C++ enum copy of `AttemptLoginOutcome` in `actor_login.proto`.
enum class AttemptLoginOutcomeMqls {
  kUnspecified,
  kSuccess,
  kNoSignInForm,
  kInvalidCredential,
  kNoFillableFields,
  kDisallowedOrigin,
  kReauthRequired,
  kReauthFailed,

};

optimization_guide::proto::
    ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome
    OutcomeEnumToProtoType(AttemptLoginOutcomeMqls outcome);

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_TYPES_H_
