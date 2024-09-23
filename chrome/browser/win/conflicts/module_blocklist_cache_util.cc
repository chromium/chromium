// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blocklist_cache_util.h"

#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/md5.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"

namespace {

// Wrapper for base::File::ReadAtCurrentPost() that returns true only if all
// the requested bytes were succesfully read from the file.
bool SafeRead(base::File* file, char* data, int size) {
  return UNSAFE_TODO(file->ReadAtCurrentPos(data, size)) == size;
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
  return base::ranges::equal(lhs.a, rhs.a);
}

// Returns MD5 hash of the cache data.
base::MD5Digest CalculateModuleBlocklistCacheMD5(
    const third_party_dlls::PackedListMetadata& metadata,
    const std::vector<third_party_dlls::PackedListModule>&
        blocklisted_modules) {
  base::MD5Context md5_context;
  base::MD5Init(&md5_context);

  base::MD5Update(&md5_context,
                  std::string_view(reinterpret_cast<const char*>(&metadata),
                                   sizeof(metadata)));
  base::MD5Update(&md5_context,
                  std::string_view(
                      reinterpret_cast<const char*>(blocklisted_modules.data()),
                      sizeof(third_party_dlls::PackedListModule) *
                          blocklisted_modules.size()));

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
  return delta.is_negative() ? 0 : static_cast<uint32_t>(delta.InHours());
}

ReadResult ReadModuleBlocklistCache(
    const base::FilePath& module_blocklist_cache_path,
    third_party_dlls::PackedListMetadata* metadata,
    std::vector<third_party_dlls::PackedListModule>* blocklisted_modules,
    base::MD5Digest* md5_digest) {
  DCHECK(metadata);
  DCHECK(blocklisted_modules);
  DCHECK(md5_digest);

  base::File file(module_blocklist_cache_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ |
                      base::File::FLAG_WIN_SHARE_DELETE);
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

  std::vector<third_party_dlls::PackedListModule> read_blocklisted_modules(
      read_metadata.module_count);
  if (!SafeRead(&file, reinterpret_cast<char*>(read_blocklisted_modules.data()),
                sizeof(third_party_dlls::PackedListModule) *
                    read_metadata.module_count)) {
    return ReadResult::kFailReadModules;
  }

  // The list should be sorted.
  if (!std::is_sorted(read_blocklisted_modules.begin(),
                      read_blocklisted_modules.end(), internal::ModuleLess())) {
    return ReadResult::kFailModulesNotSorted;
  }

  base::MD5Digest read_md5_digest;
  if (!SafeRead(&file, reinterpret_cast<char*>(&read_md5_digest.a),
                std::size(read_md5_digest.a))) {
    return ReadResult::kFailReadMD5;
  }

  if (!IsMD5DigestEqual(read_md5_digest,
                        CalculateModuleBlocklistCacheMD5(
                            read_metadata, read_blocklisted_modules))) {
    return ReadResult::kFailInvalidMD5;
  }

  *metadata = read_metadata;
  *blocklisted_modules = std::move(read_blocklisted_modules);
  *md5_digest = read_md5_digest;
  return ReadResult::kSuccess;
}

bool WriteModuleBlocklistCache(
    const base::FilePath& module_blocklist_cache_path,
    const third_party_dlls::PackedListMetadata& metadata,
    const std::vector<third_party_dlls::PackedListModule>& blocklisted_modules,
    base::MD5Digest* md5_digest) {
  DCHECK(std::is_sorted(blocklisted_modules.begin(), blocklisted_modules.end(),
                        internal::ModuleLess()));

  *md5_digest = CalculateModuleBlocklistCacheMD5(metadata, blocklisted_modules);

  std::string file_contents;
  file_contents.reserve(internal::CalculateExpectedFileSize(metadata));
  file_contents.append(reinterpret_cast<const char*>(&metadata),
                       sizeof(metadata));
  file_contents.append(
      reinterpret_cast<const char*>(blocklisted_modules.data()),
      sizeof(third_party_dlls::PackedListModule) * blocklisted_modules.size());
  file_contents.append(std::begin(md5_digest->a), std::end(md5_digest->a));

  // TODO(crbug.com/40106434): Investigate if using WriteFileAtomically() in a
  // CONTINUE_ON_SHUTDOWN sequence doesn't cause too many corrupted caches.
  return base::ImportantFileWriter::WriteFileAtomically(
      module_blocklist_cache_path, file_contents);
}

void UpdateModuleBlocklistCacheData(
    const ModuleListFilter& module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        newly_blocklisted_modules,
    const std::vector<third_party_dlls::PackedListModule>& blocked_modules,
    size_t max_modules_count,
    uint32_t min_time_date_stamp,
    third_party_dlls::PackedListMetadata* metadata,
    std::vector<third_party_dlls::PackedListModule>* blocklisted_modules) {
  DCHECK(metadata);
  DCHECK(blocklisted_modules);

  // Precondition for UpdateModuleBlocklistCacheTimestamp(). This is guaranteed
  // if the |blocklisted_modules| comes from a valid ModuleBlocklistCache.
  DCHECK(std::is_sorted(blocklisted_modules->begin(),
                        blocklisted_modules->end(), internal::ModuleLess()));

  // Remove allowlisted modules from the ModuleBlocklistCache. This can happen
  // if a module was recently added to the ModuleList's allowlist.
  internal::RemoveAllowlistedEntries(module_list_filter, blocklisted_modules);

  // Update the timestamp of all modules that were blocked for the current
  // browser execution.
  internal::UpdateModuleBlocklistCacheTimestamps(blocked_modules,
                                                 blocklisted_modules);

  // Now remove expired entries. Sorting the collection by reverse time date
  // stamp order makes this operation more efficient. Also removes enough of the
  // oldest entries to make room for the newly blocklisted modules.
  std::sort(blocklisted_modules->begin(), blocklisted_modules->end(),
            internal::ModuleTimeDateStampGreater());

  internal::RemoveExpiredEntries(min_time_date_stamp, max_modules_count,
                                 newly_blocklisted_modules.size(),
                                 blocklisted_modules);

  // Insert all the newly blocklisted modules.
  blocklisted_modules->insert(blocklisted_modules->end(),
                              newly_blocklisted_modules.begin(),
                              newly_blocklisted_modules.end());

  // Sort the collection by its final order, then remove duplicate entries.
  auto module_compare = [](const auto& lhs, const auto& rhs) {
    if (internal::ModuleLess()(lhs, rhs))
      return true;
    if (internal::ModuleLess()(rhs, lhs))
      return false;

    // Ensure the newest duplicates are kept by placing them first.
    return internal::ModuleTimeDateStampGreater()(lhs, rhs);
  };
  std::sort(blocklisted_modules->begin(), blocklisted_modules->end(),
            module_compare);

  internal::RemoveDuplicateEntries(blocklisted_modules);

  // Update the entry count in the metadata structure.
  metadata->version = third_party_dlls::PackedListVersion::kCurrent;
  metadata->module_count = blocklisted_modules->size();
}

namespace internal {

int64_t CalculateExpectedFileSize(
    third_party_dlls::PackedListMetadata packed_list_metadata) {
  return static_cast<int64_t>(sizeof(third_party_dlls::PackedListMetadata) +
                              packed_list_metadata.module_count *
                                  sizeof(third_party_dlls::PackedListModule) +
                              sizeof(base::MD5Digest::a));
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

void RemoveAllowlistedEntries(
    const ModuleListFilter& module_list_filter,
    std::vector<third_party_dlls::PackedListModule>* blocklisted_modules) {
  std::erase_if(
      *blocklisted_modules,
      [&module_list_filter](const third_party_dlls::PackedListModule& module) {
        return module_list_filter.IsAllowlisted(
            std::string_view(
                reinterpret_cast<const char*>(&module.basename_hash[0]),
                std::size(module.basename_hash)),
            std::string_view(
                reinterpret_cast<const char*>(&module.code_id_hash[0]),
                std::size(module.code_id_hash)));
      });
}

void UpdateModuleBlocklistCacheTimestamps(
    const std::vector<third_party_dlls::PackedListModule>& updated_modules,
    std::vector<third_party_dlls::PackedListModule>* blocklisted_modules) {
  DCHECK(std::is_sorted(blocklisted_modules->begin(),
                        blocklisted_modules->end(), ModuleLess()));

  for (const auto& module : updated_modules) {
    auto iter = BinaryFind(blocklisted_modules->begin(),
                           blocklisted_modules->end(), module, ModuleLess());
    if (iter != blocklisted_modules->end())
      iter->time_date_stamp = module.time_date_stamp;
  }
}

void RemoveExpiredEntries(
    uint32_t min_time_date_stamp,
    size_t max_module_blocklist_cache_size,
    size_t newly_blocklisted_modules_count,
    std::vector<third_party_dlls::PackedListModule>* blocklisted_modules) {
  DCHECK(std::is_sorted(blocklisted_modules->begin(),
                        blocklisted_modules->end(),
                        ModuleTimeDateStampGreater()));

  // To make room for newly blocklisted modules, the oldest entries that exceed
  // the max size of the module blocklist cache are considered expired and thus
  // removed.
  size_t new_max_size =
      max_module_blocklist_cache_size - newly_blocklisted_modules_count;
  if (blocklisted_modules->size() > new_max_size)
    blocklisted_modules->resize(new_max_size);

  // Then remove entries whose time date stamp is older than the limit.
  blocklisted_modules->erase(
      std::lower_bound(blocklisted_modules->begin(), blocklisted_modules->end(),
                       min_time_date_stamp,
                       [](const auto& lhs, size_t rhs) {
                         return lhs.time_date_stamp > rhs;
                       }),
      blocklisted_modules->end());
}

void RemoveDuplicateEntries(
    std::vector<third_party_dlls::PackedListModule>* blocklisted_modules) {
  DCHECK(std::is_sorted(blocklisted_modules->begin(),
                        blocklisted_modules->end(), ModuleLess()));

  blocklisted_modules->erase(
      std::unique(blocklisted_modules->begin(), blocklisted_modules->end(),
                  internal::ModuleEqual()),
      blocklisted_modules->end());
}

}  // namespace internal
