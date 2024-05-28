// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_TYPES_H_

#include <string>

#include "base/types/strong_alias.h"

// This file contains some strong-aliased types that are used for various
// authentication steps during login/unlock/in-session authentication process.
namespace ash {

// Password verified by GAIA as a part of login process.
using GaiaPassword = base::StrongAlias<class GaiaPasswordTag, std::string>;

// Password obtained from 3pIdP as a part of login process.
using SamlPassword = base::StrongAlias<class SAMLPasswordTag, std::string>;

// Password obtained/verified during any online authentication process
// (backed by either GAIA or SAML authentication).
using OnlinePassword = base::StrongAlias<class OnlinePasswordTag, std::string>;

// Password entered locally on the device that was not verified online.
using LocalPasswordInput =
    base::StrongAlias<class LocalPasswordTag, std::string>;

// Any password input used during authentication (either online or local input).
using PasswordInput = base::StrongAlias<class PasswordInputTag, std::string>;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_TYPES_H_
