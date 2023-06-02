// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_STRING_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_STRING_UTILS_H_

#include <ostream>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// Output operators for logging.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
std::ostream& operator<<(std::ostream& out, AuthPurpose purpose);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
std::ostream& operator<<(std::ostream& out, AshAuthFactor factor);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
std::ostream& operator<<(std::ostream& out, AuthHubMode mode);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_STRING_UTILS_H_
