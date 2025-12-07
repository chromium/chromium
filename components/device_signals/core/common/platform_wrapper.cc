// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_wrapper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"

namespace device_signals {

namespace {

PlatformWrapper* g_test_instance = nullptr;

}  // namespace

PlatformWrapper::PlatformWrapper() = default;
PlatformWrapper::~PlatformWrapper() = default;

// static
PlatformWrapper* PlatformWrapper::Get() {
  if (g_test_instance) {
    return g_test_instance;
  }
  static base::NoDestructor<PlatformWrapper> instance;
  return instance.get();
}

bool PlatformWrapper::Execute(const base::CommandLine& command_line,
                              std::string* output) {
  return base::GetAppOutput(command_line, output);
}

bool PlatformWrapper::PathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

// static
void PlatformWrapper::SetInstanceForTesting(PlatformWrapper* executor) {
  g_test_instance = executor;
}

}  // namespace device_signals
