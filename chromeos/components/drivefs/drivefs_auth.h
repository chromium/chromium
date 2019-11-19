// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_AUTH_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_AUTH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"

class AccountId;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace drivefs {

class COMPONENT_EXPORT(DRIVEFS) DriveFsAuth {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;
    virtual void BindIdentityAccessor(
        mojo::PendingReceiver<identity::mojom::IdentityAccessor> receiver) = 0;
    virtual const AccountId& GetAccountId() = 0;
    virtual std::string GetObfuscatedAccountId() = 0;
    virtual bool IsMetricsCollectionEnabled() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  DriveFsAuth(const base::Clock* clock,
              const base::FilePath& profile_path,
              std::unique_ptr<base::OneShotTimer> timer,
              Delegate* delegate);
  virtual ~DriveFsAuth();

  const base::FilePath& GetProfilePath() const { return profile_path_; }

  const AccountId& GetAccountId() { return delegate_->GetAccountId(); }

  std::string GetObfuscatedAccountId() {
    return delegate_->GetObfuscatedAccountId();
  }

  bool IsMetricsCollectionEnabled() {
    return delegate_->IsMetricsCollectionEnabled();
  }

  base::Optional<std::string> GetCachedAccessToken();

  virtual void GetAccessToken(
      bool use_cached,
      mojom::DriveFsDelegate::GetAccessTokenCallback callback);

 private:
  void AccountReady(const CoreAccountId& account_id,
                    const std::string& gaia,
                    const std::string& email,
                    const identity::AccountState& state);

  void GotChromeAccessToken(const base::Optional<std::string>& access_token,
                            base::Time expiration_time,
                            const GoogleServiceAuthError& error);

  const std::string& GetOrResetCachedToken(bool use_cached);

  void UpdateCachedToken(const std::string& token, base::Time expiry);

  void AuthTimeout();

  identity::mojom::IdentityAccessor* GetIdentityAccessor();

  SEQUENCE_CHECKER(sequence_checker_);
  const base::Clock* const clock_;
  const base::FilePath profile_path_;
  const std::unique_ptr<base::OneShotTimer> timer_;
  Delegate* const delegate_;

  // The connection to the identity service. Access via |GetIdentityAccessor()|.
  mojo::Remote<identity::mojom::IdentityAccessor> identity_accessor_;

  // Pending callback for an in-flight GetAccessToken request.
  mojom::DriveFsDelegate::GetAccessTokenCallback get_access_token_callback_;

  std::string last_token_;
  base::Time last_token_expiry_;

  base::WeakPtrFactory<DriveFsAuth> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DriveFsAuth);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_AUTH_H_
