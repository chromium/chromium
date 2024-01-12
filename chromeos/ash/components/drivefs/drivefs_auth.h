// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_AUTH_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_AUTH_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"

class AccountId;
class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace drivefs {

using AccessTokenCallback =
    mojom::DriveFsDelegate::GetAccessTokenWithExpiryCallback;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsAuth {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;
    virtual signin::IdentityManager* GetIdentityManager() = 0;
    virtual const AccountId& GetAccountId() = 0;
    virtual std::string GetObfuscatedAccountId() = 0;
    virtual bool IsMetricsCollectionEnabled() = 0;
  };

  DriveFsAuth(const base::Clock* clock,
              const base::FilePath& profile_path,
              std::unique_ptr<base::OneShotTimer> timer,
              Delegate* delegate);

  DriveFsAuth(const DriveFsAuth&) = delete;
  DriveFsAuth& operator=(const DriveFsAuth&) = delete;

  virtual ~DriveFsAuth();

  const base::FilePath& GetProfilePath() const { return profile_path_; }

  const AccountId& GetAccountId() { return delegate_->GetAccountId(); }

  std::string GetObfuscatedAccountId() {
    return delegate_->GetObfuscatedAccountId();
  }

  bool IsMetricsCollectionEnabled() {
    return delegate_->IsMetricsCollectionEnabled();
  }

  std::optional<std::string> GetCachedAccessToken();

  virtual void GetAccessToken(bool use_cached, AccessTokenCallback callback);

 private:
  void GotChromeAccessToken(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  const std::string& GetOrResetCachedToken(bool use_cached);

  void UpdateCachedToken(const std::string& token, base::Time expiry);

  void AuthTimeout();

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ptr<const base::Clock> clock_;
  const base::FilePath profile_path_;
  const std::unique_ptr<base::OneShotTimer> timer_;
  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Pending callback for an in-flight GetAccessToken{WithExpiry} request.
  AccessTokenCallback get_access_token_callback_;

  std::string last_token_;
  base::Time last_token_expiry_;
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_AUTH_H_
