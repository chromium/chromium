// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGGING_SESSION_LOG_H_
#define CHROMEOS_ASH_COMPONENTS_LOGGING_SESSION_LOG_H_

#include "base/component_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// The name of the environment variable holding the session log directory when
// logged in to ChromeOS.
inline constexpr char kSessionLogDirEnvVar[] = "CHROMEOS_SESSION_LOG_DIR";

// Returns the session log directory path, specified by `kSessionLogDirEnvVar`.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING)
base::FilePath GetSessionLogDir();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGGING_SESSION_LOG_H_
