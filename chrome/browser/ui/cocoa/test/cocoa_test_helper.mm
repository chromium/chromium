// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"

#include "base/mac/bundle_locations.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"

CocoaTestHelper::CocoaTestHelper() {
  CocoaTest::BootstrapCocoa();
}

CocoaTestHelper::~CocoaTestHelper() = default;

CocoaTest::CocoaTest() {
  BootstrapCocoa();
}

void CocoaTest::BootstrapCocoa() {
  // Look in the framework bundle for resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::mac::SetOverrideFrameworkBundlePath(path);
}
