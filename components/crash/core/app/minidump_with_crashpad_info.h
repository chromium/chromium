// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_MINIDUMP_WITH_CRASHPAD_INFO_H_
#define COMPONENTS_CRASH_CORE_APP_MINIDUMP_WITH_CRASHPAD_INFO_H_

// Needed for dbghelp.h.
#include <windows.h>

#include <dbghelp.h>

#include <map>
#include <string>

#include "base/files/file.h"
#include "base/process/process.h"

namespace crash_reporter {

using StringStringMap = std::map<std::string, std::string>;

// Captures a crash dump for |process| to the crashpad database at
// |crashpad_database_path|. The crash dump will contain |crash_keys|,
// which must at minimum include the following keys:
// - prod: product name, e.g. "Chrome"
// - ver: product version, e.g. "1.2.3.4"
bool DumpAndReportProcess(const base::Process& process,
                          uint32_t minidump_type,
                          MINIDUMP_EXCEPTION_INFORMATION* exc_info,
                          const StringStringMap& crash_keys,
                          const base::FilePath& crashpad_database_path);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_MINIDUMP_WITH_CRASHPAD_INFO_H_
