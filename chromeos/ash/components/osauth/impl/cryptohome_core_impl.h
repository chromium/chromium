// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_CRYPTOHOME_CORE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_CRYPTOHOME_CORE_IMPL_H_

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
class UserDataAuthClient;
class UserContext;

class CryptohomeCoreImpl : public CryptohomeCore {
 public:
  CryptohomeCoreImpl(UserDataAuthClient* client);
  ~CryptohomeCoreImpl() override;

  void WaitForService(ServiceAvailabilityCallback callback) override;
  void StartAuthSession(const AuthAttemptVector& attempt,
                        Client* client) override;
  void EndAuthSession(Client* client) override;
  UserContext* GetCurrentContext() const override;
  AuthPerformer* GetAuthPerformer() const override;
  std::unique_ptr<UserContext> BorrowContext() override;
  void ReturnContext(std::unique_ptr<UserContext> context) override;
  AuthProofToken StoreAuthenticationContext() override;

 private:
  void OnServiceStatus(ServiceAvailabilityCallback callback,
                       bool service_is_available);
  void OnAuthSessionStarted(bool user_exists,
                            std::unique_ptr<UserContext> context,
                            absl::optional<AuthenticationError> error);
  void OnInvalidateAuthSession(std::unique_ptr<UserContext> context,
                               absl::optional<AuthenticationError> error);
  void EndAuthSessionImpl();

  absl::optional<AuthAttemptVector> current_attempt_;
  base::flat_set<base::raw_ptr<Client>> clients_;
  base::flat_set<base::raw_ptr<Client>> clients_being_removed_;

  bool is_authorized_ = false;
  std::unique_ptr<UserContext> context_;
  base::raw_ptr<UserDataAuthClient> dbus_client_;
  std::unique_ptr<AuthPerformer> performer_;

  base::WeakPtrFactory<CryptohomeCoreImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_CRYPTOHOME_CORE_IMPL_H_
