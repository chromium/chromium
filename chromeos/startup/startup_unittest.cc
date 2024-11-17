// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/startup.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "chromeos/startup/startup_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

base::ScopedFD CreateMemoryFile(std::string_view content) {
  base::ScopedFD file(memfd_create("test", 0));
  if (!file.is_valid()) {
    PLOG(ERROR) << "Failed to create a memory file";
    return base::ScopedFD();
  }

  if (!base::WriteFileDescriptor(file.get(), content)) {
    LOG(ERROR) << "Failed to write the data";
    return base::ScopedFD();
  }

  // Reset the cursor.
  if (lseek(file.get(), 0, SEEK_SET) < 0) {
    PLOG(ERROR) << "Failed to reset the file position";
    return base::ScopedFD();
  }

  return file;
}

}  // namespace

TEST(ChromeOSStartup, Startup) {
  constexpr char kTestData[] = "test test test test";
  base::ScopedFD file = CreateMemoryFile(kTestData);

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Release the FD. The FD is consumed in ReadStartupData().
  command_line->AppendSwitchASCII(switches::kCrosStartupDataFD,
                                  base::NumberToString(file.release()));

  std::optional<std::string> data = ReadStartupData();
  EXPECT_EQ(data, kTestData);
}

TEST(ChromeOSStartup, NoFlag) {
  EXPECT_FALSE(ReadStartupData());
}

}  // namespace chromeos
