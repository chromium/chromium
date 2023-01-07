// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_store_utils.h"

#include <limits>
#include <string>

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"

namespace offline_pages {

namespace store_utils {

int64_t ToDatabaseTime(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

base::Time FromDatabaseTime(int64_t serialized_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(serialized_time));
}

std::string ToDatabaseFilePath(const base::FilePath& file_path) {
  return file_path.AsUTF8Unsafe();
}

base::FilePath FromDatabaseFilePath(const std::string& file_path_string) {
  return base::FilePath::FromUTF8Unsafe(file_path_string);
}

int64_t GenerateOfflineId() {
  // This is guaranteed to return positive since RandGenerator returns uint64_t.
  return base::RandGenerator(std::numeric_limits<int64_t>::max()) + 1;
}

}  // namespace store_utils

}  // namespace offline_pages
