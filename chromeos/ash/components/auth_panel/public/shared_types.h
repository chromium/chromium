// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_PUBLIC_SHARED_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_PUBLIC_SHARED_TYPES_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthHubConnector;

}  // namespace ash

namespace ash::auth_panel {

// Callback passed from clients of the dialog
// `success`: Whether or not the authentication was successful.
// `token`: If the authentication was successful, a token is returned from
// backends that can be passed to further sensitive operations
// (such as those in quickUnlockPrivate).
// `timeout`: The length of time for which the token is valid.
using AuthCompletionCallback =
    base::OnceCallback<void(bool success,
                            const ash::AuthProofToken& token,
                            base::TimeDelta timeout)>;

using SubmitPasswordCallback = base::RepeatingCallback<
    void(AuthHubConnector*, AshAuthFactor, const std::string&)>;

}  // namespace ash::auth_panel

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_PUBLIC_SHARED_TYPES_H_
