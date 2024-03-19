// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CRASH_KEYS_H_
#define CHROME_COMMON_CRASH_KEYS_H_

namespace base {
class CommandLine;
}

namespace crash_keys {

// Sets the kNumSwitches key and the set of keys named using kSwitchFormat based
// on the given |command_line|.
void SetCrashKeysFromCommandLine(const base::CommandLine& command_line);

}  // namespace crash_keys

#endif  // CHROME_COMMON_CRASH_KEYS_H_
