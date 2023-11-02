// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_CRASH_TESTING_UTILS_H_
#define CHROMECAST_CRASH_LINUX_CRASH_TESTING_UTILS_H_

#include <memory>
#include <vector>

#include "base/time/time.h"

namespace chromecast {

class DumpInfo;

// Creates a DumpInfo object corresponding to the deserialization of
// |json_string|. Returned DumpInfo object maybe invalid if |json_string|
// doesn't correspond to a valid DumpInfo object.
std::unique_ptr<DumpInfo> CreateDumpInfo(const std::string& json_string);

// Populates |dumps| with all the DumpInfo entries serialized in the lockfile at
// |lockfile_path|. Returns true on success, false on error.
bool FetchDumps(const std::string& lockfile_path,
                std::vector<std::unique_ptr<DumpInfo>>* dumps);

// Clear all dumps in the lockfile at |lockfile_path|.
// Returns true on success, false on error.
bool ClearDumps(const std::string& lockfile_path);

// Creates an empty lockfile at |lockfile_path|. Creates a default initialized
// metadata file at |metadata_path|. Returns true on success, false on error.
bool CreateFiles(const std::string& lockfile_path,
                 const std::string& metadata_path);

// Appends serialization of |dump| onto the lockfile at |lockfile_path|.
// Creates default initialized lockfile in |lockfile_path| and metadata file in
// |metadata_path| if they don't exist.
// Returns true on success, false on error.
bool AppendLockFile(const std::string& lockfile_path,
                    const std::string& metadata_path,
                    const DumpInfo& dump);

// Set the ratelimit period start in the metadata file at |metadata_path| to
// |start|. Returns true on success, false on error.
bool SetRatelimitPeriodStart(const std::string& metadata_path,
                             const base::Time& start);

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_CRASH_TESTING_UTILS_H_
