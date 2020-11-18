// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_BLACKLIST_CACHE_UTIL_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_BLACKLIST_CACHE_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"

namespace base {
class Time;
struct MD5Digest;
}  // namespace base

namespace third_party_dlls {
struct PackedListMetadata;
struct PackedListModule;
}  // namespace third_party_dlls

class ModuleListFilter;

// The relative path of the expected module list file inside of an installation
// of this component.
extern const base::FilePath::CharType kModuleListComponentRelativePath[];

// Returns the time date stamp to be used in the module blacklist cache.
// Represents the number of hours between |time| and the Windows epoch
// (1601-01-01 00:00:00 UTC).
uint32_t CalculateTimeDateStamp(base::Time time);

// The possible result value when trying to read an existing module blacklist
// cache. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class ReadResult {
  // A valid cache was successfully read.
  kSuccess = 0,
  // Failed to open the cache file for reading.
  kFailOpenFile = 1,
  // Failed to parse the metadata structure.
  kFailReadMetadata = 2,
  // The version of the cache is not supported by the current version of Chrome.
  kFailInvalidVersion = 3,
  // Failed to read the entire array of PackedListModule.
  kFailReadModules = 4,
  // The cache was rejected because the array was not correctly sorted.
  kFailModulesNotSorted = 5,
  // Failed to read the MD5 digest.
  kFailReadMD5 = 6,
  // The cache was rejected because the MD5 digest did not match the content.
  kFailInvalidMD5 = 7,
  kMaxValue = kFailInvalidMD5
};

// Reads an existing module blacklist cache at |module_blacklist_cache_path|
// into |metadata| and |blacklisted_modules| and return a ReadResult. Failures
// do not modify the out arguments.
ReadResult ReadModuleBlacklistCache(
    const base::FilePath& module_blacklist_cache_path,
    third_party_dlls::PackedListMetadata* metadata,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules,
    base::MD5Digest* md5_digest);

// Writes |metadata| and |blacklisted_modules| to |module_blacklist_cache_path|
// to create a new module blacklist cache file. The MD5 digest of the cache is
// calculated and is returned via the out parameter |md5_digest|. Returns false
// on failure.
//
// Note: |blacklisted_modules| entries must be sorted by their |basename_hash|
//       and their |code_id_hash|, in that order.
bool WriteModuleBlacklistCache(
    const base::FilePath& module_blacklist_cache_path,
    const third_party_dlls::PackedListMetadata& metadata,
    const std::vector<third_party_dlls::PackedListModule>& blacklisted_modules,
    base::MD5Digest* md5_digest);

// Updates an existing list of |blacklisted_modules|. In particular:
//   1. Whitelisted modules are removed.
//      Uses |module_list_filter| to determine if a module is whitelisted.
//   2. Removes expired entries.
//      Uses |max_module_count| and |min_time_date_stamp| to determine which
//      entries should be removed. This step also ensures that enough of the
//      oldest entries are removed to make room for the new modules.
//   3. Updates the |time_date_stamp| of blacklisted modules that attempted to
//      load and were blocked (passed via |blocked_modules|).
//   4. Adds newly blacklisted modules (passed via |newly_blacklisted_modules|).
//   5. Sorts the final list by the |basename_hash| and the |code_id_hash| of
//      each entry.
void UpdateModuleBlacklistCacheData(
    const ModuleListFilter& module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        newly_blacklisted_modules,
    const std::vector<third_party_dlls::PackedListModule>& blocked_modules,
    size_t max_module_count,
    uint32_t min_time_date_stamp,
    third_party_dlls::PackedListMetadata* metadata,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules);

namespace internal {

// Returns the expected file size of the Module Blacklist Cache for the given
// |packed_list_metadata|.
int64_t CalculateExpectedFileSize(
    third_party_dlls::PackedListMetadata packed_list_metadata);

// This comparator returns true if |lhs| should be sorted before |rhs|. Sorts
// modules by their |basename_hash|, and then their |code_id_hash|, ignoring the
// |time_date_stamp| member.
struct ModuleLess {
  bool operator()(const third_party_dlls::PackedListModule& lhs,
                  const third_party_dlls::PackedListModule& rhs) const;
};

// This comparator returns true if the 2 operands refers to the same module,
// ignoring the |time_date_stamp| member.
struct ModuleEqual {
  bool operator()(const third_party_dlls::PackedListModule& lhs,
                  const third_party_dlls::PackedListModule& rhs) const;
};

// This comparator sorts modules by their |time_date_stamp| in descending order.
struct ModuleTimeDateStampGreater {
  bool operator()(const third_party_dlls::PackedListModule& lhs,
                  const third_party_dlls::PackedListModule& rhs) const;
};

// Removes all the entries in |blacklisted_modules| that are whitelisted by the
// ModuleList component.
void RemoveWhitelistedEntries(
    const ModuleListFilter& module_list_filter,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules);

// Updates the |time_date_stamp| of each entry in |blacklisted_modules| that
// also exists in |updated_modules|.
// Precondition: |blacklisted_modules| must be sorted by |basename_hash|, and
// then by |code_id_hash|.
void UpdateModuleBlacklistCacheTimestamps(
    const std::vector<third_party_dlls::PackedListModule>& updated_modules,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules);

// Removes enough elements from the list of modules to ensure that adding all
// the newly blacklisted modules will fit inside the vector without busting the
// maximum size allowed.
// Note: |blacklisted_modules| must be sorted by |time_date_stamp| in descending
// order (use ModuleTimeDateStampGreater).
void RemoveExpiredEntries(
    uint32_t min_time_date_stamp,
    size_t max_module_blacklist_cache_size,
    size_t newly_blacklisted_modules_count,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules);

// Removes duplicates entries in |blacklisted_modules|. Keeps the first
// duplicate of each unique entry.
void RemoveDuplicateEntries(
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules);

}  // namespace internal

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_BLACKLIST_CACHE_UTIL_H_
