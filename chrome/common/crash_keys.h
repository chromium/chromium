// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CRASH_KEYS_H_
#define CHROME_COMMON_CRASH_KEYS_H_

#include <set>
#include <string>

#include "base/strings/string_piece.h"

namespace base {
class CommandLine;
}

namespace crash_keys {

// Sets the kNumSwitches key and the set of keys named using kSwitchFormat based
// on the given |command_line|.
void SetCrashKeysFromCommandLine(const base::CommandLine& command_line);

// Sets the list of "active" extensions in this process. We overload "active" to
// mean different things depending on the process type:
// - browser: all enabled extensions
// - renderer: the unique set of extension ids from all content scripts
// - extension: the id of each extension running in this process (there can be
//   multiple because of process collapsing).
void SetActiveExtensions(const std::set<std::string>& extensions);

}  // namespace crash_keys

#endif  // CHROME_COMMON_CRASH_KEYS_H_
