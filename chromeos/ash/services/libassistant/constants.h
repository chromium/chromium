// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONSTANTS_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONSTANTS_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"

namespace ash::libassistant {

// A directory to save Assistant config files.
COMPONENT_EXPORT(LIBASSISTANT_CONSTANTS)
extern const base::FilePath::CharType kAssistantBaseDirPath[];

// Libassistant library DLC root path.
COMPONENT_EXPORT(LIBASSISTANT_CONSTANTS)
extern const char kLibAssistantDlcRootPath[];

// Libassistant v2 library DLC path.
COMPONENT_EXPORT(LIBASSISTANT_CONSTANTS)
extern const base::FilePath::CharType kLibAssistantV2DlcPath[];

// A directory to save Libassistant socket files.
COMPONENT_EXPORT(LIBASSISTANT_CONSTANTS)
extern const base::FilePath::CharType kLibAssistantSocketPath[];
}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_CONSTANTS_H_
