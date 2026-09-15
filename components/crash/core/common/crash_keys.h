// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_

#include <string>
#include <string_view>
#include <vector>

#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_export.h"
#include "components/crash/core/common/crash_key.h"

namespace base {
class CommandLine;
}  // namespace base

namespace crash_keys {

// A convenient wrapper around a crash key and its name.
//
// The CrashKey contract requires that CrashKeyStrings are never moved,
// copied, or deleted (see third_party/crashpad/crashpad/client/annotation.h);
// since this class holds a CrashKeyString, it likewise cannot be moved,
// copied, or deleted.
class CRASH_KEY_EXPORT CrashKeyWithName {
 public:
  explicit CrashKeyWithName(std::string name);
  CrashKeyWithName(const CrashKeyWithName&) = delete;
  CrashKeyWithName& operator=(const CrashKeyWithName&) = delete;
  CrashKeyWithName(CrashKeyWithName&&) = delete;
  CrashKeyWithName& operator=(CrashKeyWithName&&) = delete;
  ~CrashKeyWithName() = delete;

  std::string_view Name() const { return name_; }
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
  std::string_view Value() const { return crash_key_.value(); }
#endif
  void Clear() { crash_key_.Clear(); }
  void Set(std::string_view value) { crash_key_.Set(value); }

 private:
  std::string name_;
  crash_reporter::CrashKeyString<64> crash_key_;
};

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

// Sets "commandline-enabled-feature-1", "commandline-enabled-feature-2", ...
// and "commandline-disabled-feature-1", ... based on the --enable-features and
// --disable-features flags on `command_line`.
CRASH_KEY_EXPORT void SetFeaturesFromCommandLine(
    const base::CommandLine& command_line);

// Default filter function for SetSwitchesFromCommandLine() that ignores
// switches such as --enable-features, --disable-features, and flag sentinels.
CRASH_KEY_EXPORT bool IsDefaultBoringSwitch(const std::string& flag);

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
