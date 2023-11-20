// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DIMENSIONS_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DIMENSIONS_UTILS_H_

#include <string>

#include "base/component_export.h"

namespace ash::demo_mode {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEMO_MODE)
std::string CanonicalizeDimension(const std::string& dimension_value);

}  // namespace ash::demo_mode

#endif  // CHROMEOS_ASH_COMPONENTS_DEMO_MODE_UTILS_DIMENSIONS_UTILS_H_
