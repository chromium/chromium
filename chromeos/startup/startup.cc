// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/startup.h"

#include <stdio.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/startup/startup_switches.h"

namespace chromeos {

absl::optional<std::string> ReadStartupData() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kCrosStartupDataFD))
    return absl::nullopt;

  int raw_fd = 0;
  if (!base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kCrosStartupDataFD),
          &raw_fd)) {
    LOG(ERROR) << "Unrecognizable value for --" << switches::kCrosStartupDataFD;
    return absl::nullopt;
  }
  base::ScopedFILE file(fdopen(raw_fd, "r"));
  std::string content;
  if (!base::ReadStreamToString(file.get(), &content)) {
    LOG(ERROR) << "Failed to read startup data";
    return absl::nullopt;
  }

  return absl::make_optional(std::move(content));
}

}  // namespace chromeos
