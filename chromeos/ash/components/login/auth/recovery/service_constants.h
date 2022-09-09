// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_SERVICE_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_SERVICE_CONSTANTS_H_

#include <string>

class GURL;

namespace ash {

std::string GetRecoveryHsmPublicKey();

// Used to fetch the epoch public key and metadata from the recovery server.
GURL GetRecoveryServiceEpochURL();

// used to perform mediation on the recovery request, the derived/mediated
// secrets are returned in response.
GURL GetRecoveryServiceMediateURL();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_RECOVERY_SERVICE_CONSTANTS_H_
