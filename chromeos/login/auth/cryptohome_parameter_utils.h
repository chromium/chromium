// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_CRYPTOHOME_PARAMETER_UTILS_H_
#define CHROMEOS_LOGIN_AUTH_CRYPTOHOME_PARAMETER_UTILS_H_

#include "base/component_export.h"

namespace cryptohome {
struct KeyDefinition;
}

namespace chromeos {

class UserContext;

namespace cryptohome_parameter_utils {

// This file provides helper functions for building and operating with
// cryptohome parameters based on the Chrome OS login data structures.

// Creates the cryptohome key definition structure based on the credentials and
// other information from the given user context.
COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
cryptohome::KeyDefinition CreateKeyDefFromUserContext(
    const UserContext& user_context);

// Similar to CreateKeyDefFromUserContext(), but the returned value is
// slightly altered to be suitable for authorization requests to cryptohome.
COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
cryptohome::KeyDefinition CreateAuthorizationKeyDefFromUserContext(
    const UserContext& user_context);

}  // namespace cryptohome_parameter_utils
}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_CRYPTOHOME_PARAMETER_UTILS_H_
