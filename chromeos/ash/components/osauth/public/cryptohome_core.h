// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_CRYPTOHOME_CORE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_CRYPTOHOME_CORE_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace cryptohome {
class AuthFactorRef;
}

namespace ash {

class UserContext;
class AuthPerformer;

// This class is reused by various cryptohome-based AuthFactorEngines to
// reuse common operations (e.g. waiting for service initializaion, or
// establishing AuthSession).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) CryptohomeCore {
 public:
  using ServiceAvailabilityCallback = base::OnceCallback<void(bool available)>;

  class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) Client {
   public:
    virtual ~Client() = default;
    virtual void OnCryptohomeAuthSessionStarted() = 0;
    virtual void OnAuthSessionStartFailure() = 0;
    virtual void OnAuthFactorUpdate(cryptohome::AuthFactorRef factor) = 0;
    virtual void OnCryptohomeAuthSessionFinished() = 0;
  };

  // Convenience method.
  static inline CryptohomeCore* Get() {
    return AuthParts::Get()->GetCryptohomeCore();
  }

  virtual ~CryptohomeCore() = default;

  virtual void WaitForService(ServiceAvailabilityCallback callback) = 0;
  virtual void StartAuthSession(const AuthAttemptVector& attempt,
                                Client* client) = 0;
  virtual void EndAuthSession(Client* client) = 0;
  virtual AuthPerformer* GetAuthPerformer() const = 0;
  virtual UserContext* GetCurrentContext() const = 0;

  // Borrows the UserContext to perform Cryptohome actions in |callback|.
  // If the UserContext has been borrowed, the callback will be queued
  // to be called when the UserContext is returned.
  virtual void BorrowContext(BorrowContextCallback callback) = 0;

  // Returns the UserContext and if there are queued borrow callbacks,
  // the first queued callback will take the returned context and
  // get called.
  virtual void ReturnContext(std::unique_ptr<UserContext> context) = 0;
  virtual AuthProofToken StoreAuthenticationContext() = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_CRYPTOHOME_CORE_H_
