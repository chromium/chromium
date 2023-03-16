// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_

#include <string>

#include "base/unguessable_token.h"

namespace ash {

// This token represents authentication proof. It can be safely passed
// between components, and can be used to obtain authenticated
// 'UserContext' from `AuthSessionStorage` to perform authenticated
// operations.
// TODO(b/259528315): Once switch from QuickUnlockStorage is completed,
// replace it with StrongAlias or UnguessableToken.
using AuthProofToken = std::string;

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_COMMON_TYPES_H_
