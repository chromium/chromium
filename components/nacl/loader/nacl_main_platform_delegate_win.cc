// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_main_platform_delegate.h"

#include "base/check.h"
#include "content/public/common/main_function_params.h"
#include "sandbox/win/src/sandbox.h"

void NaClMainPlatformDelegate::EnableSandbox(
    const content::MainFunctionParams& parameters) {
  sandbox::TargetServices* target_services =
      parameters.sandbox_info->target_services;

  CHECK(target_services) << "NaCl-Win EnableSandbox: No Target Services!";
  // Cause advapi32 to load before the sandbox is turned on.
  unsigned int dummy_rand;
  rand_s(&dummy_rand);

  // Turn the sandbox on.
  target_services->LowerToken();
}
