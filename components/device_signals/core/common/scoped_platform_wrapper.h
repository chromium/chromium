// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SCOPED_PLATFORM_WRAPPER_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SCOPED_PLATFORM_WRAPPER_H_

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "components/device_signals/core/common/platform_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class ScopedPlatformWrapper : public PlatformWrapper {
 public:
  ScopedPlatformWrapper();
  ~ScopedPlatformWrapper() override;

  MOCK_METHOD(bool,
              Execute,
              (const base::CommandLine&, std::string*),
              (override));

  // This override will have a default behavior of calling the real
  // implementation, to make it easier if some tests take advantage of e.g.
  // ScopedTempDir.
  MOCK_METHOD(bool, PathExists, (const base::FilePath&), (override));

 private:
  raw_ptr<PlatformWrapper> original_wrapper_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SCOPED_PLATFORM_WRAPPER_H_
