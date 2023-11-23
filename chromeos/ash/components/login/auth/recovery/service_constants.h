// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_SERVICE_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_SERVICE_CONSTANTS_H_

#include <string>
#include <vector>
#include "base/component_export.h"

class GURL;

namespace ash {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
std::string GetRecoveryHsmPublicKey();

std::string GetRecoveryLedgerName();
std::string GetRecoveryLedgerPublicKey();
uint32_t GetRecoveryLedgerPublicKeyHash();

// Used to fetch the epoch public key and metadata from the recovery server.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
GURL GetRecoveryServiceEpochURL();

// Used to perform mediation on the recovery request, the derived/mediated
// secrets are returned in response.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
GURL GetRecoveryServiceMediateURL();

// Used to fetch the reauth request token, which will be used as a parameter in
// the Gaia embedded sign-in URL.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
GURL GetRecoveryServiceReauthTokenURL();

// OAuth2 scope for the recovery service.
std::vector<std::string> GetRecoveryOAuth2Scope();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_SERVICE_CONSTANTS_H_
