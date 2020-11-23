// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_IMPL_H_
#define CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class AuthStatusConsumer;
class UserContext;

// Implements ExtendedAuthenticator.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) ExtendedAuthenticatorImpl
    : public ExtendedAuthenticator {
 public:
  static scoped_refptr<ExtendedAuthenticatorImpl> Create(
      AuthStatusConsumer* consumer);

  // ExtendedAuthenticator:
  void SetConsumer(AuthStatusConsumer* consumer) override;
  void AuthenticateToMount(const UserContext& context,
                           ResultCallback success_callback) override;
  void AuthenticateToCheck(const UserContext& context,
                           base::OnceClosure success_callback) override;
  void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EndFingerprintAuthSession() override;
  void AuthenticateWithFingerprint(
      const UserContext& context,
      base::OnceCallback<void(cryptohome::CryptohomeErrorCode)> callback)
      override;
  void AddKey(const UserContext& context,
              const cryptohome::KeyDefinition& key,
              bool clobber_if_exists,
              base::OnceClosure success_callback) override;
  void RemoveKey(const UserContext& context,
                 const std::string& key_to_remove,
                 base::OnceClosure success_callback) override;
  void TransformKeyIfNeeded(const UserContext& user_context,
                            ContextCallback callback) override;

 private:
  explicit ExtendedAuthenticatorImpl(AuthStatusConsumer* consumer);
  ~ExtendedAuthenticatorImpl() override;

  // Callback for system salt getter.
  void OnSaltObtained(const std::string& system_salt);

  // Performs actual operation with fully configured |context|.
  void DoAuthenticateToMount(ResultCallback success_callback,
                             const UserContext& context);
  void DoAuthenticateToCheck(base::OnceClosure success_callback,
                             const UserContext& context);
  void DoAddKey(const cryptohome::KeyDefinition& key,
                bool clobber_if_exists,
                base::OnceClosure success_callback,
                const UserContext& context);
  void DoRemoveKey(const std::string& key_to_remove,
                   base::OnceClosure success_callback,
                   const UserContext& context);

  // Inner operation callbacks.
  void OnMountComplete(const std::string& time_marker,
                       const UserContext& context,
                       ResultCallback success_callback,
                       base::Optional<cryptohome::BaseReply> reply);
  void OnOperationComplete(const std::string& time_marker,
                           const UserContext& context,
                           base::OnceClosure success_callback,
                           bool success,
                           cryptohome::MountError return_code);
  void OnStartFingerprintAuthSessionComplete(
      base::OnceCallback<void(bool)> callback,
      base::Optional<cryptohome::BaseReply> reply);
  void OnFingerprintScanComplete(
      base::OnceCallback<void(cryptohome::CryptohomeErrorCode)> callback,
      base::Optional<cryptohome::BaseReply> reply);

  bool salt_obtained_;
  std::string system_salt_;
  std::vector<base::OnceClosure> system_salt_callbacks_;

  AuthStatusConsumer* consumer_;

  DISALLOW_COPY_AND_ASSIGN(ExtendedAuthenticatorImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_EXTENDED_AUTHENTICATOR_IMPL_H_
