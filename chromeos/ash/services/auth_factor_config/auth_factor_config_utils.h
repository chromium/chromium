// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_UTILS_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_UTILS_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/syslog_logging.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"

namespace cryptohome {
class AuthFactor;
}

namespace ash::auth {

using ConfigureResultCallback =
    base::OnceCallback<void(mojom::ConfigureResult)>;

bool IsGaiaPassword(const cryptohome::AuthFactor& factor);

bool IsLocalPassword(const cryptohome::AuthFactor& factor);

void FailWithInvalidTokenError(base::Location from_here,
                               ConfigureResultCallback result_callback);

template <typename T>
void FailWithInvalidTokenError(
    base::Location from_here,
    base::OnceCallback<void(base::expected<T, mojom::ConfigureResult>)>
        result_callback) {
  SYSLOG(ERROR) << "(LOGIN) Invalid auth token: " << from_here.ToString();
  std::move(result_callback)
      .Run(base::unexpected(mojom::ConfigureResult::kInvalidTokenError));
}

void FailWithInvalidTokenError(base::Location from_here,
                               base::OnceCallback<void(bool)> result_callback);

template <typename ResultCallback, typename Continuation>
void OnContextBorrowed(base::Location from_here,
                       ResultCallback result_callback,
                       Continuation continuation_callback,
                       std::unique_ptr<UserContext> context) {
  if (!context) {
    FailWithInvalidTokenError(from_here, std::move(result_callback));
    return;
  }
  std::move(continuation_callback)
      .Run(std::move(result_callback), std::move(context));
}

template <typename ResultCallback, typename Continuation>
void ObtainContextOrFailImpl(base::Location from_here,
                             const std::string& auth_token,
                             ResultCallback result_callback,
                             Continuation continuation_callback) {
  if (!ash::AuthSessionStorage::Get()->IsValid(auth_token)) {
    FailWithInvalidTokenError(from_here, std::move(result_callback));
    return;
  }
  ash::AuthSessionStorage::Get()->BorrowAsync(
      from_here, auth_token,
      base::BindOnce(&OnContextBorrowed<ResultCallback, Continuation>,
                     from_here, std::move(result_callback),
                     std::move(continuation_callback)));
}

#define ObtainContextOrFail(auth_token, result_callback,                     \
                            continuation_callback)                           \
  ash::auth::ObtainContextOrFailImpl(FROM_HERE, auth_token, result_callback, \
                                     continuation_callback)

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_AUTH_FACTOR_CONFIG_UTILS_H_
