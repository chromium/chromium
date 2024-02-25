// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/key_value_data.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/sqlite_proto/table_manager.h"
#include "components/sqlite_proto/test_proto.pb.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_proto {

namespace {

template <typename T>
class FakeKeyValueTable : public KeyValueTable<T> {
 public:
  FakeKeyValueTable() : sqlite_proto::KeyValueTable<T>("") {}
  void GetAllData(std::map<std::string, T>* data_map,
                  sql::Database* db) const override {
    *data_map = data_;
  }
  void UpdateData(const std::string& key,
                  const T& data,
                  sql::Database* db) override {
    data_[key] = data;
  }
  void DeleteData(const std::vector<std::string>& keys,
                  sql::Database* db) override {
    for (const auto& key : keys)
      data_.erase(key);
  }
  void DeleteAllData(sql::Database* db) override { data_.clear(); }

  std::map<std::string, T> data_;
};

class FakeTableManager : public TableManager {
 public:
  FakeTableManager()
      : TableManager(base::SingleThreadTaskRunner::GetCurrentDefault()) {}
  void ScheduleDBTask(const base::Location& from_here,
                      base::OnceCallback<void(sql::Database*)> task) override {
    GetTaskRunner()->PostTask(
        from_here, base::BindOnce(&TableManager::ExecuteDBTaskOnDBSequence,
                                  this, std::move(task)));
  }
  void ExecuteDBTaskOnDBSequence(
      base::OnceCallback<void(sql::Database*)> task) override {
    ASSERT_TRUE(GetTaskRunner()->RunsTasksInCurrentSequence());
    std::move(task).Run(DB());
  }

 protected:
  ~FakeTableManager() override = default;

  // TableManager
  void CreateOrClearTablesIfNecessary() override {}
  void LogDatabaseStats() override {}
};

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

struct TestProtoCompare {
  bool operator()(const TestProto& lhs, const TestProto& rhs) {
    return lhs.value() < rhs.value();
  }
};

}  // namespace

class KeyValueDataTest : public ::testing::Test {
 public:
  KeyValueDataTest()
      : manager_(base::MakeRefCounted<FakeTableManager>()),
        data_(manager_, &table_, std::nullopt, base::TimeDelta()) {
    // In these tests, we're using the current thread as the DB sequence.
    data_.InitializeOnDBSequence();
  }

  ~KeyValueDataTest() override = default;

 protected:
  base::test::TaskEnvironment env_;
  FakeKeyValueTable<TestProto> table_;
  scoped_refptr<TableManager> manager_ =
      base::MakeRefCounted<FakeTableManager>();
  KeyValueData<TestProto, TestProtoCompare> data_;
};

TEST_F(KeyValueDataTest, GetWhenEmpty) {
  TestProto result;
  EXPECT_FALSE(data_.TryGetData("nonexistent_key", &result));
}

TEST_F(KeyValueDataTest, PutAndGet) {
  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  data_.UpdateData("a", first_entry);
  data_.UpdateData("b", second_entry);

  TestProto result;
  ASSERT_TRUE(data_.TryGetData("a", &result));
  EXPECT_THAT(result, EqualsProto(first_entry));

  ASSERT_TRUE(data_.TryGetData("b", &result));
  EXPECT_THAT(result, EqualsProto(second_entry));
}

// Test that deleting one entry:
// - makes that entry inaccessible, but
// - does not affect the remaining entry.
TEST_F(KeyValueDataTest, Delete) {
  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  data_.UpdateData("a", first_entry);
  data_.UpdateData("b", second_entry);

  TestProto result;
  data_.DeleteData(std::vector<std::string>{"b"});
  EXPECT_FALSE(data_.TryGetData("b", &result));

  ASSERT_TRUE(data_.TryGetData("a", &result));
  EXPECT_THAT(result, EqualsProto(first_entry));
}

TEST_F(KeyValueDataTest, DeleteAll) {
  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  data_.UpdateData("a", first_entry);
  data_.UpdateData("b", second_entry);

  data_.DeleteAllData();

  EXPECT_TRUE(data_.GetAllCached().empty());
}

TEST(KeyValueDataTestSize, CantAddToFullTable) {
  FakeKeyValueTable<TestProto> table;
  base::test::TaskEnvironment env;

  auto manager = base::MakeRefCounted<FakeTableManager>();
  KeyValueData<TestProto, TestProtoCompare> data(
      manager, &table, /*max_num_entries=*/2,
      /*flush_delay=*/base::TimeDelta());
  // In these tests, we're using the current thread as the DB sequence.
  data.InitializeOnDBSequence();

  TestProto one_entry, two_entry, three_entry;
  one_entry.set_value(1);
  two_entry.set_value(2);
  three_entry.set_value(3);

  data.UpdateData("a", one_entry);
  data.UpdateData("b", two_entry);
  data.UpdateData("c", three_entry);

  EXPECT_EQ(data.GetAllCached().size(), 2u);
}

// Test that building a KeyValueData on top of a backend table
// with more than |max_num_entries| many entries leads to the table
// being pruned down to a number of entries equal to the KeyValueData's
// capacity.
TEST(KeyValueDataTestSize, PrunesOverlargeTable) {
  FakeKeyValueTable<TestProto> table;
  base::test::TaskEnvironment env;

  auto manager = base::MakeRefCounted<FakeTableManager>();

  // Initialization: write a table of size 2 to |manager|'s backend.
  {
    KeyValueData<TestProto, TestProtoCompare> data(
        manager, &table, /*max_num_entries=*/std::nullopt,
        /*flush_delay=*/base::TimeDelta());
    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    TestProto one_entry, two_entry;
    one_entry.set_value(1);
    two_entry.set_value(2);

    data.UpdateData("a", one_entry);
    data.UpdateData("b", two_entry);

    // Write changes through to the "disk."
    env.RunUntilIdle();
  }

  {
    KeyValueData<TestProto, TestProtoCompare> data(
        manager, &table, /*max_num_entries=*/1,
        /*flush_delay=*/base::TimeDelta());
    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    // A cache with size limit less than the size of the database
    // should load items up to its capacity (evicting the rest).
    EXPECT_EQ(data.GetAllCached().size(), 1u);
  }

  {
    KeyValueData<TestProto, TestProtoCompare> data(
        manager, &table, /*max_num_entries=*/std::nullopt,
        /*flush_delay=*/base::TimeDelta());
    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    // The second, max_num_elements=1, cache should have pruned
    // the database to a single element upon initialization.
    EXPECT_EQ(data.GetAllCached().size(), 1u);
  }
}

}  // namespace sqlite_proto
