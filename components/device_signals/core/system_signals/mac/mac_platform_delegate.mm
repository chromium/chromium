// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/mac/mac_platform_delegate.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#import "base/mac/foundation_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {

namespace {

// Verifies if `file_path` points to an app bundle and then returns the
// executable path for the file inside the bundle. If `file_path` does not
// point to a bundle, it is returned as-is.
base::FilePath GetBinaryFilePath(const base::FilePath& file_path) {
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

// Attempts to parse out the bundle path from a binary absolute path. Returns
// absl::nullopt if there is no bundle path in `file_path`. Since bundle paths
// are solely used to get information about the bundle, the "path X is a bundle"
// heuristic is based on whether bundle information is available at path X.
absl::optional<base::FilePath> GetBundleFilePath(
    const base::FilePath& file_path) {
  @autoreleasepool {
    base::FilePath current_path = file_path;
    do {
      NSString* current_path_string =
          base::mac::FilePathToNSString(current_path);
      NSBundle* bundle = [NSBundle bundleWithPath:current_path_string];
      if (bundle.infoDictionary.count > 0) {
        // Current path points to a bundle that has metadata.
        return current_path;
      }
      current_path = current_path.DirName();
    } while (current_path.GetComponents().size() > 1);

    return absl::nullopt;
  }
}

}  // namespace

MacPlatformDelegate::MacPlatformDelegate() = default;

MacPlatformDelegate::~MacPlatformDelegate() = default;

bool MacPlatformDelegate::ResolveFilePath(const base::FilePath& file_path,
                                          base::FilePath* resolved_file_path) {
  base::FilePath temp_file_path;
  if (!PosixPlatformDelegate::ResolveFilePath(file_path, &temp_file_path)) {
    return false;
  }

  // Since `file_path` might point to an app bundle, resolve that path to point
  // to the binary too. This step is an optimization which will help keep the
  // "isRunning" logic platform agnostic.
  *resolved_file_path = GetBinaryFilePath(temp_file_path);
  return true;
}

absl::optional<PlatformDelegate::ProductMetadata>
MacPlatformDelegate::GetProductMetadata(const base::FilePath& file_path) {
  // The implementation in BasePlatformDelegate requires that the given path
  // points to a bundle.
  auto bundle_path = GetBundleFilePath(file_path);
  return BasePlatformDelegate::GetProductMetadata(
      bundle_path.value_or(file_path));
}

absl::optional<std::vector<std::string>>
MacPlatformDelegate::GetSigningCertificatesPublicKeyHashes(
    const base::FilePath& file_path) {
  // TODO(b:231326198): Implement.
  return absl::nullopt;
}

}  // namespace device_signals
