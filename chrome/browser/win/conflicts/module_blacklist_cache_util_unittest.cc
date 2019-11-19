// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blacklist_cache_util.h"

#include <algorithm>
#include <memory>
#include <random>
#include <set>
#include <type_traits>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/chrome_elf/sha1/sha1.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Creates a list of unique random modules.
std::vector<third_party_dlls::PackedListModule> CreateUniqueModuleEntries(
    uint32_t entry_count,
    uint32_t min_time_date_stamp,
    uint32_t max_time_date_stamp) {
  // A pseudo-random number generator that explicitely uses the default seed to
  // make the tests deterministic.
  std::mt19937 random_engine(std::mt19937::default_seed);

  // A distribution to generate bytes and another one to generate time date
  // stamp values.
  std::uniform_int_distribution<unsigned int> byte_distribution(0u, 255u);
  std::uniform_int_distribution<unsigned int> time_date_stamp_distribution(
      min_time_date_stamp, max_time_date_stamp);

  std::vector<third_party_dlls::PackedListModule> entries(entry_count);

  for (auto& entry : entries) {
    // Fill up each bytes for both SHA1 hashes.
    for (size_t i = 0; i < elf_sha1::kSHA1Length; ++i) {
      entry.basename_hash[i] = byte_distribution(random_engine);
      entry.code_id_hash[i] = byte_distribution(random_engine);
    }
    entry.time_date_stamp = time_date_stamp_distribution(random_engine);
  }

  // Sort the entries and make sure each module is unique.
  std::sort(entries.begin(), entries.end(), internal::ModuleLess());
  CHECK(std::adjacent_find(entries.begin(), entries.end(),
                           internal::ModuleEqual()) == entries.end());

  return entries;
}

// Calls the |p| binary predicate for a sample of the collection.
template <typename T, typename Predicate>
void SampleBlacklistedModules(size_t count,
                              Predicate p,
                              std::vector<T>* collection) {
  size_t to_skip = collection->size() / count;
  for (size_t i = 0; i < count; ++i)
    p((*collection)[i * to_skip]);
}

}  // namespace

class ModuleBlacklistCacheUtilTest : public testing::Test {
 protected:
  ModuleBlacklistCacheUtilTest() = default;
  ~ModuleBlacklistCacheUtilTest() override = default;

  // The number of module entry created for each test.
  enum { kTestModuleCount = 500u };

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    module_blacklist_cache_path_ = scoped_temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ModuleBlacklistCache"));
  }

  const base::FilePath& module_blacklist_cache_path() const {
    return module_blacklist_cache_path_;
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath module_blacklist_cache_path_;

  DISALLOW_COPY_AND_ASSIGN(ModuleBlacklistCacheUtilTest);
};

TEST_F(ModuleBlacklistCacheUtilTest, CalculateTimeDateStamp) {
  base::Time::Exploded chrome_birthday = {};
  chrome_birthday.year = 2008;
  chrome_birthday.month = 9;        // September.
  chrome_birthday.day_of_week = 2;  // Tuesday.
  chrome_birthday.day_of_month = 2;

  base::Time time;
  ASSERT_TRUE(chrome_birthday.HasValidValues());
  ASSERT_TRUE(base::Time::FromUTCExploded(chrome_birthday, &time));

  // Ensure that CalculateTimeDateStamp() will always return the number of
  // hours between |time| and the Windows epoch.
  EXPECT_EQ(3573552u, CalculateTimeDateStamp(time));
}

TEST_F(ModuleBlacklistCacheUtilTest, WriteEmptyCache) {
  third_party_dlls::PackedListMetadata metadata = {
      third_party_dlls::PackedListVersion::kCurrent, 0};
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_TRUE(WriteModuleBlacklistCache(module_blacklist_cache_path(), metadata,
                                        blacklisted_modules, &md5_digest));

  // Check the file's stat.
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(module_blacklist_cache_path(), &file_size));
  EXPECT_EQ(file_size, internal::CalculateExpectedFileSize(metadata));

  base::MD5Digest expected = {
      0x33, 0xCD, 0xEC, 0xCC, 0xCE, 0xBE, 0x80, 0x32,
      0x9F, 0x1F, 0xDB, 0xEE, 0x7F, 0x58, 0x74, 0xCB,
  };

  for (size_t i = 0; i < std::extent<decltype(base::MD5Digest::a)>(); ++i)
    EXPECT_EQ(expected.a[i], md5_digest.a[i]);
}

TEST_F(ModuleBlacklistCacheUtilTest, WrittenFileSize) {
  third_party_dlls::PackedListMetadata metadata = {
      third_party_dlls::PackedListVersion::kCurrent, kTestModuleCount};
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 0u, 4000u);
  base::MD5Digest md5_digest;
  EXPECT_TRUE(WriteModuleBlacklistCache(module_blacklist_cache_path(), metadata,
                                        blacklisted_modules, &md5_digest));

  // Check the file's stat.
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(module_blacklist_cache_path(), &file_size));
  EXPECT_EQ(file_size, internal::CalculateExpectedFileSize(metadata));
}

TEST_F(ModuleBlacklistCacheUtilTest, WriteAndRead) {
  third_party_dlls::PackedListMetadata metadata = {
      third_party_dlls::PackedListVersion::kCurrent, kTestModuleCount};
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 0u, 4000u);
  base::MD5Digest md5_digest;
  EXPECT_TRUE(WriteModuleBlacklistCache(module_blacklist_cache_path(), metadata,
                                        blacklisted_modules, &md5_digest));

  third_party_dlls::PackedListMetadata read_metadata;
  std::vector<third_party_dlls::PackedListModule> read_blacklisted_modules;
  base::MD5Digest read_md5_digest;
  EXPECT_EQ(
      ReadResult::kSuccess,
      ReadModuleBlacklistCache(module_blacklist_cache_path(), &read_metadata,
                               &read_blacklisted_modules, &read_md5_digest));

  EXPECT_EQ(read_metadata.version, metadata.version);
  EXPECT_EQ(read_metadata.module_count, metadata.module_count);
  ASSERT_EQ(read_blacklisted_modules.size(), blacklisted_modules.size());
  // Note: Not using PackedListModuleEquals because the time_date_stamp is also
  // verified.
  EXPECT_EQ(0, memcmp(&read_blacklisted_modules[0], &blacklisted_modules[0],
                      read_blacklisted_modules.size() *
                          sizeof(third_party_dlls::PackedListModule)));

  for (size_t i = 0; i < std::extent<decltype(base::MD5Digest::a)>(); ++i)
    EXPECT_EQ(md5_digest.a[i], read_md5_digest.a[i]);
}

class FakeModuleListFilter : public ModuleListFilter {
 public:
  FakeModuleListFilter() = default;

  void AddWhitelistedModule(const third_party_dlls::PackedListModule& module) {
    whitelisted_modules_.emplace(
        base::StringPiece(
            reinterpret_cast<const char*>(&module.basename_hash[0]),
            base::size(module.basename_hash)),
        base::StringPiece(
            reinterpret_cast<const char*>(&module.code_id_hash[0]),
            base::size(module.basename_hash)));
  }

  // ModuleListFilter:
  bool IsWhitelisted(base::StringPiece module_basename_hash,
                     base::StringPiece module_code_id_hash) const override {
    return base::Contains(
        whitelisted_modules_,
        std::make_pair(module_basename_hash, module_code_id_hash));
  }

  std::unique_ptr<chrome::conflicts::BlacklistAction> IsBlacklisted(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data) const override {
    return nullptr;
  }

 private:
  ~FakeModuleListFilter() override = default;

  std::set<std::pair<base::StringPiece, base::StringPiece>>
      whitelisted_modules_;

  DISALLOW_COPY_AND_ASSIGN(FakeModuleListFilter);
};

TEST_F(ModuleBlacklistCacheUtilTest, RemoveWhitelistedEntries) {
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 5u, 10u);

  // The number of modules that will be whitelisted in this test.
  const size_t kWhitelistedModulesCount = 5u;

  // Mark a few modules as whitelisted.
  std::vector<third_party_dlls::PackedListModule> whitelisted_modules;
  SampleBlacklistedModules(
      kWhitelistedModulesCount,
      [&whitelisted_modules](const auto& element) {
        whitelisted_modules.push_back(element);
      },
      &blacklisted_modules);
  auto module_list_filter = base::MakeRefCounted<FakeModuleListFilter>();
  for (const auto& module : whitelisted_modules)
    module_list_filter->AddWhitelistedModule(module);

  internal::RemoveWhitelistedEntries(*module_list_filter, &blacklisted_modules);

  EXPECT_EQ(kTestModuleCount - kWhitelistedModulesCount,
            blacklisted_modules.size());
  for (const auto& module : whitelisted_modules) {
    auto iter =
        std::find_if(blacklisted_modules.begin(), blacklisted_modules.end(),
                     [&module](const auto& element) {
                       return internal::ModuleEqual()(module, element);
                     });
    EXPECT_EQ(blacklisted_modules.end(), iter);
  }
}

TEST_F(ModuleBlacklistCacheUtilTest, UpdateModuleBlacklistCacheTimestamps) {
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 5u, 10u);

  // The number of modules that will get their time_date_stamp updated.
  const size_t kModulesToRemove = 5u;

  // Pick 5 modules in the list that will have their time_date_stamp updated to
  // a new value guaranteed to be outside of the range of existing values
  // (which is [5u, 10u]).
  const uint32_t kNewTimeDateStamp = 15u;
  std::vector<third_party_dlls::PackedListModule> updated_modules;
  SampleBlacklistedModules(
      kModulesToRemove,
      [&updated_modules](const auto& element) {
        updated_modules.push_back(element);
      },
      &blacklisted_modules);
  for (auto& module : updated_modules)
    module.time_date_stamp = kNewTimeDateStamp;

  internal::UpdateModuleBlacklistCacheTimestamps(updated_modules,
                                                 &blacklisted_modules);

  EXPECT_EQ(kTestModuleCount, blacklisted_modules.size());
  // For each entires, make sure they were updated.
  for (const auto& module : updated_modules) {
    auto iter =
        std::find_if(blacklisted_modules.begin(), blacklisted_modules.end(),
                     [&module](const auto& element) {
                       return internal::ModuleEqual()(module, element);
                     });
    ASSERT_NE(blacklisted_modules.end(), iter);
    EXPECT_EQ(kNewTimeDateStamp, iter->time_date_stamp);
  }
}

TEST_F(ModuleBlacklistCacheUtilTest, RemoveExpiredEntries_OnlyExpired) {
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 5u, 10u);

  // The number of modules to remove from the collection.
  const size_t kModulesToRemove = 5u;

  // Set the time date stamp of 5 modules to an expired value.
  const uint32_t kExpiredTimeDateStamp = 1u;
  std::vector<third_party_dlls::PackedListModule> expired_modules;
  SampleBlacklistedModules(
      kModulesToRemove,
      [&expired_modules](auto& element) {
        expired_modules.push_back(element);
        element.time_date_stamp = kExpiredTimeDateStamp;
      },
      &blacklisted_modules);

  std::sort(blacklisted_modules.begin(), blacklisted_modules.end(),
            internal::ModuleTimeDateStampGreater());

  // Set a minimum time date stamp that excludes all the entries in
  // |expired_modules|.
  const size_t kMinTimeDateStamp = kExpiredTimeDateStamp;
  // Set a maxium cache file that fits all the existing modules.
  const size_t kMaxModuleBlacklistCacheSize = kTestModuleCount;
  // Assume there is no newly blacklisted module.
  const size_t kNewlyBlacklistedModuleCount = 0u;

  internal::RemoveExpiredEntries(
      kMinTimeDateStamp, kMaxModuleBlacklistCacheSize,
      kNewlyBlacklistedModuleCount, &blacklisted_modules);

  // The 5 elements were removed.
  EXPECT_EQ(kTestModuleCount - kModulesToRemove, blacklisted_modules.size());
  for (const auto& module : expired_modules) {
    auto iter =
        std::find_if(blacklisted_modules.begin(), blacklisted_modules.end(),
                     [&module](const auto& element) {
                       return internal::ModuleEqual()(module, element);
                     });
    EXPECT_EQ(blacklisted_modules.end(), iter);
  }
}

TEST_F(ModuleBlacklistCacheUtilTest, RemoveExpiredEntries_NewlyBlacklisted) {
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 10u, 10000u);

  std::sort(blacklisted_modules.begin(), blacklisted_modules.end(),
            internal::ModuleTimeDateStampGreater());

  // The number of modules to remove from the collection.
  const size_t kModulesToRemove = 10u;

  // Set a minimum time date stamp that doesn't exclude any entry.
  const size_t kMinTimeDateStamp = 0u;
  // Set a maxium cache file that fits all the existing modules.
  const size_t kMaxModuleBlacklistCacheSize = kTestModuleCount;
  // Remove the kModulesToRemove oldest entries by pretending there is
  // kModulesToRemove newly blacklisted modules.
  const size_t kNewlyBlacklistedModuleCount = kModulesToRemove;

  // Get a copy of the 10 oldest modules to make sure they were removed. They
  // should at the end of the vector.
  std::vector<third_party_dlls::PackedListModule> excess_modules(
      blacklisted_modules.end() - kNewlyBlacklistedModuleCount,
      blacklisted_modules.end());

  internal::RemoveExpiredEntries(
      kMinTimeDateStamp, kMaxModuleBlacklistCacheSize,
      kNewlyBlacklistedModuleCount, &blacklisted_modules);

  // Enough elements were removed.
  EXPECT_EQ(kTestModuleCount - kNewlyBlacklistedModuleCount,
            blacklisted_modules.size());
  for (const auto& module : excess_modules) {
    auto iter =
        std::find_if(blacklisted_modules.begin(), blacklisted_modules.end(),
                     [&module](const auto& element) {
                       return internal::ModuleEqual()(module, element);
                     });
    EXPECT_EQ(blacklisted_modules.end(), iter);
  }
}

TEST_F(ModuleBlacklistCacheUtilTest, RemoveExpiredEntries_MaxSize) {
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 10u, 10000u);

  std::sort(blacklisted_modules.begin(), blacklisted_modules.end(),
            internal::ModuleTimeDateStampGreater());

  // The number of modules to remove from the collection.
  const size_t kModulesToRemove = 15u;

  // Set a minimum time date stamp that doesn't exclude any entry.
  const size_t kMinTimeDateStamp = 0u;
  // Pick a max size that ensures that kModulesToRemove entries are removed.
  const size_t kMaxModuleBlacklistCacheSize =
      kTestModuleCount - kModulesToRemove;
  // Assume there is no newly blacklisted module.
  const size_t kNewlyBlacklistedModuleCount = 0u;

  // Get a copy of the last kModulesToRemove entries to make sure they are the
  // one that are removed.
  std::vector<third_party_dlls::PackedListModule> excess_modules(
      blacklisted_modules.end() - kModulesToRemove, blacklisted_modules.end());

  // The collection contains too many entries at first.
  EXPECT_NE(kMaxModuleBlacklistCacheSize, blacklisted_modules.size());

  internal::RemoveExpiredEntries(
      kMinTimeDateStamp, kMaxModuleBlacklistCacheSize,
      kNewlyBlacklistedModuleCount, &blacklisted_modules);

  // Enough elements were removed.
  EXPECT_EQ(kMaxModuleBlacklistCacheSize, blacklisted_modules.size());
  for (const auto& module : excess_modules) {
    auto iter =
        std::find_if(blacklisted_modules.begin(), blacklisted_modules.end(),
                     [&module](const auto& element) {
                       return internal::ModuleEqual()(module, element);
                     });
    EXPECT_EQ(blacklisted_modules.end(), iter);
  }
}

TEST_F(ModuleBlacklistCacheUtilTest, RemoveDuplicateEntries) {
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 10u, 10000u);

  // The number of modules to duplicate in this test.
  const size_t kDuplicateCount = 15u;

  // Create kDuplicateCount duplicate modules and set their time_date_stamp to a
  // greater value so that the duplicate is kept instead of the original.
  std::vector<third_party_dlls::PackedListModule> duplicated_modules;
  SampleBlacklistedModules(
      kDuplicateCount,
      [&duplicated_modules](auto& element) {
        duplicated_modules.push_back(element);
      },
      &blacklisted_modules);
  for (auto& module : duplicated_modules)
    module.time_date_stamp += 10u;

  // Insert at the beginning because RemoveDuplicateEntries() keeps the first
  // duplicate in the list.
  blacklisted_modules.insert(blacklisted_modules.begin(),
                             duplicated_modules.begin(),
                             duplicated_modules.end());

  // Stable sort to keep the relative order between duplicates.
  std::stable_sort(blacklisted_modules.begin(), blacklisted_modules.end(),
                   internal::ModuleLess());

  EXPECT_EQ(kTestModuleCount + kDuplicateCount, blacklisted_modules.size());

  internal::RemoveDuplicateEntries(&blacklisted_modules);

  EXPECT_EQ(kTestModuleCount, blacklisted_modules.size());
  for (const auto& module : duplicated_modules) {
    auto iter =
        std::find_if(blacklisted_modules.begin(), blacklisted_modules.end(),
                     [&module](const auto& element) {
                       return internal::ModuleEqual()(module, element) &&
                              module.time_date_stamp == element.time_date_stamp;
                     });
    EXPECT_NE(blacklisted_modules.end(), iter);
  }
}
