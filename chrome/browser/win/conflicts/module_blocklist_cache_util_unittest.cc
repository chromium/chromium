// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blocklist_cache_util.h"

#include <memory>
#include <random>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/md5.h"
#include "base/ranges/algorithm.h"
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
  CHECK(base::ranges::adjacent_find(entries, internal::ModuleEqual()) ==
        entries.end());

  return entries;
}

// Calls the |p| binary predicate for a sample of the collection.
template <typename T, typename Predicate>
void SampleBlocklistedModules(size_t count,
                              Predicate p,
                              std::vector<T>* collection) {
  size_t to_skip = collection->size() / count;
  for (size_t i = 0; i < count; ++i)
    p((*collection)[i * to_skip]);
}

}  // namespace

class ModuleBlocklistCacheUtilTest : public testing::Test {
 public:
  ModuleBlocklistCacheUtilTest(const ModuleBlocklistCacheUtilTest&) = delete;
  ModuleBlocklistCacheUtilTest& operator=(const ModuleBlocklistCacheUtilTest&) =
      delete;

 protected:
  ModuleBlocklistCacheUtilTest() = default;
  ~ModuleBlocklistCacheUtilTest() override = default;

  // The number of module entry created for each test.
  enum { kTestModuleCount = 500u };

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    module_blocklist_cache_path_ = scoped_temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ModuleBlocklistCache"));
  }

  const base::FilePath& module_blocklist_cache_path() const {
    return module_blocklist_cache_path_;
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;

  base::FilePath module_blocklist_cache_path_;
};

TEST_F(ModuleBlocklistCacheUtilTest, CalculateTimeDateStamp) {
  static constexpr base::Time::Exploded kChromeBirthday = {
      .year = 2008, .month = 9, .day_of_week = 2, .day_of_month = 2};

  base::Time time;
  ASSERT_TRUE(kChromeBirthday.HasValidValues());
  ASSERT_TRUE(base::Time::FromUTCExploded(kChromeBirthday, &time));

  // Ensure that CalculateTimeDateStamp() will always return the number of
  // hours between |time| and the Windows epoch.
  EXPECT_EQ(3573552u, CalculateTimeDateStamp(time));
}

TEST_F(ModuleBlocklistCacheUtilTest, WriteEmptyCache) {
  third_party_dlls::PackedListMetadata metadata = {
      third_party_dlls::PackedListVersion::kCurrent, 0};
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_TRUE(WriteModuleBlocklistCache(module_blocklist_cache_path(), metadata,
                                        blocklisted_modules, &md5_digest));

  // Check the file's stat.
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(module_blocklist_cache_path(), &file_size));
  EXPECT_EQ(file_size, internal::CalculateExpectedFileSize(metadata));

  base::MD5Digest expected = {
      0x33, 0xCD, 0xEC, 0xCC, 0xCE, 0xBE, 0x80, 0x32,
      0x9F, 0x1F, 0xDB, 0xEE, 0x7F, 0x58, 0x74, 0xCB,
  };

  for (size_t i = 0; i < sizeof(base::MD5Digest::a); ++i) {
    EXPECT_EQ(expected.a[i], md5_digest.a[i]);
  }
}

TEST_F(ModuleBlocklistCacheUtilTest, WrittenFileSize) {
  third_party_dlls::PackedListMetadata metadata = {
      third_party_dlls::PackedListVersion::kCurrent, kTestModuleCount};
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 0u, 4000u);
  base::MD5Digest md5_digest;
  EXPECT_TRUE(WriteModuleBlocklistCache(module_blocklist_cache_path(), metadata,
                                        blocklisted_modules, &md5_digest));

  // Check the file's stat.
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(module_blocklist_cache_path(), &file_size));
  EXPECT_EQ(file_size, internal::CalculateExpectedFileSize(metadata));
}

TEST_F(ModuleBlocklistCacheUtilTest, WriteAndRead) {
  third_party_dlls::PackedListMetadata metadata = {
      third_party_dlls::PackedListVersion::kCurrent, kTestModuleCount};
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 0u, 4000u);
  base::MD5Digest md5_digest;
  EXPECT_TRUE(WriteModuleBlocklistCache(module_blocklist_cache_path(), metadata,
                                        blocklisted_modules, &md5_digest));

  third_party_dlls::PackedListMetadata read_metadata;
  std::vector<third_party_dlls::PackedListModule> read_blocklisted_modules;
  base::MD5Digest read_md5_digest;
  EXPECT_EQ(
      ReadResult::kSuccess,
      ReadModuleBlocklistCache(module_blocklist_cache_path(), &read_metadata,
                               &read_blocklisted_modules, &read_md5_digest));

  EXPECT_EQ(read_metadata.version, metadata.version);
  EXPECT_EQ(read_metadata.module_count, metadata.module_count);
  ASSERT_EQ(read_blocklisted_modules.size(), blocklisted_modules.size());
  // Note: Not using PackedListModuleEquals because the time_date_stamp is also
  // verified.
  EXPECT_EQ(0, memcmp(&read_blocklisted_modules[0], &blocklisted_modules[0],
                      read_blocklisted_modules.size() *
                          sizeof(third_party_dlls::PackedListModule)));

  for (size_t i = 0; i < sizeof(base::MD5Digest::a); ++i) {
    EXPECT_EQ(md5_digest.a[i], read_md5_digest.a[i]);
  }
}

class FakeModuleListFilter : public ModuleListFilter {
 public:
  FakeModuleListFilter() = default;

  FakeModuleListFilter(const FakeModuleListFilter&) = delete;
  FakeModuleListFilter& operator=(const FakeModuleListFilter&) = delete;

  void AddAllowlistedModule(const third_party_dlls::PackedListModule& module) {
    allowlisted_modules_.emplace(
        std::string_view(
            reinterpret_cast<const char*>(&module.basename_hash[0]),
            std::size(module.basename_hash)),
        std::string_view(reinterpret_cast<const char*>(&module.code_id_hash[0]),
                         std::size(module.basename_hash)));
  }

  // ModuleListFilter:
  bool IsAllowlisted(std::string_view module_basename_hash,
                     std::string_view module_code_id_hash) const override {
    return base::Contains(
        allowlisted_modules_,
        std::make_pair(module_basename_hash, module_code_id_hash));
  }

  std::unique_ptr<chrome::conflicts::BlocklistAction> IsBlocklisted(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data) const override {
    return nullptr;
  }

 private:
  ~FakeModuleListFilter() override = default;

  std::set<std::pair<std::string_view, std::string_view>> allowlisted_modules_;
};

TEST_F(ModuleBlocklistCacheUtilTest, RemoveAllowlistedEntries) {
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 5u, 10u);

  // The number of modules that will be allowlisted in this test.
  const size_t kAllowlistedModulesCount = 5u;

  // Mark a few modules as allowlisted.
  std::vector<third_party_dlls::PackedListModule> allowlisted_modules;
  SampleBlocklistedModules(
      kAllowlistedModulesCount,
      [&allowlisted_modules](const auto& element) {
        allowlisted_modules.push_back(element);
      },
      &blocklisted_modules);
  auto module_list_filter = base::MakeRefCounted<FakeModuleListFilter>();
  for (const auto& module : allowlisted_modules)
    module_list_filter->AddAllowlistedModule(module);

  internal::RemoveAllowlistedEntries(*module_list_filter, &blocklisted_modules);

  EXPECT_EQ(kTestModuleCount - kAllowlistedModulesCount,
            blocklisted_modules.size());
  for (const auto& module : allowlisted_modules) {
    EXPECT_TRUE(base::ranges::none_of(
        blocklisted_modules, [&module](const auto& element) {
          return internal::ModuleEqual()(module, element);
        }));
  }
}

TEST_F(ModuleBlocklistCacheUtilTest, UpdateModuleBlocklistCacheTimestamps) {
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 5u, 10u);

  // The number of modules that will get their time_date_stamp updated.
  const size_t kModulesToRemove = 5u;

  // Pick 5 modules in the list that will have their time_date_stamp updated to
  // a new value guaranteed to be outside of the range of existing values
  // (which is [5u, 10u]).
  const uint32_t kNewTimeDateStamp = 15u;
  std::vector<third_party_dlls::PackedListModule> updated_modules;
  SampleBlocklistedModules(
      kModulesToRemove,
      [&updated_modules](const auto& element) {
        updated_modules.push_back(element);
      },
      &blocklisted_modules);
  for (auto& module : updated_modules)
    module.time_date_stamp = kNewTimeDateStamp;

  internal::UpdateModuleBlocklistCacheTimestamps(updated_modules,
                                                 &blocklisted_modules);

  EXPECT_EQ(kTestModuleCount, blocklisted_modules.size());
  // For each entires, make sure they were updated.
  for (const auto& module : updated_modules) {
    auto iter = base::ranges::find_if(
        blocklisted_modules, [&module](const auto& element) {
          return internal::ModuleEqual()(module, element);
        });
    ASSERT_NE(blocklisted_modules.end(), iter);
    EXPECT_EQ(kNewTimeDateStamp, iter->time_date_stamp);
  }
}

TEST_F(ModuleBlocklistCacheUtilTest, RemoveExpiredEntries_OnlyExpired) {
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 5u, 10u);

  // The number of modules to remove from the collection.
  const size_t kModulesToRemove = 5u;

  // Set the time date stamp of 5 modules to an expired value.
  const uint32_t kExpiredTimeDateStamp = 1u;
  std::vector<third_party_dlls::PackedListModule> expired_modules;
  SampleBlocklistedModules(
      kModulesToRemove,
      [&expired_modules](auto& element) {
        expired_modules.push_back(element);
        element.time_date_stamp = kExpiredTimeDateStamp;
      },
      &blocklisted_modules);

  std::sort(blocklisted_modules.begin(), blocklisted_modules.end(),
            internal::ModuleTimeDateStampGreater());

  // Set a minimum time date stamp that excludes all the entries in
  // |expired_modules|.
  const size_t kMinTimeDateStamp = kExpiredTimeDateStamp;
  // Set a maxium cache file that fits all the existing modules.
  const size_t kMaxModuleBlocklistCacheSize = kTestModuleCount;
  // Assume there is no newly blocklisted module.
  const size_t kNewlyBlocklistedModuleCount = 0u;

  internal::RemoveExpiredEntries(
      kMinTimeDateStamp, kMaxModuleBlocklistCacheSize,
      kNewlyBlocklistedModuleCount, &blocklisted_modules);

  // The 5 elements were removed.
  EXPECT_EQ(kTestModuleCount - kModulesToRemove, blocklisted_modules.size());
  for (const auto& module : expired_modules) {
    EXPECT_TRUE(base::ranges::none_of(
        blocklisted_modules, [&module](const auto& element) {
          return internal::ModuleEqual()(module, element);
        }));
  }
}

TEST_F(ModuleBlocklistCacheUtilTest, RemoveExpiredEntries_NewlyBlocklisted) {
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 10u, 10000u);

  std::sort(blocklisted_modules.begin(), blocklisted_modules.end(),
            internal::ModuleTimeDateStampGreater());

  // The number of modules to remove from the collection.
  const size_t kModulesToRemove = 10u;

  // Set a minimum time date stamp that doesn't exclude any entry.
  const size_t kMinTimeDateStamp = 0u;
  // Set a maxium cache file that fits all the existing modules.
  const size_t kMaxModuleBlocklistCacheSize = kTestModuleCount;
  // Remove the kModulesToRemove oldest entries by pretending there is
  // kModulesToRemove newly blocklisted modules.
  const size_t kNewlyBlocklistedModuleCount = kModulesToRemove;

  // Get a copy of the 10 oldest modules to make sure they were removed. They
  // should at the end of the vector.
  std::vector<third_party_dlls::PackedListModule> excess_modules(
      blocklisted_modules.end() - kNewlyBlocklistedModuleCount,
      blocklisted_modules.end());

  internal::RemoveExpiredEntries(
      kMinTimeDateStamp, kMaxModuleBlocklistCacheSize,
      kNewlyBlocklistedModuleCount, &blocklisted_modules);

  // Enough elements were removed.
  EXPECT_EQ(kTestModuleCount - kNewlyBlocklistedModuleCount,
            blocklisted_modules.size());
  for (const auto& module : excess_modules) {
    EXPECT_TRUE(base::ranges::none_of(
        blocklisted_modules, [&module](const auto& element) {
          return internal::ModuleEqual()(module, element);
        }));
  }
}

TEST_F(ModuleBlocklistCacheUtilTest, RemoveExpiredEntries_MaxSize) {
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 10u, 10000u);

  std::sort(blocklisted_modules.begin(), blocklisted_modules.end(),
            internal::ModuleTimeDateStampGreater());

  // The number of modules to remove from the collection.
  const size_t kModulesToRemove = 15u;

  // Set a minimum time date stamp that doesn't exclude any entry.
  const size_t kMinTimeDateStamp = 0u;
  // Pick a max size that ensures that kModulesToRemove entries are removed.
  const size_t kMaxModuleBlocklistCacheSize =
      kTestModuleCount - kModulesToRemove;
  // Assume there is no newly blocklisted module.
  const size_t kNewlyBlocklistedModuleCount = 0u;

  // Get a copy of the last kModulesToRemove entries to make sure they are the
  // one that are removed.
  std::vector<third_party_dlls::PackedListModule> excess_modules(
      blocklisted_modules.end() - kModulesToRemove, blocklisted_modules.end());

  // The collection contains too many entries at first.
  EXPECT_NE(kMaxModuleBlocklistCacheSize, blocklisted_modules.size());

  internal::RemoveExpiredEntries(
      kMinTimeDateStamp, kMaxModuleBlocklistCacheSize,
      kNewlyBlocklistedModuleCount, &blocklisted_modules);

  // Enough elements were removed.
  EXPECT_EQ(kMaxModuleBlocklistCacheSize, blocklisted_modules.size());
  for (const auto& module : excess_modules) {
    EXPECT_TRUE(base::ranges::none_of(
        blocklisted_modules, [&module](const auto& element) {
          return internal::ModuleEqual()(module, element);
        }));
  }
}

TEST_F(ModuleBlocklistCacheUtilTest, RemoveDuplicateEntries) {
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules =
      CreateUniqueModuleEntries(kTestModuleCount, 10u, 10000u);

  // The number of modules to duplicate in this test.
  const size_t kDuplicateCount = 15u;

  // Create kDuplicateCount duplicate modules and set their time_date_stamp to a
  // greater value so that the duplicate is kept instead of the original.
  std::vector<third_party_dlls::PackedListModule> duplicated_modules;
  SampleBlocklistedModules(
      kDuplicateCount,
      [&duplicated_modules](auto& element) {
        duplicated_modules.push_back(element);
      },
      &blocklisted_modules);
  for (auto& module : duplicated_modules)
    module.time_date_stamp += 10u;

  // Insert at the beginning because RemoveDuplicateEntries() keeps the first
  // duplicate in the list.
  blocklisted_modules.insert(blocklisted_modules.begin(),
                             duplicated_modules.begin(),
                             duplicated_modules.end());

  // Stable sort to keep the relative order between duplicates.
  std::stable_sort(blocklisted_modules.begin(), blocklisted_modules.end(),
                   internal::ModuleLess());

  EXPECT_EQ(kTestModuleCount + kDuplicateCount, blocklisted_modules.size());

  internal::RemoveDuplicateEntries(&blocklisted_modules);

  EXPECT_EQ(kTestModuleCount, blocklisted_modules.size());
  for (const auto& module : duplicated_modules) {
    EXPECT_TRUE(base::ranges::any_of(
        blocklisted_modules, [&module](const auto& element) {
          return internal::ModuleEqual()(module, element) &&
                 module.time_date_stamp == element.time_date_stamp;
        }));
  }
}
