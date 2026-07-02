// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"

#include "base/syslog_logging.h"
#include "chromeos/ash/components/osauth/public/auth_policy_utils.h"

namespace ash::auth {

bool IsGaiaPassword(const cryptohome::AuthFactor& factor) {
  return ash::IsGaiaPassword(factor);
}

bool IsLocalPassword(const cryptohome::AuthFactor& factor) {
  return ash::IsLocalPassword(factor);
}

void FailWithInvalidTokenError(base::Location from_here,
                               ConfigureResultCallback result_callback) {
  SYSLOG(ERROR) << "(LOGIN) Invalid auth token: " << from_here.ToString();
  std::move(result_callback).Run(mojom::ConfigureResult::kInvalidTokenError);
}

void FailWithInvalidTokenError(base::Location from_here,
                               base::OnceCallback<void(bool)> result_callback) {
  SYSLOG(ERROR) << "(LOGIN) Invalid auth token: " << from_here.ToString();
  std::move(result_callback).Run(false);
}

}  // namespace ash::auth
