// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_PLATFORM_WRAPPER_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_PLATFORM_WRAPPER_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"

namespace base {
class CommandLine;
}  // namespace base

namespace device_signals {

// Simple wrapper around basic platform operations to allow mocking in
// tests.
class PlatformWrapper {
 public:
  PlatformWrapper();
  virtual ~PlatformWrapper();

  // Returns the singleton instance of the PlatformWrapper.
  static PlatformWrapper* Get();

  // Executes the given command and returns the output.
  virtual bool Execute(const base::CommandLine& command_line,
                       std::string* output);

  // Returns true if the given path exists.
  virtual bool PathExists(const base::FilePath& path);

  // Sets the singleton instance for tests.
  static void SetInstanceForTesting(PlatformWrapper* executor);
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_PLATFORM_WRAPPER_H_
