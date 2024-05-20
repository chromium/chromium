// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_CRYPTOHOME_CORE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_CRYPTOHOME_CORE_IMPL_H_

#include <memory>
#include <optional>
#include <queue>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"

namespace ash {
class UserDataAuthClient;
class UserContext;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) CryptohomeCoreImpl
    : public CryptohomeCore {
 public:
  explicit CryptohomeCoreImpl(UserDataAuthClient* client);
  ~CryptohomeCoreImpl() override;

  void WaitForService(ServiceAvailabilityCallback callback) override;
  void StartAuthSession(const AuthAttemptVector& attempt,
                        Client* client) override;
  void EndAuthSession(Client* client) override;
  UserContext* GetCurrentContext() const override;
  void BorrowContext(BorrowContextCallback callback) override;
  AuthPerformer* GetAuthPerformer() const override;
  void ReturnContext(std::unique_ptr<UserContext> context) override;
  AuthProofToken StoreAuthenticationContext() override;

 private:
  enum class Stage {
    kIdle,
    kAuthSessionRequested,
    kAuthFactorConfigurationRequested,
    kFinished,
  };

  void OnServiceStatus(ServiceAvailabilityCallback callback,
                       bool service_is_available);
  void OnAuthSessionStarted(bool user_exists,
                            std::unique_ptr<UserContext> context,
                            std::optional<AuthenticationError> error);
  void OnGetAuthFactorsConfiguration(std::unique_ptr<UserContext> context,
                                     std::optional<AuthenticationError> error);
  void OnInvalidateAuthSession(std::unique_ptr<UserContext> context,
                               std::optional<AuthenticationError> error);
  void EndAuthSessionImpl();
  void BorrowContextAndRun(BorrowContextCallback callback);

  std::optional<AuthAttemptVector> current_attempt_;
  base::flat_set<raw_ptr<Client>> clients_;
  base::flat_set<raw_ptr<Client>> clients_being_removed_;
  std::queue<BorrowContextCallback> borrow_callback_queue_;

  Stage current_stage_ = Stage::kIdle;
  bool auth_session_started_ = false;
  bool was_authenticated_ = false;
  std::unique_ptr<UserContext> context_;
  raw_ptr<UserDataAuthClient> dbus_client_;
  std::unique_ptr<AuthPerformer> performer_;
  std::unique_ptr<AuthFactorEditor> editor_;

  base::WeakPtrFactory<CryptohomeCoreImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_CRYPTOHOME_CORE_IMPL_H_
