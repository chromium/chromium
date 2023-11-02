// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CRASH_CRASH_KEYS_H_
#define CHROME_CHROME_CLEANER_CRASH_CRASH_KEYS_H_

#include "base/strings/string_piece_forward.h"

namespace crashpad {

// Forward-declare CrashpadInfo instead of including crashpad_info.h because
// the header pulls in extra dependencies that would need to be inherited by
// every file using crash_keys.h.
struct CrashpadInfo;

}  // namespace crashpad

namespace chrome_cleaner {

// Sets the crash key |key| to the specified |value|. The key will be
// overwritten if it was already present. This is thread-safe.
void SetCrashKey(base::StringPiece key, base::StringPiece value);

// Records the current process's command-line in a set of crash keys. This is
// thread-safe.
void SetCrashKeysFromCommandLine();

// Sets |crashpad_info| to use this process's crash key dictionary for
// annotations. Note the annotations are not used in a thread-safe way by
// Crashpad, but that should be acceptable because they are only used while
// dumping a crash.
void UseCrashKeysToAnnotate(crashpad::CrashpadInfo* crashpad_info);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CRASH_CRASH_KEYS_H_
