// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_SETUP_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_SETUP_H_

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/chrome_cleaner/engines/target/engine_delegate.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "sandbox/win/src/sandbox.h"

namespace chrome_cleaner {

ResultCode RunEngineSandboxTarget(
    scoped_refptr<EngineDelegate> engine_delegate,
    const base::CommandLine& command_line,
    sandbox::TargetServices* sandbox_target_services);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_SETUP_H_
