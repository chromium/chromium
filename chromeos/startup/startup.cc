// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/startup.h"

#include <stdio.h>
#include <sys/mman.h>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
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

base::ScopedFD CreateMemFDFromBrowserInitParams(
    const crosapi::mojom::BrowserInitParamsPtr& data) {
  std::vector<uint8_t> serialized =
      crosapi::mojom::BrowserInitParams::Serialize(&data);

  base::ScopedFD fd(memfd_create("startup_data", 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create a memory backed file";
    return base::ScopedFD();
  }

  if (!base::WriteFileDescriptor(fd.get(), serialized)) {
    LOG(ERROR) << "Failed to dump the serialized BrowserInitParams";
    return base::ScopedFD();
  }

  if (lseek(fd.get(), 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Failed to reset the FD position";
    return base::ScopedFD();
  }

  return fd;
}

}  // namespace chromeos
