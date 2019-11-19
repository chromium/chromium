// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/launchservices_utils_mac.h"

#include <Foundation/Foundation.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"

namespace test {

bool RegisterAppWithLaunchServices() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char kAppSuffix[] = "Google Chrome.app";
#else
  const char kAppSuffix[] = "Chromium.app";
#endif

  // Try to guess the path to the real org.chromium.Chromium and/or
  // org.google.Chrome bundle if the current main bundle's path isn't already a
  // .app directory:
  NSURL* bundleURL = [[NSBundle mainBundle] bundleURL];
  if (![bundleURL.lastPathComponent hasSuffix:@".app"]) {
    base::FilePath bundle_path;
    if (!base::PathService::Get(base::DIR_EXE, &bundle_path))
      return false;
    bundle_path = bundle_path.Append(kAppSuffix);
    bundleURL = base::mac::FilePathToNSURL(bundle_path);
  }

  if (![NSFileManager.defaultManager fileExistsAtPath:bundleURL.path])
    return false;

  return LSRegisterURL(base::mac::NSToCFCast(bundleURL), false) == noErr;
}

}  // namespace test
