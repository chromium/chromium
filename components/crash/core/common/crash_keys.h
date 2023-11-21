// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_

#include <string>
#include <vector>

#include "components/crash/core/common/crash_export.h"

namespace base {
class CommandLine;
}  // namespace base

namespace crash_keys {

// Sets the ID (which may either be a full GUID or a GUID that was already
// stripped from its dashes -- in either case this method will strip remaining
// dashes before setting the crash key).
void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid);
void ClearMetricsClientId();

// A function returning true if |flag| is a switch that should be filtered out
// of crash keys.
using SwitchFilterFunction = bool (*)(const std::string& flag);

// Sets the "num-switches" key and a set of keys named using kSwitchFormat based
// on the given |command_line|. If |skip_filter| is not null, ignore any switch
// for which it returns true.
void CRASH_KEY_EXPORT
SetSwitchesFromCommandLine(const base::CommandLine& command_line,
                           SwitchFilterFunction skip_filter);

// Clears all the CommandLine-related crash keys.
void ResetCommandLineForTesting();

// Sets the printer info. `data` should contain no more than 4 strings.
// Each string might get truncated if necessary.
// If `data` is empty then the `printer_name` will be used.  This provides some
// minimal information when there are issues getting the printer's info.
class ScopedPrinterInfo {
 public:
  ScopedPrinterInfo(const std::string& printer_name,
                    std::vector<std::string> data);

  ScopedPrinterInfo(const ScopedPrinterInfo&) = delete;
  ScopedPrinterInfo& operator=(const ScopedPrinterInfo&) = delete;

  ~ScopedPrinterInfo();
};

}  // namespace crash_keys

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
