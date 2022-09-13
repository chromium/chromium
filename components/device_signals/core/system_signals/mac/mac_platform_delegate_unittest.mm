// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/mac/mac_platform_delegate.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#import "base/mac/foundation_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

class MacPlatformDelegateTest : public testing::Test {
 protected:
  MacPlatformDelegate platform_delegate_;
};

TEST_F(MacPlatformDelegateTest, ResolvingBundlePath) {
  // The NSBundle object corresponds to the bundle directory containing the
  // current executable.
  // https://developer.apple.com/documentation/foundation/nsbundle
  NSBundle* main_bundle = [NSBundle mainBundle];
  NSString* bundle_binary_string = main_bundle.executablePath;
  if (!main_bundle || !bundle_binary_string) {
    // This test can only run if a mainBundle is accessible. Not having a main
    // bundle for the test runner should not be considered a failure case
    // though.
    return;
  }

  base::FilePath bundle_path =
      base::mac::NSStringToFilePath(main_bundle.bundlePath);
  base::FilePath binary_path =
      base::mac::NSStringToFilePath(bundle_binary_string);

  // `GetBinaryFilePath` should return the bundle's main executable.
  EXPECT_EQ(platform_delegate_.GetBinaryFilePath(bundle_path), binary_path);

  // `ResolveFilePath` should also point to the bundle's main executable.
  base::FilePath resolved_file_path;
  ASSERT_TRUE(
      platform_delegate_.ResolveFilePath(bundle_path, &resolved_file_path));
  EXPECT_EQ(resolved_file_path, binary_path);
}

TEST_F(MacPlatformDelegateTest, GetBinaryFilePath_BundleNotFound) {
  base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe("/some/file/path/no/bundle");
  EXPECT_EQ(platform_delegate_.GetBinaryFilePath(file_path), file_path);
}

}  // namespace device_signals
