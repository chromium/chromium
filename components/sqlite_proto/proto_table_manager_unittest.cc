// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/proto_table_manager.h"

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/test_proto.pb.h"
#include "sql/database.h"
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
      base::ThreadTaskRunnerHandle::Get());
  manager->InitializeOnDbSequence(&db, std::vector<std::string>{kTableName});

  KeyValueTable<TestProto> table(kTableName);

  TestProto first_entry, second_entry;
  first_entry.set_value(1);
  second_entry.set_value(1);

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/absl::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    // In these tests, we're using the current thread as the DB sequence.
    data.InitializeOnDBSequence();

    data.UpdateData("a", first_entry);
    data.UpdateData("b", second_entry);
    env.RunUntilIdle();
  }

  {
    KeyValueData<TestProto> data(manager, &table,
                                 /*max_num_entries=*/absl::nullopt,
                                 /*flush_delay=*/base::TimeDelta());

    data.InitializeOnDBSequence();

    TestProto result;
    ASSERT_TRUE(data.TryGetData("a", &result));
    EXPECT_THAT(result, EqualsProto(first_entry));

    ASSERT_TRUE(data.TryGetData("b", &result));
    EXPECT_THAT(result, EqualsProto(second_entry));
  }
}

}  // namespace sqlite_proto
