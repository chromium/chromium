// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/logging/session_log.h"

#include <memory>
#include <optional>
#include <string>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/notreached.h"

namespace ash {

base::FilePath GetSessionLogDir() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::optional<std::string> log_dir = env->GetVar(kSessionLogDirEnvVar);
  if (!log_dir.has_value()) {
    NOTREACHED();
  }
  return base::FilePath(log_dir.value());
}

}  // namespace ash
