// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/proto_table_manager.h"

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/test_proto.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_proto {

namespace {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

constexpr char kTableName[] = "my_table";
}  // namespace

TEST(ProtoTableTest, PutReinitializeAndGet) {
  // In order to test ProtoTableManager is correctly
  // initializing the underlying database's tables:
  // - create a database and a ProtoTableManager on it;
  // - store some data; and
  // - construct a new ProtoTableManager to read from the
  // existing database state.

  base::test::TaskEnvironment env;
  sql::Database db;
  CHECK(db.OpenInMemory());

  auto manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/1);

  KeyValueTable<TestProto> table(kTableName);

  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    data.UpdateData("a", first_entry);
    data.UpdateData("b", second_entry);
    env.RunUntilIdle();
  }

  manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    data.InitializeOnDBSequence();

    TestProto result;
    ASSERT_TRUE(data.TryGetData("a", &result));
    EXPECT_THAT(result, EqualsProto(first_entry));

    ASSERT_TRUE(data.TryGetData("b", &result));
    EXPECT_THAT(result, EqualsProto(second_entry));
  }
}

TEST(ProtoTableTest, ReinitializingWithDifferentVersionClearsTables) {
  // In order to test ProtoTableManager is correctly
  // initializing the underlying database's tables:
  // - create a database and a ProtoTableManager on it;
  // - store some data; and
  // - construct a new ProtoTableManager to read from the
  // existing database state.

  base::test::TaskEnvironment env;
  sql::Database db;
  CHECK(db.OpenInMemory());

  constexpr int kInitialVersion = 1;

  auto manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/kInitialVersion);

  KeyValueTable<TestProto> table(kTableName);

  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    data.UpdateData("a", first_entry);
    data.UpdateData("b", second_entry);
    env.RunUntilIdle();
  }

  manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/kInitialVersion + 1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    data.InitializeOnDBSequence();

    TestProto result;
    EXPECT_FALSE(data.TryGetData("a", &result));
    EXPECT_FALSE(data.TryGetData("b", &result));
  }
}

TEST(ProtoTableTest, InitializingWithoutWrittenVersionClearsTables) {
  // Check that, when reinitializing the database when the most recent write
  // occurred before the database started keeping track of versions,
  // ProtoTableManager correctly clears the database.

  base::test::TaskEnvironment env;
  sql::Database db;
  CHECK(db.OpenInMemory());

  constexpr int kInitialVersion = 1;

  auto manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/kInitialVersion);

  KeyValueTable<TestProto> table(kTableName);

  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    data.UpdateData("a", first_entry);
    data.UpdateData("b", second_entry);
    env.RunUntilIdle();

    ASSERT_TRUE(sql::MetaTable::DeleteTableForTesting(&db));
    env.RunUntilIdle();
  }

  manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/kInitialVersion);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    data.InitializeOnDBSequence();

    TestProto result;
    EXPECT_FALSE(data.TryGetData("a", &result));
    EXPECT_FALSE(data.TryGetData("b", &result));
  }
}

TEST(ProtoTableTest, LoadingUnexpectedlyLargeVersionClearsTables) {
  // Check that, when reinitializing the database and the most recent write
  // occurred against a greater version of the database, ProtoTableManager
  // correctly clears the database.

  base::test::TaskEnvironment env;
  sql::Database db;
  CHECK(db.OpenInMemory());

  constexpr int kInitialVersion = 1;

  auto manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/kInitialVersion);

  KeyValueTable<TestProto> table(kTableName);

  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    data.UpdateData("a", first_entry);
    data.UpdateData("b", second_entry);
    env.RunUntilIdle();

    // Overwrite the stored version. It's safe to use an instance of
    // sql::MetaTable here because all sql::MetaTable instances use the same
    // database name ("meta") to manipulate versions.
    //
    // MetaTable::Init only writes a version if there was no version previously
    // written, so it doesn't matter what values the final two arguments have.
    // The SetVersionNumber is what actually overwrites the version.
    sql::MetaTable meta_helper;
    ASSERT_TRUE(meta_helper.Init(&db, 1, 1));
    ASSERT_TRUE(meta_helper.SetVersionNumber(kInitialVersion + 1));
    ASSERT_TRUE(meta_helper.SetCompatibleVersionNumber(kInitialVersion + 1));
    env.RunUntilIdle();
  }

  manager = base::MakeRefCounted<ProtoTableManager>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName},
                                  /*schema_version=*/kInitialVersion);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/std::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    data.InitializeOnDBSequence();

    TestProto result;
    EXPECT_FALSE(data.TryGetData("a", &result));
    EXPECT_FALSE(data.TryGetData("b", &result));
  }
}

}  // namespace sqlite_proto
