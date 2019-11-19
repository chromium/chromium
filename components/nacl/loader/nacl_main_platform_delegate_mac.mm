// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_main_platform_delegate.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "services/service_manager/sandbox/sandbox_type.h"

void NaClMainPlatformDelegate::EnableSandbox(
    const content::MainFunctionParams& parameters) {
  // The sandbox on macOS is enabled as soon as main() executes, so there is
  // nothing to do here.
}
