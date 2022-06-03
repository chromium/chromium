// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_DUMP_HUNG_PROCESS_WITH_PTYPE_H_
#define COMPONENTS_CRASH_CORE_APP_DUMP_HUNG_PROCESS_WITH_PTYPE_H_

#include "base/process/process.h"

namespace crash_reporter {

// Captures a crash dump for |process|, which is assumed to be hung, to the
// crashpad database. Annotates the crash with suitable annotations, plus a
// "ptype" annotation set to |ptype|.
bool DumpHungProcessWithPtype(const base::Process& process, const char* ptype);

// Implementation function for the above, hosted inside chrome_elf.dll.
bool DumpHungProcessWithPtypeImpl(const base::Process& process,
                                  const char* ptype);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_DUMP_HUNG_PROCESS_WITH_PTYPE_H_
