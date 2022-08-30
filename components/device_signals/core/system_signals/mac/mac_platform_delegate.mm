// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/mac/mac_platform_delegate.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#import "base/mac/foundation_util.h"

namespace device_signals {

MacPlatformDelegate::MacPlatformDelegate() = default;

MacPlatformDelegate::~MacPlatformDelegate() = default;

bool MacPlatformDelegate::ResolveFilePath(const base::FilePath& file_path,
                                          base::FilePath* resolved_file_path) {
  if (!PosixPlatformDelegate::ResolveFilePath(file_path, resolved_file_path)) {
    return false;
  }

  // Since `file_path` might point to an app bundle, resolve that path to point
  // to the binary too.
  *resolved_file_path = GetBinaryFilePath(*resolved_file_path);
  return true;
}

base::FilePath MacPlatformDelegate::GetBinaryFilePath(
    const base::FilePath& file_path) {
  // Try to load the path into a bundle.
  NSBundle* bundle =
      [NSBundle bundleWithPath:base::mac::FilePathToNSString(file_path)];
  if (bundle) {
    NSString* executable_path = bundle.executablePath;
    if (executable_path) {
      return base::mac::NSStringToFilePath(executable_path);
    }
  }

  return file_path;
}

}  // namespace device_signals
