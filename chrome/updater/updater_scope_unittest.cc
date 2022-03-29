// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater_scope.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/updater/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterScope, GetUpdaterScopeForCommandLine) {
  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("updater.exe")));
  DCHECK_EQ(GetUpdaterScopeForCommandLine(command_line), UpdaterScope::kUser);
  command_line.AppendSwitch(kSystemSwitch);
  DCHECK_EQ(GetUpdaterScopeForCommandLine(command_line), UpdaterScope::kSystem);
}

}  // namespace updater
