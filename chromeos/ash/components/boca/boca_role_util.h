// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_ROLE_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_ROLE_UTIL_H_

#include "ash/ash_export.h"

namespace ash::boca_util {

ASH_EXPORT bool IsProducer();

ASH_EXPORT bool IsConsumer();

ASH_EXPORT bool IsEnabled();

}  // namespace ash::boca_util

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_ROLE_UTIL_H_
