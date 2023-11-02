// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TARGET_SANDBOX_SETUP_H_
#define CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TARGET_SANDBOX_SETUP_H_

#include "base/command_line.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "sandbox/win/src/sandbox.h"

namespace chrome_cleaner {

ResultCode RunZipArchiverSandboxTarget(
    const base::CommandLine& command_line,
    sandbox::TargetServices* target_services);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TARGET_SANDBOX_SETUP_H_
