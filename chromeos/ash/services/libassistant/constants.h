// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONSTANTS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace chromeos {
namespace libassistant {

// A directory to save Assistant config files.
COMPONENT_EXPORT(LIBASSISTANT_CONSTANTS)
extern const base::FilePath::CharType kAssistantBaseDirPath[];

// A directory used in gLinux simulation.
COMPONENT_EXPORT(LIBASSISTANT_CONSTANTS)
extern const base::FilePath::CharType kAssistantTempBaseDirPath[];

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONSTANTS_H_
