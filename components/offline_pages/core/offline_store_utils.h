// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_STORE_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_STORE_UTILS_H_

#include <stdint.h>

#include <string>

namespace base {
class Time;
class FilePath;
}  // namespace base

namespace offline_pages {

// The store_utils namespace within offline_pages namespace contains helper
// methods that are common and shared among all Offline Pages projects.
namespace store_utils {

// Time format conversion methods between base::Time and an int64_t, which is
// used by database storage indicating the elapsed time in microseconds since
// the Windows epoch.
int64_t ToDatabaseTime(base::Time time);
base::Time FromDatabaseTime(int64_t serialized_time);

// File path conversion methods between base::FilePath and UTF8 string formats
// for storing paths in the database.
std::string ToDatabaseFilePath(const base::FilePath& file_path);
base::FilePath FromDatabaseFilePath(const std::string& file_path_string);

// Generates a positive random int64_t, generally used for offline ids.
int64_t GenerateOfflineId();

}  // namespace store_utils

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_STORE_UTILS_H_
