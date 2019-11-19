// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "components/leveldb_proto/public/proto_database.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_database_impl.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using base::ScopedTempDir;
using base::test::TaskEnvironment;
using leveldb_env::Options;
using testing::_;
using testing::Invoke;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Return;
using testing::UnorderedElementsAre;

namespace leveldb_proto {

namespace {

typedef std::map<std::string, TestProto> EntryMap;

const char kTestLevelDBClientName[] = "Test";

class MockDB : public LevelDB {
 public:
  MOCK_METHOD3(Init,
               leveldb::Status(const base::FilePath& database_dir,
                               const leveldb_env::Options& options,
                               bool));
  MOCK_METHOD3(Save,
               bool(const KeyValueVector&, const KeyVector&, leveldb::Status*));
  MOCK_METHOD3(UpdateWithRemoveFilter,
               bool(const base::StringPairs&,
                    const KeyFilter&,
                    leveldb::Status*));
  MOCK_METHOD4(UpdateWithRemoveFilter,
               bool(const base::StringPairs&,
                    const KeyFilter&,
                    const std::string&,
                    leveldb::Status*));
  MOCK_METHOD1(Load, bool(std::vector<std::string>*));
  MOCK_METHOD2(LoadWithFilter,
               bool(const KeyFilter&, std::vector<std::string>*));
  MOCK_METHOD4(LoadWithFilter,
               bool(const KeyFilter&,
                    std::vector<std::string>*,
                    const leveldb::ReadOptions&,
                    const std::string&));
  MOCK_METHOD1(LoadKeysAndEntries, bool(std::map<std::string, std::string>*));
  MOCK_METHOD2(LoadKeysAndEntriesWithFilter,
               bool(const KeyFilter&, std::map<std::string, std::string>*));
  MOCK_METHOD4(LoadKeysAndEntriesWithFilter,
               bool(const KeyFilter&,
                    std::map<std::string, std::string>*,
                    const leveldb::ReadOptions&,
                    const std::string&));
  MOCK_METHOD5(LoadKeysAndEntriesWhile,
               bool(const KeyFilter&,
                    std::map<std::string, std::string>*,
                    const leveldb::ReadOptions&,
                    const std::string&,
                    const KeyFilter&));

  MOCK_METHOD4(Get,
               bool(const std::string&, bool*, std::string*, leveldb::Status*));
  MOCK_METHOD0(Destroy, leveldb::Status());

  MockDB() : LevelDB(kTestLevelDBClientName) {}
};

class MockDatabaseCaller {
 public:
  MOCK_METHOD1(InitCallback, void(bool));
  MOCK_METHOD1(InitStatusCallback, void(Enums::InitStatus));
  MOCK_METHOD1(DestroyCallback, void(bool));
  MOCK_METHOD1(SaveCallback, void(bool));
  void LoadCallback(bool success,
                    std::unique_ptr<std::vector<TestProto>> entries) {
    LoadCallback1(success, entries.get());
  }
  MOCK_METHOD2(LoadCallback1, void(bool, std::vector<TestProto>*));
  void LoadKeysAndEntriesCallback(
      bool success,
      std::unique_ptr<std::map<std::string, TestProto>> keys_entries) {
    LoadKeysAndEntriesCallback1(success, keys_entries.get());
  }
  MOCK_METHOD2(LoadKeysAndEntriesCallback1,
               void(bool, std::map<std::string, TestProto>*));
  void GetCallback(bool success, std::unique_ptr<TestProto> entry) {
    GetCallback1(success, entry.get());
  }
  MOCK_METHOD2(GetCallback1, void(bool, TestProto*));
};

class OptionsEqMatcher : public MatcherInterface<const Options&> {
 public:
  explicit OptionsEqMatcher(const Options& expected) : expected_(expected) {}

  bool MatchAndExplain(const Options& actual,
                       MatchResultListener* listener) const override {
    return actual.comparator == expected_.comparator &&
           actual.create_if_missing == expected_.create_if_missing &&
           actual.error_if_exists == expected_.error_if_exists &&
           actual.paranoid_checks == expected_.paranoid_checks &&
           actual.env == expected_.env &&
           actual.info_log == expected_.info_log &&
           actual.write_buffer_size == expected_.write_buffer_size &&
           actual.max_open_files == expected_.max_open_files &&
           actual.block_cache == expected_.block_cache &&
           actual.block_size == expected_.block_size &&
           actual.block_restart_interval == expected_.block_restart_interval &&
           actual.max_file_size == expected_.max_file_size &&
           actual.compression == expected_.compression &&
           actual.reuse_logs == expected_.reuse_logs &&
           actual.filter_policy == expected_.filter_policy;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "which matches the expected position";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "which does not match the expected position";
  }

 private:
  Options expected_;

  DISALLOW_COPY_AND_ASSIGN(OptionsEqMatcher);
};

Matcher<const Options&> OptionsEq(const Options& expected) {
  return MakeMatcher(new OptionsEqMatcher(expected));
}

bool ZeroFilter(const std::string& key) {
  return key == "0";
}

EntryMap GetSmallModel() {
  EntryMap model;

  model["0"].set_id("0");
  model["0"].set_data("http://foo.com/1");

  model["1"].set_id("1");
  model["1"].set_data("http://bar.com/all");

  model["2"].set_id("2");
  model["2"].set_data("http://baz.com/1");

  return model;
}

EntryMap GetRangeTestModel() {
  EntryMap model;

  model["a"].set_id("a");
  model["a"].set_data("entry_a");

  model["b"].set_id("b");
  model["b"].set_data("entry_b");

  model["c"].set_id("c");
  model["c"].set_data("entry_c");

  model["d"].set_id("d");
  model["d"].set_data("entry_d");

  model["e"].set_id("e");
  model["e"].set_data("entry_e");

  return model;
}

}  // namespace

void ExpectEntryPointersEquals(EntryMap expected,
                               const std::vector<TestProto>& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < actual.size(); i++) {
    auto expected_it = expected.find(actual[i].id());
    EXPECT_TRUE(expected_it != expected.end());
    std::string serialized_expected = expected_it->second.SerializeAsString();
    std::string serialized_actual = actual[i].SerializeAsString();
    EXPECT_EQ(serialized_expected, serialized_actual);
    expected.erase(expected_it);
  }
}

class UniqueProtoDatabaseTest : public testing::Test {
 public:
  UniqueProtoDatabaseTest()
      : options_(MakeMatcher(new OptionsEqMatcher(CreateSimpleOptions()))) {}
  void SetUp() override {
    db_ = std::make_unique<ProtoDatabaseImpl<TestProto>>(
        ProtoDbType::TEST_DATABASE0, base::FilePath(),
        base::ThreadTaskRunnerHandle::Get());
  }

  void TearDown() override {
    db_.reset();
    base::RunLoop().RunUntilIdle();
  }

  const Matcher<const Options&> options_;
  TaskEnvironment task_environment_;
  std::unique_ptr<ProtoDatabaseImpl<TestProto>> db_;
};

// Test that UniqueProtoDatabase calls Init on the underlying database and that
// the caller's InitCallback is called with the correct value.
TEST_F(UniqueProtoDatabaseTest, TestDBInitSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  EXPECT_CALL(*mock_db, Init(path, options_, _))
      .WillOnce(Return(leveldb::Status()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitStatusCallback(Enums::InitStatus::kOK));

  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBInitFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  Options options;
  options.create_if_missing = true;
  EXPECT_CALL(*mock_db, Init(path, OptionsEq(options), _))
      .WillOnce(
          Return(leveldb::Status::IOError(leveldb::Slice(), leveldb::Slice())));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitStatusCallback(Enums::InitStatus::kError));

  db_->InitWithDatabase(mock_db.get(), path, options,
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBDestroySuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  EXPECT_CALL(*mock_db, Init(path, options_, _))
      .WillOnce(Return(leveldb::Status()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitStatusCallback(Enums::InitStatus::kOK));

  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  EXPECT_CALL(caller, DestroyCallback(true));
  db_->Destroy(base::BindOnce(&MockDatabaseCaller::DestroyCallback,
                              base::Unretained(&caller)));
  EXPECT_CALL(*mock_db, Destroy()).WillOnce(Return(leveldb::Status()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBDestroyFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  EXPECT_CALL(*mock_db, Init(path, options_, _))
      .WillOnce(Return(leveldb::Status()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitStatusCallback(Enums::InitStatus::kOK));

  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  EXPECT_CALL(caller, DestroyCallback(false));
  db_->Destroy(base::BindOnce(&MockDatabaseCaller::DestroyCallback,
                              base::Unretained(&caller)));
  EXPECT_CALL(*mock_db, Destroy())
      .WillOnce(
          Return(leveldb::Status::IOError(leveldb::Slice(), leveldb::Slice())));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(AppendLoadEntries, model) {
  std::vector<std::string>* output = arg1;
  for (const auto& pair : model)
    output->push_back(pair.second.SerializeAsString());

  return true;
}

ACTION_P(AppendLoadKeysAndEntries, model) {
  std::map<std::string, std::string>* output = arg1;
  for (const auto& pair : model)
    output->insert(std::make_pair(pair.first, pair.second.SerializeAsString()));

  return true;
}

ACTION_P(VerifyLoadEntries, expected) {
  std::vector<TestProto>* actual = arg1;
  ExpectEntryPointersEquals(expected, *actual);
}

ACTION_P(VerifyLoadKeysAndEntries, expected) {
  std::map<std::string, TestProto>* actual_map = arg1;
  std::vector<TestProto> actual;
  for (const auto& pair : *actual_map)
    actual.push_back(pair.second);
  ExpectEntryPointersEquals(expected, actual);
}

// Test that UniqueProtoDatabase calls Load on the underlying database and that
// the caller's LoadCallback is called with the correct success value. Also
// confirms that on success, the expected entries are passed to the caller's
// LoadCallback.
TEST_F(UniqueProtoDatabaseTest, TestDBLoadSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, LoadKeysAndEntriesWhile(_, _, _, _, _))
      .WillOnce(AppendLoadKeysAndEntries(model));
  EXPECT_CALL(caller, LoadKeysAndEntriesCallback1(true, _))
      .WillOnce(VerifyLoadKeysAndEntries(testing::ByRef(model)));
  db_->LoadKeysAndEntries(
      base::BindOnce(&MockDatabaseCaller::LoadKeysAndEntriesCallback,
                     base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBLoadFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, LoadWithFilter(_, _, _, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, LoadCallback1(false, _));
  db_->LoadEntries(base::BindOnce(&MockDatabaseCaller::LoadCallback,
                                  base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBLoadInRangeSuccess) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  MockDatabaseCaller caller;
  EntryMap model = GetRangeTestModel();
  EntryMap expected;
  expected["b"] = model["b"];
  expected["c"] = model["c"];
  expected["d"] = model["d"];

  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(db.get(), temp_dir.GetPath(), CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  auto save_entries =
      std::make_unique<ProtoDatabase<TestProto>::KeyEntryVector>();
  std::map<std::string, TestProto> load_keys_entries;
  std::unique_ptr<KeyVector> remove_keys(new KeyVector());

  for (const auto& pair : model)
    save_entries->push_back(std::make_pair(pair.second.id(), pair.second));

  leveldb::Status status;
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(std::move(save_entries), std::move(remove_keys),
                     base::BindOnce(&MockDatabaseCaller::SaveCallback,
                                    base::Unretained(&caller)));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(caller, LoadKeysAndEntriesCallback1(true, _))
      .WillOnce(VerifyLoadKeysAndEntries(testing::ByRef(expected)));
  db_->LoadKeysAndEntriesInRange(
      "b", "d",
      base::BindOnce(&MockDatabaseCaller::LoadKeysAndEntriesCallback,
                     base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBLoadInRangeSuccessSameKey) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  MockDatabaseCaller caller;
  EntryMap model = GetRangeTestModel();
  EntryMap expected;

  expected["d"] = model["d"];

  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(db.get(), temp_dir.GetPath(), CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  auto save_entries =
      std::make_unique<ProtoDatabase<TestProto>::KeyEntryVector>();
  std::map<std::string, TestProto> load_keys_entries;
  std::unique_ptr<KeyVector> remove_keys(new KeyVector());

  for (const auto& pair : model) {
    save_entries->push_back(std::make_pair(pair.second.id(), pair.second));
  }

  leveldb::Status status;
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(std::move(save_entries), std::move(remove_keys),
                     base::BindOnce(&MockDatabaseCaller::SaveCallback,
                                    base::Unretained(&caller)));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(caller, LoadKeysAndEntriesCallback1(true, _))
      .WillOnce(VerifyLoadKeysAndEntries(testing::ByRef(expected)));
  db_->LoadKeysAndEntriesInRange(
      "d", "d",
      base::BindOnce(&MockDatabaseCaller::LoadKeysAndEntriesCallback,
                     base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(SetGetEntry, model) {
  const std::string& key = arg0;
  bool* found = arg1;
  std::string* output = arg2;
  auto it = model.find(key);
  if (it == model.end()) {
    *found = false;
  } else {
    *found = true;
    *output = it->second.SerializeAsString();
  }
  return true;
}

ACTION_P(VerifyGetEntry, expected) {
  TestProto* actual = arg1;
  EXPECT_EQ(expected.SerializeAsString(), actual->SerializeAsString());
}

TEST_F(UniqueProtoDatabaseTest, TestDBGetSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  std::string key("1");
  ASSERT_TRUE(model.count(key));
  EXPECT_CALL(*mock_db, Get(key, _, _, _)).WillOnce(SetGetEntry(model));
  EXPECT_CALL(caller, GetCallback1(true, _))
      .WillOnce(VerifyGetEntry(model[key]));
  db_->GetEntry(key, base::BindOnce(&MockDatabaseCaller::GetCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBRemoveKeys) {
  const std::string kTestPrefix = "test_prefix";
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(*mock_db, UpdateWithRemoveFilter(_, _, kTestPrefix, _))
      .WillOnce(Return(true));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_update_entries;
  auto expect_update_success = base::BindOnce(
      [](base::OnceClosure signal, bool success) {
        EXPECT_TRUE(success);
        std::move(signal).Run();
      },
      run_update_entries.QuitClosure());
  ProtoDatabaseImpl<TestProto>* wrapper =
      reinterpret_cast<ProtoDatabaseImpl<TestProto>*>(db_.get());
  wrapper->RemoveKeysForTesting(
      base::BindRepeating([](const std::string& str) { return true; }),
      kTestPrefix, std::move(expect_update_success));
  run_update_entries.Run();
}

class UniqueProtoDatabaseLevelDBTest : public testing::Test {
 public:
  void TearDown() override { base::RunLoop().RunUntilIdle(); }

 private:
  TaskEnvironment task_environment_;
};

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBSaveAndLoadKeys) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());
  std::unique_ptr<ProtoDatabase<TestProto>> db =
      ProtoDatabaseProvider::GetUniqueDB<TestProto>(ProtoDbType::TEST_DATABASE0,
                                                    temp_dir.GetPath(),
                                                    db_thread.task_runner());

  base::RunLoop init_loop;
  db->Init(base::BindOnce(
      [](base::OnceClosure closure, leveldb_proto::Enums::InitStatus status) {
        std::move(closure).Run();
      },
      init_loop.QuitClosure()));
  init_loop.Run();

  base::RunLoop run_update_entries;
  auto expect_update_success = base::BindOnce(
      [](base::OnceClosure signal, bool success) {
        EXPECT_TRUE(success);
        std::move(signal).Run();
      },
      run_update_entries.QuitClosure());
  TestProto test_proto;
  test_proto.set_data("some data");
  ProtoDatabase<TestProto>::KeyEntryVector data_set(
      {{"0", test_proto}, {"1", test_proto}, {"2", test_proto}});
  db->UpdateEntries(
      std::make_unique<ProtoDatabase<TestProto>::KeyEntryVector>(data_set),
      std::make_unique<std::vector<std::string>>(),
      std::move(expect_update_success));
  run_update_entries.Run();

  base::RunLoop run_load_keys;
  auto verify_loaded_keys = base::BindOnce(
      [](base::OnceClosure signal, bool success,
         std::unique_ptr<std::vector<std::string>> keys) {
        EXPECT_TRUE(success);
        EXPECT_THAT(*keys, UnorderedElementsAre("0", "1", "2"));
        std::move(signal).Run();
      },
      run_load_keys.QuitClosure());
  db->LoadKeys(std::move(verify_loaded_keys));
  run_load_keys.Run();

  // Shutdown database.
  db.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBGetNotFound) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  std::string key("does_not_exist");
  ASSERT_FALSE(model.count(key));
  EXPECT_CALL(*mock_db, Get(key, _, _, _)).WillOnce(SetGetEntry(model));
  EXPECT_CALL(caller, GetCallback1(true, nullptr));
  db_->GetEntry(key, base::BindOnce(&MockDatabaseCaller::GetCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBGetFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  std::string key("does_not_exist");
  ASSERT_FALSE(model.count(key));
  EXPECT_CALL(*mock_db, Get(key, _, _, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, GetCallback1(false, nullptr));
  db_->GetEntry(key, base::BindOnce(&MockDatabaseCaller::GetCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(VerifyUpdateEntries, expected) {
  const KeyValueVector actual = arg0;
  // Create a vector of TestProto from |actual| to reuse the comparison
  // function.
  std::vector<TestProto> extracted_entries;
  for (const auto& pair : actual) {
    TestProto entry;
    if (!entry.ParseFromString(pair.second)) {
      ADD_FAILURE() << "Unable to deserialize the protobuf";
      return false;
    }

    extracted_entries.push_back(entry);
  }

  ExpectEntryPointersEquals(expected, extracted_entries);
  return true;
}

// Test that UniqueProtoDatabase calls Save on the underlying database with the
// correct entries to save and that the caller's SaveCallback is called with the
// correct success value.
TEST_F(UniqueProtoDatabaseTest, TestDBSaveSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  for (const auto& pair : model)
    entries->push_back(std::make_pair(pair.second.id(), pair.second));

  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Save(_, _, _)).WillOnce(VerifyUpdateEntries(model));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove),
                     base::BindOnce(&MockDatabaseCaller::SaveCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBSaveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove),
                     base::BindOnce(&MockDatabaseCaller::SaveCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

// Test that UniqueProtoDatabase calls Save on the underlying database with the
// correct entries to delete and that the caller's SaveCallback is called with
// the correct success value.
TEST_F(UniqueProtoDatabaseTest, TestDBRemoveSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());
  for (const auto& pair : model)
    keys_to_remove->push_back(pair.second.id());

  KeyVector keys_copy(*keys_to_remove.get());
  EXPECT_CALL(*mock_db, Save(_, keys_copy, _)).WillOnce(Return(true));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove),
                     base::BindOnce(&MockDatabaseCaller::SaveCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(UniqueProtoDatabaseTest, TestDBRemoveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  auto mock_db = std::make_unique<MockDB>();
  MockDatabaseCaller caller;
  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Init(_, options_, _));
  EXPECT_CALL(caller, InitStatusCallback(_));
  db_->InitWithDatabase(mock_db.get(), path, CreateSimpleOptions(),
                        base::BindOnce(&MockDatabaseCaller::InitStatusCallback,
                                       base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove),
                     base::BindOnce(&MockDatabaseCaller::SaveCallback,
                                    base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

// This tests that normal usage of the real database does not cause any
// threading violations.
TEST(UniqueProtoDatabaseThreadingTest, TestDBDestruction) {
  TaskEnvironment task_environment;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());

  std::unique_ptr<ProtoDatabase<TestProto>> db =
      ProtoDatabaseProvider::GetUniqueDB<TestProto>(ProtoDbType::TEST_DATABASE0,
                                                    temp_dir.GetPath(),
                                                    db_thread.task_runner());

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(_));
  base::RunLoop init_loop;
  db->Init(base::BindOnce(
      [](MockDatabaseCaller* caller, base::OnceClosure closure,
         leveldb_proto::Enums::InitStatus status) {
        bool success = status == Enums::kOK;
        caller->InitCallback(success);
        std::move(closure).Run();
      },
      &caller, init_loop.QuitClosure()));
  init_loop.Run();

  db.reset();
  base::RunLoop run_loop;
  db_thread.task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                            run_loop.QuitClosure());
  run_loop.Run();
}

// This tests that normal usage of the real database does not cause any
// threading violations.
TEST(UniqueProtoDatabaseThreadingTest, TestDBDestroy) {
  TaskEnvironment task_environment;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());

  std::unique_ptr<ProtoDatabase<TestProto>> db =
      ProtoDatabaseProvider::GetUniqueDB<TestProto>(ProtoDbType::TEST_DATABASE0,
                                                    temp_dir.GetPath(),
                                                    db_thread.task_runner());

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(_));
  base::RunLoop init_loop;
  db->Init(base::BindOnce(
      [](MockDatabaseCaller* caller, base::OnceClosure closure,
         leveldb_proto::Enums::InitStatus status) {
        bool success = status == Enums::kOK;
        caller->InitCallback(success);
        std::move(closure).Run();
      },
      &caller, init_loop.QuitClosure()));
  init_loop.Run();

  EXPECT_CALL(caller, DestroyCallback(_));
  base::RunLoop destroy_loop;
  db->Destroy(base::BindOnce(
      [](MockDatabaseCaller* caller, base::OnceClosure closure, bool success) {
        caller->DestroyCallback(success);
        std::move(closure).Run();
      },
      &caller, destroy_loop.QuitClosure()));
  destroy_loop.Run();

  // Verify the db is actually destroyed.
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath()));
}

// Test that the LevelDB properly saves entries and that load returns the saved
// entries. If |close_after_save| is true, the database will be closed after
// saving and then re-opened to ensure that the data is properly persisted.
void TestLevelDBSaveAndLoad(bool close_after_save) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetSmallModel();

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  for (const auto& pair : model) {
    save_entries.push_back(
        std::make_pair(pair.second.id(), pair.second.SerializeAsString()));
  }

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  EXPECT_TRUE(db->Init(temp_dir.GetPath(), CreateSimpleOptions()));
  leveldb::Status status;
  EXPECT_TRUE(db->Save(save_entries, remove_keys, &status));

  if (close_after_save) {
    db.reset(new LevelDB(kTestLevelDBClientName));
    EXPECT_TRUE(db->Init(temp_dir.GetPath(), CreateSimpleOptions()));
  }

  EXPECT_TRUE(db->Load(&load_entries));

  // Convert the strings back to TestProto.
  std::vector<TestProto> loaded_protos;
  for (const auto& serialized_entry : load_entries) {
    TestProto entry;
    ASSERT_TRUE(entry.ParseFromString(serialized_entry));
    loaded_protos.push_back(entry);
  }

  ExpectEntryPointersEquals(model, loaded_protos);
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBSaveAndLoad) {
  TestLevelDBSaveAndLoad(false);
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBCloseAndReopen) {
  TestLevelDBSaveAndLoad(true);
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBLoadWithFilter) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetSmallModel();

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  for (const auto& pair : model) {
    save_entries.push_back(
        std::make_pair(pair.second.id(), pair.second.SerializeAsString()));
  }

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  EXPECT_TRUE(db->Init(temp_dir.GetPath(), CreateSimpleOptions()));
  leveldb::Status status;
  EXPECT_TRUE(db->Save(save_entries, remove_keys, &status));

  EXPECT_TRUE(
      db->LoadWithFilter(base::BindRepeating(&ZeroFilter), &load_entries));

  EXPECT_EQ(load_entries.size(), 1u);
  TestProto entry;
  ASSERT_TRUE(entry.ParseFromString(load_entries[0]));
  EXPECT_EQ(entry.SerializeAsString(), model["0"].SerializeAsString());
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBLoadKeysAndEntries) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetSmallModel();

  KeyValueVector save_entries;
  std::map<std::string, std::string> load_keys_entries;
  KeyVector remove_keys;

  for (const auto& pair : model) {
    save_entries.push_back(
        std::make_pair(pair.second.id(), pair.second.SerializeAsString()));
  }

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  EXPECT_TRUE(db->Init(temp_dir.GetPath(), CreateSimpleOptions()));
  leveldb::Status status;
  EXPECT_TRUE(db->Save(save_entries, remove_keys, &status));

  EXPECT_TRUE(db->LoadKeysAndEntries(&load_keys_entries));

  EXPECT_EQ(load_keys_entries.size(), model.size());
  for (const auto& pair : load_keys_entries) {
    TestProto entry;
    ASSERT_TRUE(entry.ParseFromString(pair.second));
    EXPECT_EQ(entry.SerializeAsString(), model[pair.first].SerializeAsString());
  }
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBLoadKeysAndEntriesWhile) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetRangeTestModel();
  EntryMap expected;
  expected["b"] = model["b"];
  expected["c"] = model["c"];
  expected["d"] = model["d"];

  KeyValueVector save_entries;
  std::map<std::string, std::string> load_keys_entries;
  KeyVector remove_keys;

  for (const auto& pair : model) {
    save_entries.push_back(
        std::make_pair(pair.second.id(), pair.second.SerializeAsString()));
  }

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  EXPECT_TRUE(db->Init(temp_dir.GetPath(), CreateSimpleOptions()));
  leveldb::Status status;
  EXPECT_TRUE(db->Save(save_entries, remove_keys, &status));

  EXPECT_TRUE(db->LoadKeysAndEntriesWhile(
      KeyFilter(), &load_keys_entries, leveldb::ReadOptions(), "b",
      base::BindRepeating(
          [](const std::string& range_end, const std::string& key) {
            return key.compare(range_end) <= 0;
          },
          "d")));

  EXPECT_EQ(load_keys_entries.size(), expected.size());
  for (const auto& pair : load_keys_entries) {
    TestProto entry;
    ASSERT_TRUE(entry.ParseFromString(pair.second));
    EXPECT_EQ(entry.SerializeAsString(),
              expected[pair.first].SerializeAsString());
  }
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBInitFail) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  Options options;
  options.create_if_missing = false;
  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  EXPECT_FALSE(db->Init(temp_dir.GetPath(), options));
  EXPECT_FALSE(db->Load(&load_entries));
  leveldb::Status status;
  EXPECT_FALSE(db->Save(save_entries, remove_keys, &status));
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestMemoryDatabase) {
  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));

  std::vector<std::string> load_entries;

  ASSERT_TRUE(db->Init(base::FilePath(), CreateSimpleOptions()));

  ASSERT_TRUE(db->Load(&load_entries));
  EXPECT_EQ(0u, load_entries.size());

  KeyValueVector save_entries(1, std::make_pair("foo", "bar"));
  KeyVector remove_keys;

  leveldb::Status status;
  ASSERT_TRUE(db->Save(save_entries, remove_keys, &status));

  std::vector<std::string> second_load_entries;

  ASSERT_TRUE(db->Load(&second_load_entries));
  EXPECT_EQ(1u, second_load_entries.size());
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestCorruptDBReset) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a database, write some data, and then close the db.
  {
    LevelDB db(kTestLevelDBClientName);
    ASSERT_TRUE(db.Init(temp_dir.GetPath(), CreateSimpleOptions()));

    base::StringPairs pairs_to_save;
    pairs_to_save.push_back(std::make_pair("TheKey", "KeyValue"));
    std::vector<std::string> keys_to_remove;
    leveldb::Status status;
    ASSERT_TRUE(db.Save(pairs_to_save, keys_to_remove, &status));
  }

  EXPECT_TRUE(leveldb_chrome::CorruptClosedDBForTesting(temp_dir.GetPath()));

  // Open the corrupt database which should succeed, but will destroy the
  // existing corrupt database.
  LevelDB db(kTestLevelDBClientName);
  leveldb_env::Options options = CreateSimpleOptions();
  options.paranoid_checks = true;
  ASSERT_TRUE(db.Init(temp_dir.GetPath(), options));
  bool found = false;
  std::string value;
  leveldb::Status status;
  ASSERT_TRUE(db.Get("TheKey", &found, &value, &status));
  ASSERT_EQ("", value);
  ASSERT_FALSE(found);
}

TEST_F(UniqueProtoDatabaseLevelDBTest, TestDBDeleteWithFilter) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetSmallModel();

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  for (const auto& pair : model) {
    save_entries.push_back(
        std::make_pair(pair.second.id(), pair.second.SerializeAsString()));
  }

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  EXPECT_TRUE(db->Init(temp_dir.GetPath(), CreateSimpleOptions()));
  leveldb::Status status;
  EXPECT_TRUE(db->UpdateWithRemoveFilter(save_entries, KeyFilter(), &status));

  // Make sure the "0" entry is in database.
  EXPECT_TRUE(
      db->LoadWithFilter(base::BindRepeating(&ZeroFilter), &load_entries));
  EXPECT_EQ(load_entries.size(), 1u);

  // Delete "0" entry.
  save_entries.clear();
  EXPECT_TRUE(db->UpdateWithRemoveFilter(
      save_entries, base::BindRepeating(&ZeroFilter), &status));

  // Make sure the "0" entry is not there.
  load_entries.clear();
  EXPECT_TRUE(
      db->LoadWithFilter(base::BindRepeating(&ZeroFilter), &load_entries));
  EXPECT_EQ(load_entries.size(), 0u);
}

}  // namespace leveldb_proto
