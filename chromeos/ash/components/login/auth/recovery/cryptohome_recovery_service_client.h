// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_SERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_SERVICE_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/login/auth/public/recovery_types.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ash {

// Handles fetching epoch and sending recovery request to the recovery service.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    CryptohomeRecoveryServiceClient {
 public:
  using OnEpochResponseCallback =
      base::OnceCallback<void(std::optional<CryptohomeRecoveryEpochResponse>,
                              CryptohomeRecoveryServerStatusCode)>;
  using OnRecoveryResponseCallback =
      base::OnceCallback<void(std::optional<CryptohomeRecoveryResponse>,
                              CryptohomeRecoveryServerStatusCode)>;

  explicit CryptohomeRecoveryServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  CryptohomeRecoveryServiceClient(
      const CryptohomeRecoveryServiceClient& other) = delete;
  CryptohomeRecoveryServiceClient& operator=(
      const CryptohomeRecoveryServiceClient& other) = delete;

  ~CryptohomeRecoveryServiceClient();

  void FetchEpoch(const GaiaAccessToken& gaia_access_token,
                  OnEpochResponseCallback callback);

  void FetchRecoveryResponse(const std::string& request,
                             const GaiaAccessToken& gaia_access_token,
                             OnRecoveryResponseCallback callback);

 private:
  void OnFetchEpochComplete(const GaiaAccessToken& access_token,
                            OnEpochResponseCallback callback,
                            std::unique_ptr<std::string> response_body);
  void OnFetchRecoveryResponseComplete(
      const std::string& request,
      const GaiaAccessToken& access_token,
      OnRecoveryResponseCallback callback,
      std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The backoff policy for fetching epoch and recovery response.
  net::BackoffEntry epoch_retry_backoff_;
  net::BackoffEntry recovery_retry_backoff_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CryptohomeRecoveryServiceClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_CRYPTOHOME_RECOVERY_SERVICE_CLIENT_H_
