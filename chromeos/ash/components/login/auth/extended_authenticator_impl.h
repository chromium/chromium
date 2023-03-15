// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/auth/extended_authenticator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class AuthStatusConsumer;
class UserContext;

// Implements ExtendedAuthenticator.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    ExtendedAuthenticatorImpl : public ExtendedAuthenticator {
 public:
  static scoped_refptr<ExtendedAuthenticatorImpl> Create(
      AuthStatusConsumer* consumer);

  ExtendedAuthenticatorImpl(const ExtendedAuthenticatorImpl&) = delete;
  ExtendedAuthenticatorImpl& operator=(const ExtendedAuthenticatorImpl&) =
      delete;

  // ExtendedAuthenticator:
  void SetConsumer(AuthStatusConsumer* consumer) override;
  void AuthenticateToCheck(const UserContext& context,
                           base::OnceClosure success_callback) override;
  void TransformKeyIfNeeded(const UserContext& user_context,
                            ContextCallback callback) override;

 private:
  explicit ExtendedAuthenticatorImpl(AuthStatusConsumer* consumer);
  ~ExtendedAuthenticatorImpl() override;

  // Callback for system salt getter.
  void OnSaltObtained(const std::string& system_salt);

  // Performs actual operation with fully configured |context|.
  void DoAuthenticateToCheck(base::OnceClosure success_callback,
                             bool unlock_webauthn_secret,
                             const UserContext& context);

  // Inner operation callbacks.
  template <typename ReplyType>
  void OnOperationComplete(const char* time_marker,
                           const UserContext& context,
                           base::OnceClosure success_callback,
                           absl::optional<ReplyType> reply);

  bool salt_obtained_;
  std::string system_salt_;
  std::vector<base::OnceClosure> system_salt_callbacks_;

  AuthStatusConsumer* consumer_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_IMPL_H_
