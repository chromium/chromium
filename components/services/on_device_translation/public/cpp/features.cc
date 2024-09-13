// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/public/cpp/features.h"

#include "base/command_line.h"

namespace on_device_translation {

namespace {

base::FilePath GetPathFromCommandLine(const char* switch_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return base::FilePath();
  }
  return command_line->GetSwitchValuePath(switch_name);
}

}  // namespace

BASE_FEATURE(kEnableTranslateKitComponent,
             "EnableTranslateKitComponent",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
base::FilePath GetTranslateKitRootDirFromCommandLine() {
  return GetPathFromCommandLine(kTranslateKitRootDir);
}

// static
base::FilePath GetTranslateKitBinaryPathFromCommandLine() {
  return GetPathFromCommandLine(kTranslateKitBinaryPath);
}

}  // namespace on_device_translation
