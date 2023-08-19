// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/launchservices_utils_mac.h"

#include <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"

namespace test {

bool RegisterAppWithLaunchServices() {
  NSURL* bundleURL = base::apple::FilePathToNSURL(GuessAppBundlePath());

  if (![bundleURL checkResourceIsReachableAndReturnError:nil]) {
    return false;
  }

  return LSRegisterURL(base::apple::NSToCFPtrCast(bundleURL),
                       /*inUpdate=*/false) == noErr;
}

base::FilePath GuessAppBundlePath() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char kAppSuffix[] = "Google Chrome.app";
#else
  const char kAppSuffix[] = "Chromium.app";
#endif

  // Try to guess the path to the real org.chromium.Chromium and/or
  // org.google.Chrome bundle if the current main bundle's path isn't already a
  // .app directory:
  NSURL* bundleURL = NSBundle.mainBundle.bundleURL;
  if ([bundleURL.lastPathComponent hasSuffix:@".app"]) {
    return base::apple::NSURLToFilePath(bundleURL);
  }

  base::FilePath exe_path;
  if (!base::PathService::Get(base::DIR_EXE, &exe_path)) {
    return base::FilePath();
  }
  return exe_path.Append(kAppSuffix);
}

}  // namespace test
