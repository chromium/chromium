// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROCESS_SINGLETON_LOCK_POSIX_H_
#define CHROME_COMMON_PROCESS_SINGLETON_LOCK_POSIX_H_

#include <string>

#include "base/files/file_path.h"

// Extract the hostname and pid from the lock symlink. Returns true if the lock
// existed. See ProcessSingleton for additional details.
bool ParseProcessSingletonLock(const base::FilePath& path,
                               std::string* hostname,
                               int* pid);

extern const char kProcessSingletonLockDelimiter;

#endif  // CHROME_COMMON_PROCESS_SINGLETON_LOCK_POSIX_H_
