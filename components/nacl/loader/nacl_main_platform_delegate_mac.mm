// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_main_platform_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void NaClMainPlatformDelegate::EnableSandbox(
    const content::MainFunctionParams& parameters) {
  // The sandbox on macOS is enabled as soon as main() executes, so there is
  // nothing to do here.
}
