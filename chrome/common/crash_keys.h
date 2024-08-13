// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CRASH_KEYS_H_
#define CHROME_COMMON_CRASH_KEYS_H_

#include <string_view>

namespace base {
class CommandLine;
}

namespace crash_keys {

// This function is only expected to be called in the browser process. Allocates
// a crash key in this process. The (key, value) pair will be included in the
// switch value set by AppendStringAnnotationsCommandLineSwitch().
void AllocateCrashKeyInBrowserAndChildren(std::string_view key,
                                          std::string_view value);

// Extends `command_line` with a switch containing crash key values allocated by
// AllocateCrashKeyInBrowserAndChildren() which can be consumed by
// SetCrashKeysFromCommandLine().
void AppendStringAnnotationsCommandLineSwitch(base::CommandLine* command_line);

// Sets the kNumSwitches key and the set of keys named using kSwitchFormat based
// on the given `command_line`. For non-browser processes, allocates crash keys
// from the switch value set by AppendStringAnnotationsCommandLineSwitch().
void SetCrashKeysFromCommandLine(const base::CommandLine& command_line);

}  // namespace crash_keys

#endif  // CHROME_COMMON_CRASH_KEYS_H_
