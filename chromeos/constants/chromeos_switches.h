// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"

// This file is only for command-line switches that are shared between
// ash-chrome and lacros-chrome. For ash command-line switches, please add them
// in //ash/constants/ash_switches.h.
namespace chromeos::switches {

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kContainerAppPreinstallActivationTimeThreshold[];

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kMahiRestrictionsOverride[];

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
std::optional<base::Time> GetContainerAppPreinstallActivationTimeThreshold();

}  // namespace chromeos::switches

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_SWITCHES_H_
