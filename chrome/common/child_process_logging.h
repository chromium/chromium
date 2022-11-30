// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
#define CHROME_COMMON_CHILD_PROCESS_LOGGING_H_

#include "build/build_config.h"

namespace child_process_logging {

#if BUILDFLAG(IS_WIN)
// Sets up the base/debug/crash_logging.h mechanism.
void Init();
#endif  // BUILDFLAG(IS_WIN)

}  // namespace child_process_logging

#endif  // CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
