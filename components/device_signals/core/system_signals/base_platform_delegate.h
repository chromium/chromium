// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_BASE_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_BASE_PLATFORM_DELEGATE_H_

#include "build/build_config.h"
#include "components/device_signals/core/system_signals/platform_delegate.h"

namespace device_signals {

// Implements some functionality that is common to all PlatformDelegate
// specializations.
class BasePlatformDelegate : public PlatformDelegate {
 public:
  ~BasePlatformDelegate() override;

  // PlatformDelegate:
  bool PathIsReadable(const base::FilePath& file_path) const override;
  bool DirectoryExists(const base::FilePath& file_path) const override;
  FilePathMap<bool> AreExecutablesRunning(
      const FilePathSet& file_paths) override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::optional<ProductMetadata> GetProductMetadata(
      const base::FilePath& file_path) override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

 protected:
  BasePlatformDelegate();
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_BASE_PLATFORM_DELEGATE_H_
