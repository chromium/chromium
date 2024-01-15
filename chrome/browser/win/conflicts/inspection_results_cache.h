// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_INSPECTION_RESULTS_CACHE_H_
#define CHROME_BROWSER_WIN_CONFLICTS_INSPECTION_RESULTS_CACHE_H_

#include <map>
#include <optional>
#include <utility>

#include "chrome/browser/win/conflicts/module_info.h"

namespace base {
class FilePath;
}

// The possible result value when trying to read an existing inspection results
// cache. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class ReadCacheResult {
  // A valid cache was successfully read.
  kSuccess = 0,
  // Failed to read the content of the file.
  kFailReadFile = 1,
  // Failed to deserialize the version number.
  kFailDeserializeVersion = 2,
  // The version of the cache was invalid.
  kFailInvalidVersion = 3,
  // Failed to deserialize the number of inspection results the cache contains.
  kFailDeserializeCount = 4,
  // A negative count was encountered.
  kFailInvalidCount = 5,
  // Failed to deserialize an inspection result.
  kFailDeserializeInspectionResult = 6,
  // Failed to deserialize the MD5 digest.
  kFailDeserializeMD5 = 7,
  // The cache was rejected because the MD5 digest did not match the content.
  kFailInvalidMD5 = 8,
  kMaxValue = kFailInvalidMD5
};

// The InspectionResultsCache maps ModuleInfoKey to a ModuleInspectionResult.
// The uint32_t is a time stamp that keep tracks of when the inspection result
// was needed (i.e. It was queried using GetInspectionResultFromCache() or added
// using AddInspectionResultToCache()) calculated by CalculateTimeStamp().
using InspectionResultsCache =
    std::map<ModuleInfoKey, std::pair<ModuleInspectionResult, uint32_t>>;

// Helper function to add an inspection result to an existing cache.
void AddInspectionResultToCache(
    const ModuleInfoKey& module_key,
    const ModuleInspectionResult& inspection_result,
    InspectionResultsCache* inspection_results_cache);

// Helper function to retrieve a ModuleInspectionResult from an existing cache.
// Also updates the time stamp of the element found to base::Time::Now().
// Returns std::nullopt if the cache does not contains an entry for
// |module_key|.
std::optional<ModuleInspectionResult> GetInspectionResultFromCache(
    const ModuleInfoKey& module_key,
    InspectionResultsCache* inspection_results_cache);

// Reads a serialized InspectionResultsCache from |file_path|. Returns a
// ReadCacheResult value indicating what failed if unsuccessful.
ReadCacheResult ReadInspectionResultsCache(
    const base::FilePath& file_path,
    uint32_t min_time_stamp,
    InspectionResultsCache* inspection_results_cache);

// Writes an InspectionResultsCache to disk at |file_path| location. Returns
// false on failure.
bool WriteInspectionResultsCache(
    const base::FilePath& file_path,
    const InspectionResultsCache& inspection_results_cache);

#endif  // CHROME_BROWSER_WIN_CONFLICTS_INSPECTION_RESULTS_CACHE_H_
