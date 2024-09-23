// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/drive/resource_metadata_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "components/drive/drive.pb.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace drive {
namespace internal {

namespace {

// Helper to destroy objects which needs Destroy() to be called on destruction.
// Note: When using this helper, you should destruct objects before
// BrowserThread.
struct DestroyHelperForTests {
  template <typename T>
  void operator()(T* object) const {
    if (object) {
      object->Destroy();
      content::RunAllTasksUntilIdle();  // Finish destruction.
    }
  }
};

}  // namespace

class ResourceMetadataStorageTest : public testing::Test {
 public:
  ResourceMetadataStorageTest(const ResourceMetadataStorageTest&) = delete;
  ResourceMetadataStorageTest& operator=(const ResourceMetadataStorageTest&) =
      delete;

 protected:
  ResourceMetadataStorageTest() = default;
  ~ResourceMetadataStorageTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    storage_.reset(new ResourceMetadataStorage(
        temp_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get()));
    ASSERT_TRUE(storage_->Initialize());
  }

  // Overwrites |storage_|'s version.
  void SetDBVersion(int version) {
    ResourceMetadataHeader header;
    ASSERT_EQ(FILE_ERROR_OK, storage_->GetHeader(&header));
    header.set_version(version);
    EXPECT_EQ(FILE_ERROR_OK, storage_->PutHeader(header));
  }

  bool CheckValidity() {
    return storage_->CheckValidity();
  }

  leveldb::DB* resource_map() { return storage_->resource_map_.get(); }

  // Puts a child entry.
  void PutChild(const std::string& parent_id,
                const std::string& child_base_name,
                const std::string& child_id) {
    storage_->resource_map_->Put(
        leveldb::WriteOptions(),
        ResourceMetadataStorage::GetChildEntryKey(parent_id, child_base_name),
        child_id);
  }

  // Removes a child entry.
  void RemoveChild(const std::string& parent_id,
                   const std::string& child_base_name) {
    storage_->resource_map_->Delete(
        leveldb::WriteOptions(),
        ResourceMetadataStorage::GetChildEntryKey(parent_id, child_base_name));
  }

  bool UpgradeOldDB() {
    return ResourceMetadataStorage::UpgradeOldDB(temp_dir_.GetPath());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ResourceMetadataStorage, DestroyHelperForTests> storage_;
};

TEST_F(ResourceMetadataStorageTest, LargestChangestamp) {
  const int64_t kLargestChangestamp = 1234567890;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));
  int64_t value = 0;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetLargestChangestamp(&value));
  EXPECT_EQ(kLargestChangestamp, value);
}

TEST_F(ResourceMetadataStorageTest, StartPageToken) {
  constexpr char kStartPageToken[] = "123456";
  EXPECT_EQ(FILE_ERROR_OK, storage_->SetStartPageToken(kStartPageToken));
  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetStartPageToken(&start_page_token));
  EXPECT_EQ(kStartPageToken, start_page_token);
}

TEST_F(ResourceMetadataStorageTest, PutEntry) {
  const std::string key1 = "abcdefg";
  const std::string key2 = "abcd";
  const std::string key3 = "efgh";
  const std::string name2 = "ABCD";
  const std::string name3 = "EFGH";

  // key1 not found.
  ResourceEntry result;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetEntry(key1, &result));

  // Put entry1.
  ResourceEntry entry1;
  entry1.set_local_id(key1);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry1));

  // key1 found.
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(key1, &result));

  // key2 not found.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetEntry(key2, &result));

  // Put entry2 as a child of entry1.
  ResourceEntry entry2;
  entry2.set_local_id(key2);
  entry2.set_parent_local_id(key1);
  entry2.set_base_name(name2);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry2));

  // key2 found.
  std::string child_id;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(key2, &result));
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetChild(key1, name2, &child_id));
  EXPECT_EQ(key2, child_id);

  // Put entry3 as a child of entry2.
  ResourceEntry entry3;
  entry3.set_local_id(key3);
  entry3.set_parent_local_id(key2);
  entry3.set_base_name(name3);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry3));

  // key3 found.
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(key3, &result));
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetChild(key2, name3, &child_id));
  EXPECT_EQ(key3, child_id);

  // Change entry3's parent to entry1.
  entry3.set_parent_local_id(key1);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry3));

  // entry3 is a child of entry1 now.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetChild(key2, name3, &child_id));
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetChild(key1, name3, &child_id));
  EXPECT_EQ(key3, child_id);

  // Remove entries.
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(key3));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetEntry(key3, &result));
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(key2));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetEntry(key2, &result));
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(key1));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetEntry(key1, &result));
}

TEST_F(ResourceMetadataStorageTest, Iterator) {
  // Prepare data.
  std::vector<std::string> keys;

  keys.push_back("entry1");
  keys.push_back("entry2");
  keys.push_back("entry3");
  keys.push_back("entry4");

  for (const std::string& key : keys) {
    ResourceEntry entry;
    entry.set_local_id(key);
    EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));
  }

  // Iterate and check the result.
  std::map<std::string, ResourceEntry> found_entries;
  std::unique_ptr<ResourceMetadataStorage::Iterator> it =
      storage_->GetIterator();
  ASSERT_TRUE(it);
  for (; !it->IsAtEnd(); it->Advance()) {
    const ResourceEntry& entry = it->GetValue();
    found_entries[it->GetID()] = entry;
  }
  EXPECT_FALSE(it->HasError());

  EXPECT_EQ(keys.size(), found_entries.size());
  for (const std::string& key : keys)
    EXPECT_TRUE(base::Contains(found_entries, key));
}

TEST_F(ResourceMetadataStorageTest, GetIdByResourceId) {
  const std::string local_id = "local_id";
  const std::string resource_id = "resource_id";

  // Resource ID to local ID mapping is not stored yet.
  std::string id;
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            storage_->GetIdByResourceId(resource_id, &id));

  // Put an entry with the resource ID.
  ResourceEntry entry;
  entry.set_local_id(local_id);
  entry.set_resource_id(resource_id);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));

  // Can get local ID by resource ID.
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetIdByResourceId(resource_id, &id));
  EXPECT_EQ(local_id, id);

  // Resource ID to local ID mapping is removed.
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(local_id));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            storage_->GetIdByResourceId(resource_id, &id));
}

TEST_F(ResourceMetadataStorageTest, GetChildren) {
  const std::string parents_id[] = { "mercury", "venus", "mars", "jupiter",
                                     "saturn" };
  std::vector<base::StringPairs> children_name_id(std::size(parents_id));
  // Skip children_name_id[0/1] here because Mercury and Venus have no moon.
  children_name_id[2].push_back(std::make_pair("phobos", "mars_i"));
  children_name_id[2].push_back(std::make_pair("deimos", "mars_ii"));
  children_name_id[3].push_back(std::make_pair("io", "jupiter_i"));
  children_name_id[3].push_back(std::make_pair("europa", "jupiter_ii"));
  children_name_id[3].push_back(std::make_pair("ganymede", "jupiter_iii"));
  children_name_id[3].push_back(std::make_pair("calisto", "jupiter_iv"));
  children_name_id[4].push_back(std::make_pair("mimas", "saturn_i"));
  children_name_id[4].push_back(std::make_pair("enceladus", "saturn_ii"));
  children_name_id[4].push_back(std::make_pair("tethys", "saturn_iii"));
  children_name_id[4].push_back(std::make_pair("dione", "saturn_iv"));
  children_name_id[4].push_back(std::make_pair("rhea", "saturn_v"));
  children_name_id[4].push_back(std::make_pair("titan", "saturn_vi"));
  children_name_id[4].push_back(std::make_pair("iapetus", "saturn_vii"));

  // Put parents.
  for (const std::string& id : parents_id) {
    ResourceEntry entry;
    entry.set_local_id(id);
    EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));
  }

  // Put children.
  for (size_t i = 0; i < children_name_id.size(); ++i) {
    for (const auto& id : children_name_id[i]) {
      ResourceEntry entry;
      entry.set_local_id(id.second);
      entry.set_parent_local_id(parents_id[i]);
      entry.set_base_name(id.first);
      EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));
    }
  }

  // Try to get children.
  for (size_t i = 0; i < children_name_id.size(); ++i) {
    std::vector<std::string> children;
    storage_->GetChildren(parents_id[i], &children);
    EXPECT_EQ(children_name_id[i].size(), children.size());
    for (const auto& id : children_name_id[i]) {
      EXPECT_EQ(1, base::ranges::count(children, id.second));
    }
  }
}

TEST_F(ResourceMetadataStorageTest, OpenExistingDB) {
  const std::string parent_id1 = "abcdefg";
  const std::string child_name1 = "WXYZABC";
  const std::string child_id1 = "qwerty";

  ResourceEntry entry1;
  entry1.set_local_id(parent_id1);
  ResourceEntry entry2;
  entry2.set_local_id(child_id1);
  entry2.set_parent_local_id(parent_id1);
  entry2.set_base_name(child_name1);

  // Put some data.
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry1));
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry2));

  // Close DB and reopen.
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Can read data.
  ResourceEntry result;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(parent_id1, &result));

  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(child_id1, &result));
  EXPECT_EQ(parent_id1, result.parent_local_id());
  EXPECT_EQ(child_name1, result.base_name());

  std::string child_id;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetChild(parent_id1, child_name1, &child_id));
  EXPECT_EQ(child_id1, child_id);
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_M29) {
  const int64_t kLargestChangestamp = 1234567890;
  const std::string title = "title";

  // Construct M29 version DB.
  SetDBVersion(6);
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));

  leveldb::WriteBatch batch;

  // Put a file entry and its cache entry.
  ResourceEntry entry;
  std::string serialized_entry;
  entry.set_title(title);
  entry.set_resource_id("file:abcd");
  EXPECT_TRUE(entry.SerializeToString(&serialized_entry));
  batch.Put("file:abcd", serialized_entry);

  FileCacheEntry cache_entry;
  EXPECT_TRUE(cache_entry.SerializeToString(&serialized_entry));
  batch.Put(std::string("file:abcd") + '\0' + "CACHE", serialized_entry);

  EXPECT_TRUE(resource_map()->Write(leveldb::WriteOptions(), &batch).ok());

  // Upgrade and reopen.
  storage_.reset();
  EXPECT_TRUE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Resource-ID-to-local-ID mapping is added.
  std::string id;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetIdByResourceId("abcd", &id));  // "file:" is dropped.

  // Data is erased, except cache entries.
  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(0, largest_changestamp);
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.title().empty());
  EXPECT_TRUE(entry.file_specific_info().has_cache_state());
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_M32) {
  const int64_t kLargestChangestamp = 1234567890;
  const std::string title = "title";
  const std::string resource_id = "abcd";
  const std::string local_id = "local-abcd";

  // Construct M32 version DB.
  SetDBVersion(11);
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));

  leveldb::WriteBatch batch;

  // Put a file entry and its cache and id entry.
  ResourceEntry entry;
  std::string serialized_entry;
  entry.set_title(title);
  entry.set_local_id(local_id);
  entry.set_resource_id(resource_id);
  EXPECT_TRUE(entry.SerializeToString(&serialized_entry));
  batch.Put(local_id, serialized_entry);

  FileCacheEntry cache_entry;
  EXPECT_TRUE(cache_entry.SerializeToString(&serialized_entry));
  batch.Put(local_id + '\0' + "CACHE", serialized_entry);

  batch.Put('\0' + std::string("ID") + '\0' + resource_id, local_id);

  EXPECT_TRUE(resource_map()->Write(leveldb::WriteOptions(), &batch).ok());

  // Upgrade and reopen.
  storage_.reset();
  EXPECT_TRUE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Data is erased, except cache and id mapping entries.
  std::string id;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetIdByResourceId(resource_id, &id));
  EXPECT_EQ(local_id, id);
  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(0, largest_changestamp);
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.title().empty());
  EXPECT_TRUE(entry.file_specific_info().has_cache_state());
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_M33) {
  const int64_t kLargestChangestamp = 1234567890;
  const std::string title = "title";
  const std::string resource_id = "abcd";
  const std::string local_id = "local-abcd";
  const std::string md5 = "md5";
  const std::string resource_id2 = "efgh";
  const std::string local_id2 = "local-efgh";
  const std::string md5_2 = "md5_2";

  // Construct M33 version DB.
  SetDBVersion(12);
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));

  leveldb::WriteBatch batch;

  // Put a file entry and its cache and id entry.
  ResourceEntry entry;
  std::string serialized_entry;
  entry.set_title(title);
  entry.set_local_id(local_id);
  entry.set_resource_id(resource_id);
  EXPECT_TRUE(entry.SerializeToString(&serialized_entry));
  batch.Put(local_id, serialized_entry);

  FileCacheEntry cache_entry;
  cache_entry.set_md5(md5);
  EXPECT_TRUE(cache_entry.SerializeToString(&serialized_entry));
  batch.Put(local_id + '\0' + "CACHE", serialized_entry);

  batch.Put('\0' + std::string("ID") + '\0' + resource_id, local_id);

  // Put another cache entry which is not accompanied by a ResourceEntry.
  cache_entry.set_md5(md5_2);
  EXPECT_TRUE(cache_entry.SerializeToString(&serialized_entry));
  batch.Put(local_id2 + '\0' + "CACHE", serialized_entry);
  batch.Put('\0' + std::string("ID") + '\0' + resource_id2, local_id2);

  EXPECT_TRUE(resource_map()->Write(leveldb::WriteOptions(), &batch).ok());

  // Upgrade and reopen.
  storage_.reset();
  EXPECT_TRUE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // largest_changestamp is cleared.
  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(0, largest_changestamp);

  // No other data is lost.
  std::string id;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetIdByResourceId(resource_id, &id));
  EXPECT_EQ(local_id, id);
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(id, &entry));
  EXPECT_EQ(title, entry.title());
  EXPECT_EQ(md5, entry.file_specific_info().cache_state().md5());

  EXPECT_EQ(FILE_ERROR_OK, storage_->GetIdByResourceId(resource_id2, &id));
  EXPECT_EQ(local_id2, id);
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry(id, &entry));
  EXPECT_EQ(md5_2, entry.file_specific_info().cache_state().md5());
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_Unknown) {
  const int64_t kLargestChangestamp = 1234567890;
  const std::string key1 = "abcd";

  // Put some data.
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));
  ResourceEntry entry;
  entry.set_local_id(key1);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));

  // Set newer version, upgrade and reopen DB.
  SetDBVersion(ResourceMetadataStorage::kDBVersion + 1);
  storage_.reset();
  EXPECT_FALSE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Data is erased because of the incompatible version.
  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(0, largest_changestamp);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, storage_->GetEntry(key1, &entry));
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_M37) {
  const int64_t kLargestChangestamp = 1234567890;

  // Construct v13 DB. It was used from M37 to M62.
  SetDBVersion(13);
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));

  leveldb::WriteBatch batch;

  // Put an ID entry with a corresponding ResourceEntry.
  ResourceEntry entry;
  entry.set_local_id("id1");
  entry.set_resource_id("resource_id1");

  std::string serialized_entry;
  EXPECT_TRUE(entry.SerializeToString(&serialized_entry));
  batch.Put("id1", serialized_entry);
  batch.Put('\0' + std::string("ID") + '\0' + "resource_id1", "id1");

  // Put an ID entry without any corresponding entries.
  batch.Put('\0' + std::string("ID") + '\0' + "resource_id2", "id3");

  EXPECT_TRUE(resource_map()->Write(leveldb::WriteOptions(), &batch).ok());

  // Upgrade and reopen.
  storage_.reset();
  EXPECT_TRUE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Only the unused entry is deleted.
  std::string id;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetIdByResourceId("resource_id1", &id));
  EXPECT_EQ("id1", id);
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            storage_->GetIdByResourceId("resource_id2", &id));

  // largest_changestamp is cleared.
  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(0, largest_changestamp);
}

TEST_F(ResourceMetadataStorageTest, WrongPath) {
  // Create a file.
  base::FilePath path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &path));

  storage_.reset(new ResourceMetadataStorage(
      path, base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  // Cannot initialize DB beacause the path does not point a directory.
  ASSERT_FALSE(storage_->Initialize());
}

TEST_F(ResourceMetadataStorageTest, RecoverCacheEntriesFromTrashedResourceMap) {
  // Put entry with id_foo.
  ResourceEntry entry;
  entry.set_local_id("id_foo");
  entry.set_base_name("foo");
  entry.set_title("foo");
  entry.mutable_file_specific_info()->mutable_cache_state()->set_md5("md5_foo");
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));

  // Put entry with id_bar as a id_foo's child.
  entry.set_local_id("id_bar");
  entry.set_parent_local_id("id_foo");
  entry.set_base_name("bar");
  entry.set_title("bar");
  entry.mutable_file_specific_info()->mutable_cache_state()->set_md5("md5_bar");
  entry.mutable_file_specific_info()->mutable_cache_state()->set_is_dirty(true);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));

  // Remove parent-child relationship to make the DB invalid.
  RemoveChild("id_foo", "bar");
  EXPECT_FALSE(CheckValidity());

  // Reopen. This should result in trashing the DB.
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Recover cache entries from the trashed DB.
  ResourceMetadataStorage::RecoveredCacheInfoMap recovered_cache_info;
  storage_->RecoverCacheInfoFromTrashedResourceMap(&recovered_cache_info);
  EXPECT_EQ(2U, recovered_cache_info.size());
  EXPECT_FALSE(recovered_cache_info["id_foo"].is_dirty);
  EXPECT_EQ("md5_foo", recovered_cache_info["id_foo"].md5);
  EXPECT_EQ("foo", recovered_cache_info["id_foo"].title);
  EXPECT_TRUE(recovered_cache_info["id_bar"].is_dirty);
  EXPECT_EQ("md5_bar", recovered_cache_info["id_bar"].md5);
  EXPECT_EQ("bar", recovered_cache_info["id_bar"].title);
}

TEST_F(ResourceMetadataStorageTest, CheckValidity) {
  const std::string key1 = "foo";
  const std::string name1 = "hoge";
  const std::string key2 = "bar";
  const std::string name2 = "fuga";
  const std::string key3 = "boo";
  const std::string name3 = "piyo";

  // Empty storage is valid.
  EXPECT_TRUE(CheckValidity());

  // Put entry with key1.
  ResourceEntry entry;
  entry.set_local_id(key1);
  entry.set_base_name(name1);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));
  EXPECT_TRUE(CheckValidity());

  // Put entry with key2 under key1.
  entry.set_local_id(key2);
  entry.set_parent_local_id(key1);
  entry.set_base_name(name2);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));
  EXPECT_TRUE(CheckValidity());

  RemoveChild(key1, name2);
  EXPECT_FALSE(CheckValidity());  // Missing parent-child relationship.

  // Add back parent-child relationship between key1 and key2.
  PutChild(key1, name2, key2);
  EXPECT_TRUE(CheckValidity());

  // Add parent-child relationship between key2 and key3.
  PutChild(key2, name3, key3);
  EXPECT_FALSE(CheckValidity());  // key3 is not stored in the storage.

  // Put entry with key3 under key2.
  entry.set_local_id(key3);
  entry.set_parent_local_id(key2);
  entry.set_base_name(name3);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));
  EXPECT_TRUE(CheckValidity());

  // Parent-child relationship with wrong name.
  RemoveChild(key2, name3);
  EXPECT_FALSE(CheckValidity());
  PutChild(key2, name2, key3);
  EXPECT_FALSE(CheckValidity());

  // Fix up the relationship between key2 and key3.
  RemoveChild(key2, name2);
  EXPECT_FALSE(CheckValidity());
  PutChild(key2, name3, key3);
  EXPECT_TRUE(CheckValidity());

  // Remove key2.
  RemoveChild(key1, name2);
  EXPECT_FALSE(CheckValidity());
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(key2));
  EXPECT_FALSE(CheckValidity());

  // Remove key3.
  RemoveChild(key2, name3);
  EXPECT_FALSE(CheckValidity());
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(key3));
  EXPECT_TRUE(CheckValidity());

  // Remove key1.
  EXPECT_EQ(FILE_ERROR_OK, storage_->RemoveEntry(key1));
  EXPECT_TRUE(CheckValidity());
}

TEST_F(ResourceMetadataStorageTest, UpgradeDBv15) {
  constexpr int64_t kLargestChangestamp = 54321;
  constexpr char kStartPageToken[] = "54322";
  constexpr int64_t kDirectoryChangestamp = 12345;
  constexpr char kDirectoryStartpageToken[] = "12346";

  // Construct a v15 DB
  SetDBVersion(15);
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));

  // Add a directory with a changestamp
  ResourceEntry entry;
  entry.set_local_id("local_id_1");
  entry.set_base_name("resource_id_1");
  entry.mutable_directory_specific_info()->set_changestamp(
      kDirectoryChangestamp);
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));

  // Upgrade and reopen
  storage_.reset();
  EXPECT_TRUE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(kLargestChangestamp, largest_changestamp);

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetStartPageToken(&start_page_token));
  EXPECT_EQ(kStartPageToken, start_page_token);

  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry("local_id_1", &entry));
  EXPECT_EQ(kDirectoryChangestamp,
            entry.directory_specific_info().changestamp());
  EXPECT_EQ(kDirectoryStartpageToken,
            entry.directory_specific_info().start_page_token());
}

// Test that upgrading from DB version 16 to 17 triggers a full metadata refresh
// (since this changes alternate_url to be set for directories, which need to
// be re-fetched).
TEST_F(ResourceMetadataStorageTest, UpgradeDBv16) {
  constexpr int64_t kLargestChangestamp = 54321;
  constexpr char kStartPageToken[] = "54322";

  // Construct a v16 DB.
  SetDBVersion(16);
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->SetLargestChangestamp(kLargestChangestamp));
  EXPECT_EQ(FILE_ERROR_OK, storage_->SetStartPageToken(kStartPageToken));

  // Add a file.
  ResourceEntry entry;
  entry.set_local_id("local_id_1");
  entry.set_base_name("resource_id_1");
  EXPECT_EQ(FILE_ERROR_OK, storage_->PutEntry(entry));

  // Upgrade and reopen.
  storage_.reset();
  EXPECT_TRUE(UpgradeOldDB());
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.GetPath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Changestamps are reset.
  int64_t largest_changestamp = 0;
  EXPECT_EQ(FILE_ERROR_OK,
            storage_->GetLargestChangestamp(&largest_changestamp));
  EXPECT_EQ(0, largest_changestamp);

  std::string start_page_token;
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetStartPageToken(&start_page_token));
  EXPECT_EQ("", start_page_token);

  // The data is retained.
  EXPECT_EQ(FILE_ERROR_OK, storage_->GetEntry("local_id_1", &entry));
  EXPECT_EQ("resource_id_1", entry.base_name());
}

}  // namespace internal
}  // namespace drive
