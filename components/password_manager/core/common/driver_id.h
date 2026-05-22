// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_DRIVER_ID_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_DRIVER_ID_H_

#include "base/types/id_type.h"

namespace password_manager {

using DriverId = base::IdType32<class PasswordManagerDriverTag>;

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_DRIVER_ID_H_
