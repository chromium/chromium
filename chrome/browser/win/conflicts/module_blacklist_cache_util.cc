// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blacklist_cache_util.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/md5.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"

namespace {

// Wrapper for base::File::ReadAtCurrentPost() that returns true only if all
// the requested bytes were succesfully read from the file.
bool SafeRead(base::File* file, char* data, int size) {
  return file->ReadAtCurrentPos(data, size) == size;
}

// Returns an iterator to the element equal to |value|, or |last| if it can't
// be found.
template <class ForwardIt, class T, class Compare = std::less<>>
ForwardIt BinaryFind(ForwardIt first,
                     ForwardIt last,
                     const T& value,
                     Compare comp = {}) {
  first = std::lower_bound(first, last, value, comp);
  return first != last && !comp(value, *first) ? first : last;
}

// Returns true if the 2 digests are equal.
bool IsMD5DigestEqual(const base::MD5Digest& lhs, const base::MD5Digest& rhs) {
  return std::equal(std::begin(lhs.a), std::end(lhs.a), std::begin(rhs.a),
                    std::end(rhs.a));
}

// Returns MD5 hash of the cache data.
base::MD5Digest CalculateModuleBlacklistCacheMD5(
    const third_party_dlls::PackedListMetadata& metadata,
    const std::vector<third_party_dlls::PackedListModule>&
        blacklisted_modules) {
  base::MD5Context md5_context;
  base::MD5Init(&md5_context);

  base::MD5Update(&md5_context,
                  base::StringPiece(reinterpret_cast<const char*>(&metadata),
                                    sizeof(metadata)));
  base::MD5Update(&md5_context,
                  base::StringPiece(
                      reinterpret_cast<const char*>(blacklisted_modules.data()),
                      sizeof(third_party_dlls::PackedListModule) *
                          blacklisted_modules.size()));

  base::MD5Digest md5_digest;
  base::MD5Final(&md5_digest, &md5_context);
  return md5_digest;
}

}  // namespace

const base::FilePath::CharType kModuleListComponentRelativePath[] =
    FILE_PATH_LITERAL("ThirdPartyModuleList")
#ifdef _WIN64
        FILE_PATH_LITERAL("64");
#else
        FILE_PATH_LITERAL("32");
#endif

uint32_t CalculateTimeDateStamp(base::Time time) {
  const auto delta = time.ToDeltaSinceWindowsEpoch();
  return delta < base::TimeDelta() ? 0 : static_cast<uint32_t>(delta.InHours());
}

ReadResult ReadModuleBlacklistCache(
    const base::FilePath& module_blacklist_cache_path,
    third_party_dlls::PackedListMetadata* metadata,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules,
    base::MD5Digest* md5_digest) {
  DCHECK(metadata);
  DCHECK(blacklisted_modules);
  DCHECK(md5_digest);

  base::File file(module_blacklist_cache_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ |
                      base::File::FLAG_SHARE_DELETE);
  if (!file.IsValid())
    return ReadResult::kFailOpenFile;

  third_party_dlls::PackedListMetadata read_metadata;
  if (!SafeRead(&file, reinterpret_cast<char*>(&read_metadata),
                sizeof(read_metadata))) {
    return ReadResult::kFailReadMetadata;
  }

  // Make sure the version is supported.
  if (read_metadata.version > third_party_dlls::PackedListVersion::kCurrent)
    return ReadResult::kFailInvalidVersion;

  std::vector<third_party_dlls::PackedListModule> read_blacklisted_modules(
      read_metadata.module_count);
  if (!SafeRead(&file, reinterpret_cast<char*>(read_blacklisted_modules.data()),
                sizeof(third_party_dlls::PackedListModule) *
                    read_metadata.module_count)) {
    return ReadResult::kFailReadModules;
  }

  // The list should be sorted.
  if (!std::is_sorted(read_blacklisted_modules.begin(),
                      read_blacklisted_modules.end(), internal::ModuleLess())) {
    return ReadResult::kFailModulesNotSorted;
  }

  base::MD5Digest read_md5_digest;
  if (!SafeRead(&file, reinterpret_cast<char*>(&read_md5_digest.a),
                base::size(read_md5_digest.a))) {
    return ReadResult::kFailReadMD5;
  }

  if (!IsMD5DigestEqual(read_md5_digest,
                        CalculateModuleBlacklistCacheMD5(
                            read_metadata, read_blacklisted_modules))) {
    return ReadResult::kFailInvalidMD5;
  }

  *metadata = read_metadata;
  *blacklisted_modules = std::move(read_blacklisted_modules);
  *md5_digest = read_md5_digest;
  return ReadResult::kSuccess;
}

bool WriteModuleBlacklistCache(
    const base::FilePath& module_blacklist_cache_path,
    const third_party_dlls::PackedListMetadata& metadata,
    const std::vector<third_party_dlls::PackedListModule>& blacklisted_modules,
    base::MD5Digest* md5_digest) {
  DCHECK(std::is_sorted(blacklisted_modules.begin(), blacklisted_modules.end(),
                        internal::ModuleLess()));

  *md5_digest = CalculateModuleBlacklistCacheMD5(metadata, blacklisted_modules);

  std::string file_contents;
  file_contents.reserve(internal::CalculateExpectedFileSize(metadata));
  file_contents.append(reinterpret_cast<const char*>(&metadata),
                       sizeof(metadata));
  file_contents.append(
      reinterpret_cast<const char*>(blacklisted_modules.data()),
      sizeof(third_party_dlls::PackedListModule) * blacklisted_modules.size());
  file_contents.append(std::begin(md5_digest->a), std::end(md5_digest->a));

  // TODO(1022041): Investigate if using WriteFileAtomically() in a
  // CONTINUE_ON_SHUTDOWN sequence doesn't cause too many corrupted caches.
  return base::ImportantFileWriter::WriteFileAtomically(
      module_blacklist_cache_path, file_contents);
}

void UpdateModuleBlacklistCacheData(
    const ModuleListFilter& module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        newly_blacklisted_modules,
    const std::vector<third_party_dlls::PackedListModule>& blocked_modules,
    size_t max_modules_count,
    uint32_t min_time_date_stamp,
    third_party_dlls::PackedListMetadata* metadata,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules) {
  DCHECK(metadata);
  DCHECK(blacklisted_modules);

  // Precondition for UpdateModuleBlacklistCacheTimestamp(). This is guaranteed
  // if the |blacklisted_modules| comes from a valid ModuleBlacklistCache.
  DCHECK(std::is_sorted(blacklisted_modules->begin(),
                        blacklisted_modules->end(), internal::ModuleLess()));

  // Remove whitelisted modules from the ModuleBlacklistCache. This can happen
  // if a module was recently added to the ModuleList's whitelist.
  internal::RemoveWhitelistedEntries(module_list_filter, blacklisted_modules);

  // Update the timestamp of all modules that were blocked for the current
  // browser execution.
  internal::UpdateModuleBlacklistCacheTimestamps(blocked_modules,
                                                 blacklisted_modules);

  // Now remove expired entries. Sorting the collection by reverse time date
  // stamp order makes this operation more efficient. Also removes enough of the
  // oldest entries to make room for the newly blacklisted modules.
  std::sort(blacklisted_modules->begin(), blacklisted_modules->end(),
            internal::ModuleTimeDateStampGreater());

  internal::RemoveExpiredEntries(min_time_date_stamp, max_modules_count,
                                 newly_blacklisted_modules.size(),
                                 blacklisted_modules);

  // Insert all the newly blacklisted modules.
  blacklisted_modules->insert(blacklisted_modules->end(),
                              newly_blacklisted_modules.begin(),
                              newly_blacklisted_modules.end());

  // Sort the collection by its final order, then remove duplicate entries.
  auto module_compare = [](const auto& lhs, const auto& rhs) {
    if (internal::ModuleLess()(lhs, rhs))
      return true;
    if (internal::ModuleLess()(rhs, lhs))
      return false;

    // Ensure the newest duplicates are kept by placing them first.
    return internal::ModuleTimeDateStampGreater()(lhs, rhs);
  };
  std::sort(blacklisted_modules->begin(), blacklisted_modules->end(),
            module_compare);

  internal::RemoveDuplicateEntries(blacklisted_modules);

  // Update the entry count in the metadata structure.
  metadata->version = third_party_dlls::PackedListVersion::kCurrent;
  metadata->module_count = blacklisted_modules->size();
}

namespace internal {

int64_t CalculateExpectedFileSize(
    third_party_dlls::PackedListMetadata packed_list_metadata) {
  return static_cast<int64_t>(sizeof(third_party_dlls::PackedListMetadata) +
                              packed_list_metadata.module_count *
                                  sizeof(third_party_dlls::PackedListModule) +
                              std::extent<decltype(base::MD5Digest::a)>());
}

bool ModuleLess::operator()(
    const third_party_dlls::PackedListModule& lhs,
    const third_party_dlls::PackedListModule& rhs) const {
  return std::tie(lhs.basename_hash, lhs.code_id_hash) <
         std::tie(rhs.basename_hash, rhs.code_id_hash);
}

bool ModuleEqual::operator()(
    const third_party_dlls::PackedListModule& lhs,
    const third_party_dlls::PackedListModule& rhs) const {
  return lhs.basename_hash == rhs.basename_hash &&
         lhs.code_id_hash == rhs.code_id_hash;
}

bool ModuleTimeDateStampGreater::operator()(
    const third_party_dlls::PackedListModule& lhs,
    const third_party_dlls::PackedListModule& rhs) const {
  return lhs.time_date_stamp > rhs.time_date_stamp;
}

void RemoveWhitelistedEntries(
    const ModuleListFilter& module_list_filter,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules) {
  base::EraseIf(
      *blacklisted_modules,
      [&module_list_filter](const third_party_dlls::PackedListModule& module) {
        return module_list_filter.IsWhitelisted(
            base::StringPiece(
                reinterpret_cast<const char*>(&module.basename_hash[0]),
                base::size(module.basename_hash)),
            base::StringPiece(
                reinterpret_cast<const char*>(&module.code_id_hash[0]),
                base::size(module.code_id_hash)));
      });
}

void UpdateModuleBlacklistCacheTimestamps(
    const std::vector<third_party_dlls::PackedListModule>& updated_modules,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules) {
  DCHECK(std::is_sorted(blacklisted_modules->begin(),
                        blacklisted_modules->end(), ModuleLess()));

  for (const auto& module : updated_modules) {
    auto iter = BinaryFind(blacklisted_modules->begin(),
                           blacklisted_modules->end(), module, ModuleLess());
    if (iter != blacklisted_modules->end())
      iter->time_date_stamp = module.time_date_stamp;
  }
}

void RemoveExpiredEntries(
    uint32_t min_time_date_stamp,
    size_t max_module_blacklist_cache_size,
    size_t newly_blacklisted_modules_count,
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules) {
  DCHECK(std::is_sorted(blacklisted_modules->begin(),
                        blacklisted_modules->end(),
                        ModuleTimeDateStampGreater()));

  // To make room for newly blacklisted modules, the oldest entries that exceed
  // the max size of the module blacklist cache are considered expired and thus
  // removed.
  size_t new_max_size =
      max_module_blacklist_cache_size - newly_blacklisted_modules_count;
  if (blacklisted_modules->size() > new_max_size)
    blacklisted_modules->resize(new_max_size);

  // Then remove entries whose time date stamp is older than the limit.
  blacklisted_modules->erase(
      std::lower_bound(blacklisted_modules->begin(), blacklisted_modules->end(),
                       min_time_date_stamp,
                       [](const auto& lhs, size_t rhs) {
                         return lhs.time_date_stamp > rhs;
                       }),
      blacklisted_modules->end());
}

void RemoveDuplicateEntries(
    std::vector<third_party_dlls::PackedListModule>* blacklisted_modules) {
  DCHECK(std::is_sorted(blacklisted_modules->begin(),
                        blacklisted_modules->end(), ModuleLess()));

  blacklisted_modules->erase(
      std::unique(blacklisted_modules->begin(), blacklisted_modules->end(),
                  internal::ModuleEqual()),
      blacklisted_modules->end());
}

}  // namespace internal
