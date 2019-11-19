// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/directory_backing_store.h"

#include <stddef.h>

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/sync/base/node_ordinal.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/on_disk_directory_backing_store.h"
#include "components/sync/test/test_directory_backing_store.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace syncable {
namespace {

const char kTestCacheGuid[] = "test_cache_guid";

base::RepeatingCallback<std::string()> TestCacheGuidGenerator() {
  return base::BindRepeating([]() -> std::string { return kTestCacheGuid; });
}

// A handler that simply sets |catastrophic_error_handler_was_called| to true.
void CatastrophicErrorHandler(bool* catastrophic_error_handler_was_called) {
  *catastrophic_error_handler_was_called = true;
}

// Create a dirty EntryKernel with an ID derived from |id| + |id_suffix|.
std::unique_ptr<EntryKernel> CreateEntry(int id, const std::string& id_suffix) {
  std::unique_ptr<EntryKernel> entry(new EntryKernel());
  std::string id_string = base::NumberToString(id) + id_suffix;
  entry->put(ID, Id::CreateFromClientString(id_string));
  entry->put(META_HANDLE, id);
  entry->mark_dirty(nullptr);
  return entry;
}

}  // namespace

class MigrationTest : public testing::TestWithParam<int> {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  std::string GetUsername() {
    return "nick@chromium.org";
  }

  base::FilePath GetDatabasePath() {
    return temp_dir_.GetPath().Append(Directory::kSyncDatabaseFilename);
  }

  static bool LoadAndIgnoreReturnedData(DirectoryBackingStore *dbs) {
    Directory::MetahandlesMap tmp_handles_map;
    MetahandleSet metahandles_to_purge;
    Directory::KernelLoadInfo kernel_load_info;
    DirOpenResult result =
        dbs->Load(&tmp_handles_map, &metahandles_to_purge, &kernel_load_info);
    return result == OPENED_NEW || result == OPENED_EXISTING;
  }

  void SetUpCorruptedRootDatabase(sql::Database* connection);
  void SetUpVersion67Database(sql::Database* connection);
  void SetUpVersion68Database(sql::Database* connection);
  void SetUpVersion69Database(sql::Database* connection);
  void SetUpVersion70Database(sql::Database* connection);
  void SetUpVersion71Database(sql::Database* connection);
  void SetUpVersion72Database(sql::Database* connection);
  void SetUpVersion73Database(sql::Database* connection);
  void SetUpVersion74Database(sql::Database* connection);
  void SetUpVersion75Database(sql::Database* connection);
  void SetUpVersion76Database(sql::Database* connection);
  void SetUpVersion77Database(sql::Database* connection);
  void SetUpVersion78Database(sql::Database* connection);
  void SetUpVersion79Database(sql::Database* connection);
  void SetUpVersion80Database(sql::Database* connection);
  void SetUpVersion81Database(sql::Database* connection);
  void SetUpVersion82Database(sql::Database* connection);
  void SetUpVersion83Database(sql::Database* connection);
  void SetUpVersion84Database(sql::Database* connection);
  void SetUpVersion85Database(sql::Database* connection);
  void SetUpVersion86Database(sql::Database* connection);
  void SetUpVersion87Database(sql::Database* connection);
  void SetUpVersion88Database(sql::Database* connection);
  void SetUpVersion89Database(sql::Database* connection);
  void SetUpVersion90Database(sql::Database* connection);
  void SetUpVersion91Database(sql::Database* connection);
  void SetUpVersion92Database(sql::Database* connection);

  void SetUpCurrentDatabaseAndCheckVersion(sql::Database* connection) {
    SetUpVersion92Database(connection);  // Prepopulates data.
    TestDirectoryBackingStore dbs(GetUsername(), connection);
    ASSERT_EQ(kCurrentDBVersion, dbs.GetVersion());

    ASSERT_TRUE(LoadAndIgnoreReturnedData(&dbs));
    ASSERT_FALSE(dbs.needs_column_refresh());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

class DirectoryBackingStoreTest : public MigrationTest {};

#if defined(OS_WIN)

// On Windows, we used to store timestamps in FILETIME format.
#define LEGACY_META_PROTO_TIMES_1 129079956640320000LL
#define LEGACY_META_PROTO_TIMES_2 128976886618480000LL
#define LEGACY_META_PROTO_TIMES_4 129002163642690000LL
#define LEGACY_META_PROTO_TIMES_5 129001555500000000LL
#define LEGACY_META_PROTO_TIMES_6 129053976170000000LL
#define LEGACY_META_PROTO_TIMES_7 128976864758480000LL
#define LEGACY_META_PROTO_TIMES_8 128976864758480000LL
#define LEGACY_META_PROTO_TIMES_9 128976864758480000LL
#define LEGACY_META_PROTO_TIMES_10 128976864758480000LL
#define LEGACY_META_PROTO_TIMES_11 129079956948440000LL
#define LEGACY_META_PROTO_TIMES_12 129079957513650000LL
#define LEGACY_META_PROTO_TIMES_13 129079957985300000LL
#define LEGACY_META_PROTO_TIMES_14 129079958383000000LL

#define LEGACY_META_PROTO_TIMES_STR_1 "129079956640320000"
#define LEGACY_META_PROTO_TIMES_STR_2 "128976886618480000"
#define LEGACY_META_PROTO_TIMES_STR_4 "129002163642690000"
#define LEGACY_META_PROTO_TIMES_STR_5 "129001555500000000"
#define LEGACY_META_PROTO_TIMES_STR_6 "129053976170000000"
#define LEGACY_META_PROTO_TIMES_STR_7 "128976864758480000"
#define LEGACY_META_PROTO_TIMES_STR_8 "128976864758480000"
#define LEGACY_META_PROTO_TIMES_STR_9 "128976864758480000"
#define LEGACY_META_PROTO_TIMES_STR_10 "128976864758480000"
#define LEGACY_META_PROTO_TIMES_STR_11 "129079956948440000"
#define LEGACY_META_PROTO_TIMES_STR_12 "129079957513650000"
#define LEGACY_META_PROTO_TIMES_STR_13 "129079957985300000"
#define LEGACY_META_PROTO_TIMES_STR_14 "129079958383000000"

// Generated via:
//
// ruby -ane '$F[1].sub!("LEGACY_", ""); $F[2] = Integer($F[2].sub!("LL", "")) /
//    10000 - 11644473600000; print "#{$F[0]} #{$F[1]} #{$F[2]}LL"'
//
// Magic numbers taken from
// http://stackoverflow.com/questions/5398557/
//    java-library-for-dealing-with-win32-filetime .

// Now we store them in Java format (ms since the Unix epoch).
#define META_PROTO_TIMES_1 1263522064032LL
#define META_PROTO_TIMES_2 1253215061848LL
#define META_PROTO_TIMES_4 1255742764269LL
#define META_PROTO_TIMES_5 1255681950000LL
#define META_PROTO_TIMES_6 1260924017000LL
#define META_PROTO_TIMES_7 1253212875848LL
#define META_PROTO_TIMES_8 1253212875848LL
#define META_PROTO_TIMES_9 1253212875848LL
#define META_PROTO_TIMES_10 1253212875848LL
#define META_PROTO_TIMES_11 1263522094844LL
#define META_PROTO_TIMES_12 1263522151365LL
#define META_PROTO_TIMES_13 1263522198530LL
#define META_PROTO_TIMES_14 1263522238300LL

#define META_PROTO_TIMES_STR_1 "1263522064032"
#define META_PROTO_TIMES_STR_2 "1253215061848"
#define META_PROTO_TIMES_STR_4 "1255742764269"
#define META_PROTO_TIMES_STR_5 "1255681950000"
#define META_PROTO_TIMES_STR_6 "1260924017000"
#define META_PROTO_TIMES_STR_7 "1253212875848"
#define META_PROTO_TIMES_STR_8 "1253212875848"
#define META_PROTO_TIMES_STR_9 "1253212875848"
#define META_PROTO_TIMES_STR_10 "1253212875848"
#define META_PROTO_TIMES_STR_11 "1263522094844"
#define META_PROTO_TIMES_STR_12 "1263522151365"
#define META_PROTO_TIMES_STR_13 "1263522198530"
#define META_PROTO_TIMES_STR_14 "1263522238300"

#else

// On other platforms, we used to store timestamps in time_t format (s
// since the Unix epoch).
#define LEGACY_META_PROTO_TIMES_1 1263522064LL
#define LEGACY_META_PROTO_TIMES_2 1253215061LL
#define LEGACY_META_PROTO_TIMES_4 1255742764LL
#define LEGACY_META_PROTO_TIMES_5 1255681950LL
#define LEGACY_META_PROTO_TIMES_6 1260924017LL
#define LEGACY_META_PROTO_TIMES_7 1253212875LL
#define LEGACY_META_PROTO_TIMES_8 1253212875LL
#define LEGACY_META_PROTO_TIMES_9 1253212875LL
#define LEGACY_META_PROTO_TIMES_10 1253212875LL
#define LEGACY_META_PROTO_TIMES_11 1263522094LL
#define LEGACY_META_PROTO_TIMES_12 1263522151LL
#define LEGACY_META_PROTO_TIMES_13 1263522198LL
#define LEGACY_META_PROTO_TIMES_14 1263522238LL

#define LEGACY_META_PROTO_TIMES_STR_1 "1263522064"
#define LEGACY_META_PROTO_TIMES_STR_2 "1253215061"
#define LEGACY_META_PROTO_TIMES_STR_4 "1255742764"
#define LEGACY_META_PROTO_TIMES_STR_5 "1255681950"
#define LEGACY_META_PROTO_TIMES_STR_6 "1260924017"
#define LEGACY_META_PROTO_TIMES_STR_7 "1253212875"
#define LEGACY_META_PROTO_TIMES_STR_8 "1253212875"
#define LEGACY_META_PROTO_TIMES_STR_9 "1253212875"
#define LEGACY_META_PROTO_TIMES_STR_10 "1253212875"
#define LEGACY_META_PROTO_TIMES_STR_11 "1263522094"
#define LEGACY_META_PROTO_TIMES_STR_12 "1263522151"
#define LEGACY_META_PROTO_TIMES_STR_13 "1263522198"
#define LEGACY_META_PROTO_TIMES_STR_14 "1263522238"

// Now we store them in Java format (ms since the Unix epoch).
#define META_PROTO_TIMES_1 1263522064000LL
#define META_PROTO_TIMES_2 1253215061000LL
#define META_PROTO_TIMES_4 1255742764000LL
#define META_PROTO_TIMES_5 1255681950000LL
#define META_PROTO_TIMES_6 1260924017000LL
#define META_PROTO_TIMES_7 1253212875000LL
#define META_PROTO_TIMES_8 1253212875000LL
#define META_PROTO_TIMES_9 1253212875000LL
#define META_PROTO_TIMES_10 1253212875000LL
#define META_PROTO_TIMES_11 1263522094000LL
#define META_PROTO_TIMES_12 1263522151000LL
#define META_PROTO_TIMES_13 1263522198000LL
#define META_PROTO_TIMES_14 1263522238000LL

#define META_PROTO_TIMES_STR_1 "1263522064000"
#define META_PROTO_TIMES_STR_2 "1253215061000"
#define META_PROTO_TIMES_STR_4 "1255742764000"
#define META_PROTO_TIMES_STR_5 "1255681950000"
#define META_PROTO_TIMES_STR_6 "1260924017000"
#define META_PROTO_TIMES_STR_7 "1253212875000"
#define META_PROTO_TIMES_STR_8 "1253212875000"
#define META_PROTO_TIMES_STR_9 "1253212875000"
#define META_PROTO_TIMES_STR_10 "1253212875000"
#define META_PROTO_TIMES_STR_11 "1263522094000"
#define META_PROTO_TIMES_STR_12 "1263522151000"
#define META_PROTO_TIMES_STR_13 "1263522198000"
#define META_PROTO_TIMES_STR_14 "1263522238000"

#endif

// Helper macros for the database dumps in the SetUpVersion*Database
// functions.
#define LEGACY_META_PROTO_TIMES(x) LEGACY_META_PROTO_TIMES_##x
#define LEGACY_META_PROTO_TIMES_STR(x) LEGACY_META_PROTO_TIMES_STR_##x
#define LEGACY_PROTO_TIME_VALS(x)    \
  LEGACY_META_PROTO_TIMES_STR(x) "," \
  LEGACY_META_PROTO_TIMES_STR(x) "," \
  LEGACY_META_PROTO_TIMES_STR(x) "," \
  LEGACY_META_PROTO_TIMES_STR(x)
#define META_PROTO_TIMES(x) META_PROTO_TIMES_##x
#define META_PROTO_TIMES_STR(x) META_PROTO_TIMES_STR_##x
#define META_PROTO_TIMES_VALS(x)    \
  META_PROTO_TIMES_STR(x) "," \
  META_PROTO_TIMES_STR(x) "," \
  META_PROTO_TIMES_STR(x) "," \
  META_PROTO_TIMES_STR(x)

namespace {

// Helper functions for testing.

enum ShouldIncludeDeletedItems {
  INCLUDE_DELETED_ITEMS,
  DONT_INCLUDE_DELETED_ITEMS
};

// Returns a map from metahandle -> expected legacy time (in proto
// format).
std::map<int64_t, int64_t> GetExpectedLegacyMetaProtoTimes(
    enum ShouldIncludeDeletedItems include_deleted) {
  std::map<int64_t, int64_t> expected_legacy_meta_proto_times;
  expected_legacy_meta_proto_times[1] = LEGACY_META_PROTO_TIMES(1);
  if (include_deleted == INCLUDE_DELETED_ITEMS) {
    expected_legacy_meta_proto_times[2] = LEGACY_META_PROTO_TIMES(2);
    expected_legacy_meta_proto_times[4] = LEGACY_META_PROTO_TIMES(4);
    expected_legacy_meta_proto_times[5] = LEGACY_META_PROTO_TIMES(5);
  }
  expected_legacy_meta_proto_times[6] = LEGACY_META_PROTO_TIMES(6);
  expected_legacy_meta_proto_times[7] = LEGACY_META_PROTO_TIMES(7);
  expected_legacy_meta_proto_times[8] = LEGACY_META_PROTO_TIMES(8);
  expected_legacy_meta_proto_times[9] = LEGACY_META_PROTO_TIMES(9);
  expected_legacy_meta_proto_times[10] = LEGACY_META_PROTO_TIMES(10);
  expected_legacy_meta_proto_times[11] = LEGACY_META_PROTO_TIMES(11);
  expected_legacy_meta_proto_times[12] = LEGACY_META_PROTO_TIMES(12);
  expected_legacy_meta_proto_times[13] = LEGACY_META_PROTO_TIMES(13);
  expected_legacy_meta_proto_times[14] = LEGACY_META_PROTO_TIMES(14);
  return expected_legacy_meta_proto_times;
}

// Returns a map from metahandle -> expected time (in proto format).
std::map<int64_t, int64_t> GetExpectedMetaProtoTimes(
    enum ShouldIncludeDeletedItems include_deleted) {
  std::map<int64_t, int64_t> expected_meta_proto_times;
  expected_meta_proto_times[1] = META_PROTO_TIMES(1);
  if (include_deleted == INCLUDE_DELETED_ITEMS) {
    expected_meta_proto_times[2] = META_PROTO_TIMES(2);
    expected_meta_proto_times[4] = META_PROTO_TIMES(4);
    expected_meta_proto_times[5] = META_PROTO_TIMES(5);
  }
  expected_meta_proto_times[6] = META_PROTO_TIMES(6);
  expected_meta_proto_times[7] = META_PROTO_TIMES(7);
  expected_meta_proto_times[8] = META_PROTO_TIMES(8);
  expected_meta_proto_times[9] = META_PROTO_TIMES(9);
  expected_meta_proto_times[10] = META_PROTO_TIMES(10);
  expected_meta_proto_times[11] = META_PROTO_TIMES(11);
  expected_meta_proto_times[12] = META_PROTO_TIMES(12);
  expected_meta_proto_times[13] = META_PROTO_TIMES(13);
  expected_meta_proto_times[14] = META_PROTO_TIMES(14);
  return expected_meta_proto_times;
}

// Returns a map from metahandle -> expected time (as a Time object).
std::map<int64_t, base::Time> GetExpectedMetaTimes() {
  std::map<int64_t, base::Time> expected_meta_times;
  const std::map<int64_t, int64_t>& expected_meta_proto_times =
      GetExpectedMetaProtoTimes(INCLUDE_DELETED_ITEMS);
  for (auto it = expected_meta_proto_times.begin();
       it != expected_meta_proto_times.end(); ++it) {
    expected_meta_times[it->first] = ProtoTimeToTime(it->second);
  }
  return expected_meta_times;
}

// Extracts a map from metahandle -> time (in proto format) from the
// given database.
std::map<int64_t, int64_t> GetMetaProtoTimes(sql::Database* db) {
  sql::Statement s(db->GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT metahandle, mtime, server_mtime, ctime, server_ctime "
          "FROM metas"));
  EXPECT_EQ(5, s.ColumnCount());
  std::map<int64_t, int64_t> meta_times;
  while (s.Step()) {
    int64_t metahandle = s.ColumnInt64(0);
    int64_t mtime = s.ColumnInt64(1);
    int64_t server_mtime = s.ColumnInt64(2);
    int64_t ctime = s.ColumnInt64(3);
    int64_t server_ctime = s.ColumnInt64(4);
    EXPECT_EQ(mtime, server_mtime);
    EXPECT_EQ(mtime, ctime);
    EXPECT_EQ(mtime, server_ctime);
    meta_times[metahandle] = mtime;
  }
  EXPECT_TRUE(s.Succeeded());
  return meta_times;
}

::testing::AssertionResult AssertTimesMatch(const char* t1_expr,
                                            const char* t2_expr,
                                            const base::Time& t1,
                                            const base::Time& t2) {
  if (t1 == t2)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
      << t1_expr << " and " << t2_expr
      << " (internal values: " << t1.ToInternalValue()
      << " and " << t2.ToInternalValue()
      << ") (proto time: " << TimeToProtoTime(t1)
      << " and " << TimeToProtoTime(t2)
      << ") do not match";
}

// Expect that all time fields of the given entry kernel will be the
// given time.
void ExpectTime(const EntryKernel& entry_kernel,
                const base::Time& expected_time) {
  EXPECT_PRED_FORMAT2(AssertTimesMatch,
                      expected_time, entry_kernel.ref(CTIME));
  EXPECT_PRED_FORMAT2(AssertTimesMatch,
                      expected_time, entry_kernel.ref(SERVER_CTIME));
  EXPECT_PRED_FORMAT2(AssertTimesMatch,
                      expected_time, entry_kernel.ref(MTIME));
  EXPECT_PRED_FORMAT2(AssertTimesMatch,
                      expected_time, entry_kernel.ref(SERVER_MTIME));
}

// Expect that all the entries in |entries| have times matching those in
// the given map (from metahandle to expect time).
void ExpectTimes(const Directory::MetahandlesMap& handles_map,
                 const std::map<int64_t, base::Time>& expected_times) {
  for (auto it = handles_map.begin(); it != handles_map.end(); ++it) {
    int64_t meta_handle = it->first;
    SCOPED_TRACE(meta_handle);
    auto it2 = expected_times.find(meta_handle);
    if (it2 == expected_times.end()) {
      ADD_FAILURE() << "Could not find expected time for " << meta_handle;
      continue;
    }
    ExpectTime(*it->second, it2->second);
  }
}

}  // namespace

void MigrationTest::SetUpCorruptedRootDatabase(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
      "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE metas (metahandle bigint primary key ON CONFLICT FAIL,"
      "base_version bigint default -1,server_version bigint default 0,"
      "mtime bigint default 0,server_mtime bigint default 0,"
      "ctime bigint default 0,server_ctime bigint default 0,"
      "server_position_in_parent bigint default 0,"
      "local_external_id bigint default 0,id varchar(255) default 'r',"
      "parent_id varchar(255) default 'r',"
      "server_parent_id varchar(255) default 'r',"
      "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
      "is_unsynced bit default 0,is_unapplied_update bit default 0,"
      "is_del bit default 0,is_dir bit default 0,"
      "is_bookmark_object bit default 0,server_is_dir bit default 0,"
      "server_is_del bit default 0,server_is_bookmark_object bit default 0,"
      "name varchar(255), "            /* COLLATE PATHNAME, */
      "unsanitized_name varchar(255)," /* COLLATE PATHNAME, */
      "non_unique_name varchar,"
      "server_name varchar(255)," /* COLLATE PATHNAME */
      "server_non_unique_name varchar,"
      "bookmark_url varchar,server_bookmark_url varchar,"
      "singleton_tag varchar,bookmark_favicon blob,"
      "server_bookmark_favicon blob);"
      "INSERT INTO metas VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(
          1) ",0,0,'r2','r2','r','r','r',0,0,0,1,0,0,0,0,NULL,"
             "NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);"
             "CREATE TABLE share_info (id VARCHAR(128) primary key, "
             "last_sync_timestamp INT, name VARCHAR(128), "
             "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
             "db_create_version VARCHAR(128), db_create_time int, "
             "next_id bigint default -2, cache_guid VARCHAR(32));"
             "INSERT INTO share_info VALUES('nick@chromium.org',694,"
             "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
             "'Unknown',1263522064,-65542,"
             "'9010788312004066376x-6609234393368420856x');"
             "CREATE TABLE share_version (id VARCHAR(128) primary key, data "
             "INT);"
             "INSERT INTO share_version VALUES('nick@chromium.org',68);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion67Database(sql::Database* connection) {
  // This is a version 67 database dump whose contents were backformed from
  // the contents of the version 68 database dump (the v68 migration was
  // actually written first).
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE metas (metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,"
          "ctime bigint default 0,server_ctime bigint default 0,"
          "server_position_in_parent bigint default 0,"
          "local_external_id bigint default 0,id varchar(255) default 'r',"
          "parent_id varchar(255) default 'r',"
          "server_parent_id varchar(255) default 'r',"
          "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
          "is_unsynced bit default 0,is_unapplied_update bit default 0,"
          "is_del bit default 0,is_dir bit default 0,"
          "is_bookmark_object bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,server_is_bookmark_object bit default 0,"
          "name varchar(255), "  /* COLLATE PATHNAME, */
          "unsanitized_name varchar(255)," /* COLLATE PATHNAME, */
          "non_unique_name varchar,"
          "server_name varchar(255),"  /* COLLATE PATHNAME */
          "server_non_unique_name varchar,"
          "bookmark_url varchar,server_bookmark_url varchar,"
          "singleton_tag varchar,bookmark_favicon blob,"
          "server_bookmark_favicon blob);"
      "INSERT INTO metas VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,0,0,NULL,"
          "NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,"
          "4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,1,0,1,1,"
          "'Deleted Item',NULL,'Deleted Item','Deleted Item','Deleted Item',"
          "'http://www.google.com/','http://www.google.com/2',NULL,'AASGASGA',"
          "'ASADGADGADG');"
      "INSERT INTO metas VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,1,0,1,1,"
          "'Welcome to Chromium',NULL,'Welcome to Chromium',"
          "'Welcome to Chromium','Welcome to Chromium',"
          "'http://www.google.com/chrome/intl/en/welcome.html',"
          "'http://www.google.com/chrome/intl/en/welcome.html',NULL,NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,"
          "7,'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,1,0,1,1,"
          "'Google',NULL,'Google','Google','Google','http://www.google.com/',"
          "'http://www.google.com/',NULL,'AGASGASG','AGFDGASG');"
      "INSERT INTO metas VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,"
          "6,'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,1,0,1,"
          "'The Internet',NULL,'The Internet','The Internet',"
          "'The Internet',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ","
          "1048576,0,'s_ID_7','r','r','r','r',0,0,0,1,1,1,0,1,"
          "'Google Chrome',NULL,'Google Chrome','Google Chrome',"
          "'Google Chrome',NULL,NULL,'google_chrome',NULL,NULL);"
      "INSERT INTO metas VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,"
          "0,'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,1,0,1,'Bookmarks',"
          "NULL,'Bookmarks','Bookmarks','Bookmarks',NULL,NULL,"
          "'google_chrome_bookmarks',NULL,NULL);"
      "INSERT INTO metas VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ","
          "1048576,1,'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,1,0,"
          "1,'Bookmark Bar',NULL,'Bookmark Bar','Bookmark Bar','Bookmark Bar',"
          "NULL,NULL,'bookmark_bar',NULL,NULL);"
      "INSERT INTO metas VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,"
          "2,'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,1,0,1,"
          "'Other Bookmarks',NULL,'Other Bookmarks','Other Bookmarks',"
          "'Other Bookmarks',NULL,NULL,'other_bookmarks',"
          "NULL,NULL);"
      "INSERT INTO metas VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,1,0,0,1,"
          "'Home (The Chromium Projects)',NULL,'Home (The Chromium Projects)',"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "'http://dev.chromium.org/','http://dev.chromium.org/other',NULL,"
          "'AGATWA','AFAGVASF');"
      "INSERT INTO metas VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,1,0,1,"
          "'Extra Bookmarks',NULL,'Extra Bookmarks','Extra Bookmarks',"
          "'Extra Bookmarks',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,1,0,0,"
          "1,'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN  Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'http://www.icann.com/','http://www.icann.com/',NULL,"
          "'PNGAXF0AAFF','DAAFASF');"
      "INSERT INTO metas VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,"
          "11,'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,1,0,0,1,"
          "'The WebKit Open Source Project',NULL,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "'The WebKit Open Source Project','http://webkit.org/',"
          "'http://webkit.org/x',NULL,'PNGX','PNG2Y');"
      "CREATE TABLE share_info (id VARCHAR(128) primary key, "
          "last_sync_timestamp INT, name VARCHAR(128), "
          "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
          "db_create_version VARCHAR(128), db_create_time int, "
          "next_id bigint default -2, cache_guid VARCHAR(32));"
      "INSERT INTO share_info VALUES('nick@chromium.org',694,"
          "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
          "'Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO share_version VALUES('nick@chromium.org',68);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion68Database(sql::Database* connection) {
  // This sets up an actual version 68 database dump.  The IDs were
  // canonicalized to be less huge, and the favicons were overwritten
  // with random junk so that they didn't contain any unprintable
  // characters.  A few server URLs were tweaked so that they'd be
  // different from the local URLs.  Lastly, the custom collation on
  // the server_non_unique_name column was removed.
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE metas (metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,"
          "ctime bigint default 0,server_ctime bigint default 0,"
          "server_position_in_parent bigint default 0,"
          "local_external_id bigint default 0,id varchar(255) default 'r',"
          "parent_id varchar(255) default 'r',"
          "server_parent_id varchar(255) default 'r',"
          "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
          "is_unsynced bit default 0,is_unapplied_update bit default 0,"
          "is_del bit default 0,is_dir bit default 0,"
          "is_bookmark_object bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,"
          "server_is_bookmark_object bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "bookmark_url varchar,server_bookmark_url varchar,"
          "singleton_tag varchar,bookmark_favicon blob,"
          "server_bookmark_favicon blob);"
      "INSERT INTO metas VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,0,0,NULL,"
          "NULL,NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,"
          "4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,1,0,1,1,"
          "'Deleted Item','Deleted Item','http://www.google.com/',"
          "'http://www.google.com/2',NULL,'AASGASGA','ASADGADGADG');"
      "INSERT INTO metas VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,1,0,1,1,"
          "'Welcome to Chromium','Welcome to Chromium',"
          "'http://www.google.com/chrome/intl/en/welcome.html',"
          "'http://www.google.com/chrome/intl/en/welcome.html',NULL,NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,"
          "7,'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,1,0,1,1,"
          "'Google','Google','http://www.google.com/',"
          "'http://www.google.com/',NULL,'AGASGASG','AGFDGASG');"
      "INSERT INTO metas VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,"
          "6,'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,1,0,1,"
          "'The Internet','The Internet',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ","
          "1048576,0,'s_ID_7','r','r','r','r',0,0,0,1,1,1,0,1,"
          "'Google Chrome','Google Chrome',NULL,NULL,'google_chrome',NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,"
          "0,'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,1,0,1,'Bookmarks',"
          "'Bookmarks',NULL,NULL,'google_chrome_bookmarks',NULL,NULL);"
      "INSERT INTO metas VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ","
          "1048576,1,'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,1,0,"
          "1,'Bookmark Bar','Bookmark Bar',NULL,NULL,'bookmark_bar',NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,"
          "2,'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,1,0,1,"
          "'Other Bookmarks','Other Bookmarks',NULL,NULL,'other_bookmarks',"
          "NULL,NULL);"
      "INSERT INTO metas VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,1,0,0,1,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "'http://dev.chromium.org/','http://dev.chromium.org/other',NULL,"
          "'AGATWA','AFAGVASF');"
      "INSERT INTO metas VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,1,0,1,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,1,0,0,"
          "1,'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'http://www.icann.com/','http://www.icann.com/',NULL,"
          "'PNGAXF0AAFF','DAAFASF');"
      "INSERT INTO metas VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,"
          "11,'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,1,0,0,1,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "'http://webkit.org/','http://webkit.org/x',NULL,'PNGX','PNG2Y');"
      "CREATE TABLE share_info (id VARCHAR(128) primary key, "
          "last_sync_timestamp INT, name VARCHAR(128), "
          "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
          "db_create_version VARCHAR(128), db_create_time int, "
          "next_id bigint default -2, cache_guid VARCHAR(32));"
      "INSERT INTO share_info VALUES('nick@chromium.org',694,"
          "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
          "'Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO share_version VALUES('nick@chromium.org',68);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion69Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE metas (metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,"
          "ctime bigint default 0,server_ctime bigint default 0,"
          "server_position_in_parent bigint default 0,"
          "local_external_id bigint default 0,id varchar(255) default 'r',"
          "parent_id varchar(255) default 'r',"
          "server_parent_id varchar(255) default 'r',"
          "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
          "is_unsynced bit default 0,is_unapplied_update bit default 0,"
          "is_del bit default 0,is_dir bit default 0,"
          "is_bookmark_object bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,"
          "server_is_bookmark_object bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "bookmark_url varchar,server_bookmark_url varchar,"
          "singleton_tag varchar,bookmark_favicon blob,"
          "server_bookmark_favicon blob, specifics blob, "
          "server_specifics blob);"
      "INSERT INTO metas VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,0,0,NULL,NULL,NULL,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO metas VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,"
          "4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,1,0,1,1,"
          "'Deleted Item','Deleted Item','http://www.google.com/',"
          "'http://www.google.com/2',NULL,'AASGASGA','ASADGADGADG',"
          "X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F120841415"
          "34741534741',X'C28810260A17687474703A2F2F7777772E676F6F676C652E636F"
          "6D2F32120B4153414447414447414447');"
      "INSERT INTO metas VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,1,0,1,1,"
          "'Welcome to Chromium','Welcome to Chromium',"
          "'http://www.google.com/chrome/intl/en/welcome.html',"
          "'http://www.google.com/chrome/intl/en/welcome.html',NULL,NULL,NULL,"
          "X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A3168"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F6"
          "56E2F77656C636F6D652E68746D6C1200');"
      "INSERT INTO metas VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,1,0,1,1,"
          "'Google','Google','http://www.google.com/',"
          "'http://www.google.com/',NULL,'AGASGASG','AGFDGASG',X'C28810220A166"
          "87474703A2F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'"
          "C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464"
          "447415347');"
      "INSERT INTO metas VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,6"
          ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,1,0,1,'The Internet',"
          "'The Internet',NULL,NULL,NULL,NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,1,0,1,'Google Chrome',"
          "'Google Chrome',NULL,NULL,'google_chrome',NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,1,0,1,'Bookmarks',"
          "'Bookmarks',NULL,NULL,'google_chrome_bookmarks',NULL,NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,1,0,1,"
          "'Bookmark Bar','Bookmark Bar',NULL,NULL,'bookmark_bar',NULL,NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,1,0,1,"
          "'Other Bookmarks','Other Bookmarks',NULL,NULL,'other_bookmarks',"
          "NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,1,0,0,1,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "'http://dev.chromium.org/','http://dev.chromium.org/other',NULL,"
          "'AGATWA','AFAGVASF',X'C28810220A18687474703A2F2F6465762E6368726F6D6"
          "9756D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F646576"
          "2E6368726F6D69756D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO metas VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,1,0,1,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,NULL,NULL,NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,1,0,0,"
          "1,'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'http://www.icann.com/','http://www.icann.com/',NULL,'PNGAXF0AAFF',"
          "'DAAFASF',X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F1"
          "20B504E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963"
          "616E6E2E636F6D2F120744414146415346');"
      "INSERT INTO metas VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,1,0,0,1,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "'http://webkit.org/','http://webkit.org/x',NULL,'PNGX','PNG2Y',"
          "X'C288101A0A12687474703A2F2F7765626B69742E6F72672F1204504E4758',X'C2"
          "88101C0A13687474703A2F2F7765626B69742E6F72672F781205504E473259');"
      "CREATE TABLE share_info (id VARCHAR(128) primary key, "
          "last_sync_timestamp INT, name VARCHAR(128), "
          "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
          "db_create_version VARCHAR(128), db_create_time int, "
          "next_id bigint default -2, cache_guid VARCHAR(32));"
      "INSERT INTO share_info VALUES('nick@chromium.org',694,"
          "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
          "'Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO share_version VALUES('nick@chromium.org',69);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion70Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE share_info (id VARCHAR(128) primary key, "
          "last_sync_timestamp INT, name VARCHAR(128), "
          "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
          "db_create_version VARCHAR(128), db_create_time int, "
          "next_id bigint default -2, cache_guid VARCHAR(32));"
      "INSERT INTO share_info VALUES('nick@chromium.org',694,"
          "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
          "'Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO share_version VALUES('nick@chromium.org',70);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,"
          "ctime bigint default 0,server_ctime bigint default 0,"
          "server_position_in_parent bigint default 0,"
          "local_external_id bigint default 0,id varchar(255) default 'r',"
          "parent_id varchar(255) default 'r',"
          "server_parent_id varchar(255) default 'r',"
          "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
          "is_unsynced bit default 0,is_unapplied_update bit default 0,"
          "is_del bit default 0,is_dir bit default 0,"
          "server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,"
          "specifics blob,server_specifics blob);"
      "INSERT INTO metas VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'');"
      "INSERT INTO metas VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2) ","
          "-2097152,4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,"
          "1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A"
          "2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X'C2881026"
          "0A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B415341444741"
          "4447414447');"
      "INSERT INTO metas VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A"
          "31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E74"
          "6C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F"
          "2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F7765"
          "6C636F6D652E68746D6C1200');"
      "INSERT INTO metas VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C6"
          "52E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F77777"
          "72E676F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO metas VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,"
          "6,'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome',"
          "'Google Chrome','google_chrome',NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO metas VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,"
          "1,'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,"
          "'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO metas VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ","
          "2097152,2,'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F"
          "72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636872"
          "6F6D69756D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO metas VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO metas VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F"
          "120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772E69"
          "63616E6E2E636F6D2F120744414146415346');"
      "INSERT INTO metas VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,"
          "11,'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F78120550"
          "4E473259');"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion71Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',71);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,ctime bigint "
          "default 0,server_ctime bigint default 0,server_position_in_parent "
          "bigint default 0,local_external_id bigint default 0,id varchar(255) "
          "default 'r',parent_id varchar(255) default 'r',server_parent_id "
          "varchar(255) default 'r',prev_id varchar(255) default 'r',next_id "
          "varchar(255) default 'r',is_unsynced bit default 0,"
          "is_unapplied_update bit default 0,is_del bit default 0,is_dir bit "
          "default 0,server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,specifics blob,"
          "server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,4,"
          "'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,"
          "'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F"
          "7777772E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A1768"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474144"
          "47');"
      "INSERT INTO 'metas' VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,3,"
          "'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A31"
          "687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F"
          "656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7777"
          "772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D"
          "652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652"
          "E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,6,"
          "'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome'"
          ",'google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar',"
          "'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,8,"
          "'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',NULL,"
          "NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1"
          "206414741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,10,"
          "'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',NULL,"
          "NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
          "E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963616E6E2"
          "E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,""X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E"
          "473259');"
      "CREATE TABLE models (model_id BLOB primary key, "
          "last_download_timestamp INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, "
          "store_birthday TEXT, db_create_version TEXT, db_create_time INT, "
          "next_id INT default -2, cache_guid TEXT);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion72Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',72);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,ctime bigint "
          "default 0,server_ctime bigint default 0,server_position_in_parent "
          "bigint default 0,local_external_id bigint default 0,id varchar(255) "
          "default 'r',parent_id varchar(255) default 'r',server_parent_id "
          "varchar(255) default 'r',prev_id varchar(255) default 'r',next_id "
          "varchar(255) default 'r',is_unsynced bit default 0,"
          "is_unapplied_update bit default 0,is_del bit default 0,is_dir bit "
          "default 0,server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,specifics blob,"
          "server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,4,"
          "'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,"
          "'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F"
          "7777772E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A1768"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474144"
          "47');"
      "INSERT INTO 'metas' VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,3,"
          "'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A31"
          "687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F"
          "656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7777"
          "772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D"
          "652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652"
          "E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,6,"
          "'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome'"
          ",'google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar',"
          "'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,8,"
          "'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',NULL,"
          "NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1"
          "206414741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,10,"
          "'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',NULL,"
          "NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
          "E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963616E6E2"
          "E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,""X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E"
          "473259');"
      "CREATE TABLE models (model_id BLOB primary key, "
          "last_download_timestamp INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, "
          "store_birthday TEXT, db_create_version TEXT, db_create_time INT, "
          "next_id INT default -2, cache_guid TEXT);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion73Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',73);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,ctime bigint "
          "default 0,server_ctime bigint default 0,server_position_in_parent "
          "bigint default 0,local_external_id bigint default 0,id varchar(255) "
          "default 'r',parent_id varchar(255) default 'r',server_parent_id "
          "varchar(255) default 'r',prev_id varchar(255) default 'r',next_id "
          "varchar(255) default 'r',is_unsynced bit default 0,"
          "is_unapplied_update bit default 0,is_del bit default 0,is_dir bit "
          "default 0,server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,specifics blob,"
          "server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,4,"
          "'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,"
          "'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F"
          "7777772E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A1768"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474144"
          "47');"
      "INSERT INTO 'metas' VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,3,"
          "'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A31"
          "687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F"
          "656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7777"
          "772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D"
          "652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652"
          "E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,6,"
          "'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome'"
          ",'google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar',"
          "'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,8,"
          "'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',NULL,"
          "NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1"
          "206414741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,10,"
          "'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',NULL,"
          "NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
          "E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963616E6E2"
          "E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,""X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E"
          "473259');"
      "CREATE TABLE models (model_id BLOB primary key, "
          "last_download_timestamp INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, "
          "store_birthday TEXT, db_create_version TEXT, db_create_time INT, "
          "next_id INT default -2, cache_guid TEXT, "
          "notification_state BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x',X'C2881000');"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion74Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',74);"
      "CREATE TABLE models (model_id BLOB primary key, last_download_timestamp"
          " INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthd"
          "ay TEXT, db_create_version TEXT, db_create_time INT, next_id INT de"
          "fault -2, cache_guid TEXT , notification_state BLOB, autofill_migra"
          "tion_state INT default 0, bookmarks_added_during_autofill_migration"
          " INT default 0, autofill_migration_time INT default 0, autofill_ent"
          "ries_added_during_migration INT default 0, autofill_profiles_added_"
          "during_migration INT default 0);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org'"
          ",'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542"
          ",'9010788312004066376x-6609234393368420856x',NULL,0,0,0,0,0);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,bas"
          "e_version bigint default -1,server_version bigint default 0,mtime b"
          "igint default 0,server_mtime bigint default 0,ctime bigint default "
          "0,server_ctime bigint default 0,server_position_in_parent bigint de"
          "fault 0,local_external_id bigint default 0,id varchar(255) default "
          "'r',parent_id varchar(255) default 'r',server_parent_id varchar(255"
          ") default 'r',prev_id varchar(255) default 'r',next_id varchar(255)"
          " default 'r',is_unsynced bit default 0,is_unapplied_update bit defa"
          "ult 0,is_del bit default 0,is_dir bit default 0,server_is_dir bit d"
          "efault 0,server_is_del bit default 0,non_unique_name varchar,server"
          "_non_unique_name varchar(255),unique_server_tag varchar,unique_clie"
          "nt_tag varchar,specifics blob,server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'"
          "');"
      "INSERT INTO 'metas' VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,4,'s_ID_2','s_ID"
          "_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,'Deleted Item','Deleted "
          "Item',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652E6"
          "36F6D2F12084141534741534741',X'C28810260A17687474703A2F2F7777772E67"
          "6F6F676C652E636F6D2F32120B4153414447414447414447');"
      "INSERT INTO 'metas' VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,3,'s_ID_4','s_ID"
          "_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,'Welcome to Chromium','W"
          "elcome to Chromium',NULL,NULL,X'C28810350A31687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D652E"
          "68746D6C1200',X'C28810350A31687474703A2F2F7777772E676F6F676C652E636"
          "F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,'s_ID_5','s_ID_"
          "9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google','Google',NULL,NU"
          "LL,X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F120841"
          "47415347415347',X'C28810220A16687474703A2F2F7777772E676F6F676C652E6"
          "36F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,6,'s_ID_6','s_ID"
          "_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet','The Internet',NULL"
          ",NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,'s_ID_7','r','r"
          "','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google_chrom"
          "e',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,'s_ID_8','s_ID_"
          "7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks','Bookmarks','google_chr"
          "ome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,1,'s_ID_9','s_ID_"
          "8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar'"
          ",'bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,2,'s_ID_10','s_I"
          "D_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,'Other Bookmarks','Other Boo"
          "kmarks','other_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,8,'s_ID_11','s_"
          "ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,'Home (The Chromium Projec"
          "ts)','Home (The Chromium Projects)',NULL,NULL,X'C28810220A186874747"
          "03A2F2F6465762E6368726F6D69756D2E6F72672F1206414741545741',X'C28810"
          "290A1D687474703A2F2F6465762E6368726F6D69756D2E6F72672F6F74686572120"
          "84146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,'s_ID_12','s_ID_6','"
          "s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bo"
          "okmarks',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,10,'s_ID_13','s_"
          "ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,'ICANN | Internet Co"
          "rporation for Assigned Names and Numbers','ICANN | Internet Corpora"
          "tion for Assigned Names and Numbers',NULL,NULL,X'C28810240A15687474"
          "703A2F2F7777772E6963616E6E2E636F6D2F120B504E474158463041414646',X'C"
          "28810200A15687474703A2F2F7777772E6963616E6E2E636F6D2F12074441414641"
          "5346');"
      "INSERT INTO 'metas' VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,11,'s_ID_14','s_"
          "ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,'The WebKit Open Source Pr"
          "oject','The WebKit Open Source Project',NULL,NULL,X'C288101A0A12687"
          "474703A2F2F7765626B69742E6F72672F1204504E4758',X'C288101C0A13687474"
          "703A2F2F7765626B69742E6F72672F781205504E473259');"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion75Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',75);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthd"
          "ay TEXT, db_create_version TEXT, db_create_time INT, next_id INT de"
          "fault -2, cache_guid TEXT , notification_state BLOB, autofill_migra"
          "tion_state INT default 0,bookmarks_added_during_autofill_migration "
          "INT default 0, autofill_migration_time INT default 0, autofill_entr"
          "ies_added_during_migration INT default 0, autofill_profiles_added_d"
          "uring_migration INT default 0);"
       "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org"
           "','c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-655"
           "42,'9010788312004066376x-6609234393368420856x',NULL,0,0,0,0,0);"
       "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, "
           "initial_sync_ended BOOLEAN default 0);"
       "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
       "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,ba"
           "se_version bigint default -1,server_version bigint default 0,mtime"
           " bigint default 0,server_mtime bigint default 0,ctime bigint defau"
           "lt 0,server_ctime bigint default 0,server_position_in_parent bigin"
           "t default 0,local_external_id bigint default 0,id varchar(255) def"
           "ault 'r',parent_id varchar(255) default 'r',server_parent_id varch"
           "ar(255) default 'r',prev_id varchar(255) default 'r',next_id varch"
           "ar(255) default 'r',is_unsynced bit default 0,is_unapplied_update "
           "bit default 0,is_del bit default 0,is_dir bit default 0,server_is_"
           "dir bit default 0,server_is_del bit default 0,non_unique_name varc"
           "har,server_non_unique_name varchar(255),unique_server_tag varchar,"
           "unique_client_tag varchar,specifics blob,server_specifics blob);"
           "INSERT INTO 'metas' VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
              ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NUL"
              "L,X'',X'');"
           "INSERT INTO 'metas' VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
              ",-2097152,4,'s_ID_"
              "2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,'Deleted Ite"
              "m','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F7777772"
              "E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A176874"
              "74703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474"
              "14447');"
           "INSERT INTO 'metas' VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
              ",-3145728,3,'s_ID_"
              "4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,'Welcome to "
              "Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A316874747"
              "03A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F65"
              "6E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7"
              "777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F7765"
              "6C636F6D652E68746D6C1200');"
           "INSERT INTO 'metas' VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
              ",1048576,7,'s_ID_5"
              "','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google','Goo"
              "gle',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C65"
              "2E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F777"
              "7772E676F6F676C652E636F6D2F12084147464447415347');"
           "INSERT INTO 'metas' VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
              ",-4194304,6,'s_ID_"
              "6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet','The In"
              "ternet',NULL,NULL,X'C2881000',X'C2881000');"
           "INSERT INTO 'metas' VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
              ",1048576,0,'s_ID_7"
              "','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','"
              "google_chrome',NULL,NULL,NULL);"
           "INSERT INTO 'metas' VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
              ",1048576,0,'s_ID_8"
              "','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks','Bookmarks'"
              ",'google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
           "INSERT INTO 'metas' VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
              ",1048576,1,'s_ID_9"
              "','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar','B"
              "ookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
           "INSERT INTO 'metas' VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
              ",2097152,2,'s_ID_"
              "10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,'Other Bookmarks"
              "','Other Bookmarks','other_bookmarks',NULL,X'C2881000',X'C28810"
              "00');"
           "INSERT INTO 'metas' VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
              ",-1048576,8,'s_ID"
              "_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,'Home (The Chr"
              "omium Projects)','Home (The Chromium Projects)',NULL,NULL,X'C28"
              "810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F120641"
              "4741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
              "D2E6F72672F6F7468657212084146414756415346');"
           "INSERT INTO 'metas' VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
              ",0,9,'s_ID_12','s"
              "_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,'Extra Bookmark"
              "s','Extra Bookmarks',NULL,NULL,X'C2881000',X'C2881000');"
           "INSERT INTO 'metas' VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
              ",-917504,10,'s_ID"
              "_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,'ICANN |"
              " Internet Corporation for Assigned Names and Numbers','ICANN | "
              "Internet Corporation for Assigned Names and Numbers',NULL,NULL,"
              "X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
              "E474158463041414646',X'C28810200A15687474703A2F2F7777772E696361"
              "6E6E2E636F6D2F120744414146415346');"
           "INSERT INTO 'metas' VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
              ",1048576,11,'s_ID"
              "_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,'The WebKit Op"
              "en Source Project','The WebKit Open Source Project',NULL,NULL,X"
              "'C288101A0A12687474703A2F2F7765626B69742E6F72672F1204504E4758',"
              "X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E473"
              "259');"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion76Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',76);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,mtime big"
          "int default 0,server_mtime bigint default 0,ctime bigint default 0,s"
          "erver_ctime bigint default 0,server_position_in_parent bigint defaul"
          "t 0,local_external_id bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0," LEGACY_PROTO_TIME_VALS(1)
          ",0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'')"
          ";"
      "INSERT INTO 'metas' VALUES(2,669,669," LEGACY_PROTO_TIME_VALS(2)
          ",-2097152,4,'s_ID_2','s_ID_9"
          "','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,'Deleted Item','Deleted Ite"
          "m',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6"
          "D2F12084141534741534741',X'C28810260A17687474703A2F2F7777772E676F6F6"
          "76C652E636F6D2F32120B4153414447414447414447');"
      "INSERT INTO 'metas' VALUES(4,681,681," LEGACY_PROTO_TIME_VALS(4)
          ",-3145728,3,'s_ID_4','s_ID_9"
          "','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,'Welcome to Chromium','Welc"
          "ome to Chromium',NULL,NULL,X'C28810350A31687474703A2F2F7777772E676F6"
          "F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D652E68746"
          "D6C1200',X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6"
          "368726F6D652F696E746C2F656E2F77656C636F6D652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677," LEGACY_PROTO_TIME_VALS(5)
          ",1048576,7,'s_ID_5','s_ID_9'"
          ",'s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google','Google',NULL,NULL,"
          "X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F1208414741"
          "5347415347',X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D"
          "2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694," LEGACY_PROTO_TIME_VALS(6)
          ",-4194304,6,'s_ID_6','s_ID_9"
          "','s_ID_9','r','r',0,0,0,1,1,0,'The Internet','The Internet',NULL,NU"
          "LL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663," LEGACY_PROTO_TIME_VALS(7)
          ",1048576,0,'s_ID_7','r','r',"
          "'r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google_chrome',"
          "NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664," LEGACY_PROTO_TIME_VALS(8)
          ",1048576,0,'s_ID_8','s_ID_7'"
          ",'s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks','Bookmarks','google_chrome"
          "_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665," LEGACY_PROTO_TIME_VALS(9)
          ",1048576,1,'s_ID_9','s_ID_8'"
          ",'s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar','b"
          "ookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666," LEGACY_PROTO_TIME_VALS(10)
          ",2097152,2,'s_ID_10','s_ID_"
          "8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,'Other Bookmarks','Other Bookma"
          "rks','other_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683," LEGACY_PROTO_TIME_VALS(11)
          ",-1048576,8,'s_ID_11','s_ID"
          "_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,'Home (The Chromium Projects)"
          "','Home (The Chromium Projects)',NULL,NULL,X'C28810220A18687474703A2"
          "F2F6465762E6368726F6D69756D2E6F72672F1206414741545741',X'C28810290A1"
          "D687474703A2F2F6465762E6368726F6D69756D2E6F72672F6F74686572120841464"
          "14756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685," LEGACY_PROTO_TIME_VALS(12)
          ",0,9,'s_ID_12','s_ID_6','s_"
          "ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookm"
          "arks',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687," LEGACY_PROTO_TIME_VALS(13)
          ",-917504,10,'s_ID_13','s_ID"
          "_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,'ICANN | Internet Corpo"
          "ration for Assigned Names and Numbers','ICANN | Internet Corporation"
          " for Assigned Names and Numbers',NULL,NULL,X'C28810240A15687474703A2"
          "F2F7777772E6963616E6E2E636F6D2F120B504E474158463041414646',X'C288102"
          "00A15687474703A2F2F7777772E6963616E6E2E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692," LEGACY_PROTO_TIME_VALS(14)
          ",1048576,11,'s_ID_14','s_ID"
          "_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,'The WebKit Open Source Proje"
          "ct','The WebKit Open Source Project',NULL,NULL,X'C288101A0A126874747"
          "03A2F2F7765626B69742E6F72672F1204504E4758',X'C288101C0A13687474703A2"
          "F2F7765626B69742E6F72672F781205504E473259');"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,'"
          "9010788312004066376x-6609234393368420856x',NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion77Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',77);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,server_po"
          "sition_in_parent bigint default 0,local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0," META_PROTO_TIMES_VALS(1)
          ",'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669,-2097152,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447');"
      "INSERT INTO 'metas' VALUES(4,681,681,-3145728,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677,1048576,7," META_PROTO_TIMES_VALS(5)
          ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google','"
          "Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652E"
          "636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E67"
          "6F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694,-4194304,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ");"
      "INSERT INTO 'metas' VALUES(7,663,663,1048576,0," META_PROTO_TIMES_VALS(7)
          ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Goo"
          "gle Chrome','google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,1048576,0," META_PROTO_TIMES_VALS(8)
          ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks','Bookmar"
          "ks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665,1048576,1," META_PROTO_TIMES_VALS(9)
          ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar'"
          ",'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666,2097152,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683,-1048576,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685,0,9," META_PROTO_TIMES_VALS(12)
          ",'s_ID_12','s_ID_6','s_"
          "ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookm"
          "arks',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687,-917504,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692,1048576,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259');"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,'"
          "9010788312004066376x-6609234393368420856x',NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion78Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',78);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,server_po"
          "sition_in_parent bigint default 0,local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ");"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL);"
      "INSERT INTO 'metas' VALUES(2,669,669,-2097152,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL);"
      "INSERT INTO 'metas' VALUES(4,681,681,-3145728,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL);"
      "INSERT INTO 'metas' VALUES(5,677,677,1048576,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,-4194304,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,1048576,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,1048576,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1048576,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2097152,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,-1048576,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,0,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(13,687,687,-917504,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,1048576,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,'"
          "9010788312004066376x-6609234393368420856x',NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion79Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',79);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,server_po"
          "sition_in_parent bigint default 0,local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ");"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL);"
      "INSERT INTO 'metas' VALUES(2,669,669,-2097152,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL);"
      "INSERT INTO 'metas' VALUES(4,681,681,-3145728,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL);"
      "INSERT INTO 'metas' VALUES(5,677,677,1048576,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,-4194304,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,1048576,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,1048576,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1048576,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2097152,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,-1048576,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,0,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(13,687,687,-917504,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,1048576,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion80Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',80);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,server_po"
          "sition_in_parent bigint default 0,local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ");"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
      "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL);"
      "INSERT INTO 'metas' VALUES(2,669,669,-2097152,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL);"
      "INSERT INTO 'metas' VALUES(4,681,681,-3145728,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL);"
      "INSERT INTO 'metas' VALUES(5,677,677,1048576,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,-4194304,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,1048576,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,1048576,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1048576,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2097152,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,-1048576,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,0,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL);"
      "INSERT INTO 'metas' VALUES(13,687,687,-917504,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,1048576,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB, bag_of_chips "
          "blob);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL, NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}


// Helper definitions to create the version 81 DB tables.
namespace {

const int V80_ROW_COUNT = 13;
const int64_t V80_POSITIONS[V80_ROW_COUNT] = {
    0,       -2097152, -3145728, 1048576, -4194304, 1048576, 1048576,
    1048576, 2097152,  -1048576, 0,       -917504,  1048576};

std::string V81_Ordinal(int n) {
  return Int64ToNodeOrdinal(V80_POSITIONS[n]).ToInternalValue();
}

}  // namespace

// Unlike the earlier versions, the rows for version 81 are generated
// programmatically to accurately handle unprintable characters for the
// server_ordinal_in_parent field.
void MigrationTest::SetUpVersion81Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',81);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB, bag_of_chips "
          "blob);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL, NULL);"));

      const char* insert_stmts[V80_ROW_COUNT] = {
      "INSERT INTO 'metas' VALUES(1,-1,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL,?);",
      "INSERT INTO 'metas' VALUES(2,669,669,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL,?);",
      "INSERT INTO 'metas' VALUES(4,681,681,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL,?);",
      "INSERT INTO 'metas' VALUES(5,677,677,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL,?);",
      "INSERT INTO 'metas' VALUES(6,694,694,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL,?);",
      "INSERT INTO 'metas' VALUES(7,663,663,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL,?);",
      "INSERT INTO 'metas' VALUES(8,664,664,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(9,665,665,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(10,666,666,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(11,683,683,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL,?);",
      "INSERT INTO 'metas' VALUES(12,685,685,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(13,687,687,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL,?);",
      "INSERT INTO 'metas' VALUES(14,692,692,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL,?);" };

  for (int i = 0; i < V80_ROW_COUNT; i++) {
    sql::Statement s(connection->GetUniqueStatement(insert_stmts[i]));
    std::string ord = V81_Ordinal(i);
    s.BindBlob(0, ord.data(), ord.length());
    ASSERT_TRUE(s.Run());
    s.Reset(true);
  }
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion82Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',82);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0, transaction_version BIGINT "
          "default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1, 1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB, bag_of_chips "
          "blob);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL, NULL);"));

      const char* insert_stmts[V80_ROW_COUNT] = {
      "INSERT INTO 'metas' VALUES(1,-1,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL,?);",
      "INSERT INTO 'metas' VALUES(2,669,669,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL,?);",
      "INSERT INTO 'metas' VALUES(4,681,681,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL,?);",
      "INSERT INTO 'metas' VALUES(5,677,677,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL,?);",
      "INSERT INTO 'metas' VALUES(6,694,694,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL,?);",
      "INSERT INTO 'metas' VALUES(7,663,663,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL,?);",
      "INSERT INTO 'metas' VALUES(8,664,664,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(9,665,665,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(10,666,666,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(11,683,683,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL,?);",
      "INSERT INTO 'metas' VALUES(12,685,685,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL,?);",
      "INSERT INTO 'metas' VALUES(13,687,687,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL,?);",
      "INSERT INTO 'metas' VALUES(14,692,692,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL,?);" };

  for (int i = 0; i < V80_ROW_COUNT; i++) {
    sql::Statement s(connection->GetUniqueStatement(insert_stmts[i]));
    std::string ord = V81_Ordinal(i);
    s.BindBlob(0, ord.data(), ord.length());
    ASSERT_TRUE(s.Run());
    s.Reset(true);
  }
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion83Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',83);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0, transaction_version BIGINT "
          "default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1, 1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob, transaction_version bigint default "
          "0);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB, bag_of_chips "
          "blob);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL, NULL);"));

      const char* insert_stmts[V80_ROW_COUNT] = {
      "INSERT INTO 'metas' VALUES(1,-1,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(2,669,669,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(4,681,681,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(5,677,677,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL,?,0);",
      "INSERT INTO 'metas' VALUES(6,694,694,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL,?,0);",
      "INSERT INTO 'metas' VALUES(7,663,663,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL,?,0);"
          "",
      "INSERT INTO 'metas' VALUES(8,664,664,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(9,665,665,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(10,666,666,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(11,683,683,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(12,685,685,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(13,687,687,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(14,692,692,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL,?,0);" };

  for (int i = 0; i < V80_ROW_COUNT; i++) {
    sql::Statement s(connection->GetUniqueStatement(insert_stmts[i]));
    std::string ord = V81_Ordinal(i);
    s.BindBlob(0, ord.data(), ord.length());
    ASSERT_TRUE(s.Run());
    s.Reset(true);
  }
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion84Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',84);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, in"
          "itial_sync_ended BOOLEAN default 0, transaction_version BIGINT "
          "default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1, 1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob, transaction_version bigint default "
          "0);"
      "CREATE TABLE 'deleted_metas'"
          "(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob, transaction_version bigint default "
          "0);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB, bag_of_chips "
          "blob);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL, NULL);"));

      const char* insert_stmts[V80_ROW_COUNT] = {
      "INSERT INTO 'metas' VALUES(1,-1,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(2,669,669,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(4,681,681,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(5,677,677,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL,?,0);",
      "INSERT INTO 'metas' VALUES(6,694,694,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL,?,0);",
      "INSERT INTO 'metas' VALUES(7,663,663,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL,?,0);"
          "",
      "INSERT INTO 'metas' VALUES(8,664,664,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(9,665,665,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(10,666,666,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(11,683,683,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(12,685,685,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(13,687,687,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(14,692,692,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL,?,0);" };

  for (int i = 0; i < V80_ROW_COUNT; i++) {
    sql::Statement s(connection->GetUniqueStatement(insert_stmts[i]));
    std::string ord = V81_Ordinal(i);
    s.BindBlob(0, ord.data(), ord.length());
    ASSERT_TRUE(s.Run());
    s.Reset(true);
  }
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion85Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',85);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, "
          "transaction_version BIGINT default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605', 1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob, transaction_version bigint default "
          "0);"
      "CREATE TABLE 'deleted_metas'"
          "(metahandle bigint primary key ON CONFLICT FAIL,base"
          "_version bigint default -1,server_version bigint default 0,         "
          "local_external_id bigint default 0"
          ",mtime bigint default 0,server_mtime bigint default 0,ctime bigint d"
          "efault 0,server_ctime bigint default 0,id varchar(255) default 'r',p"
          "arent_id varchar(255) default 'r',server_parent_id varchar(255) defa"
          "ult 'r',prev_id varchar(255) default 'r',next_id varchar(255) defaul"
          "t 'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is"
          "_del bit default 0,is_dir bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,non_unique_name varchar,server_non_uniqu"
          "e_name varchar(255),unique_server_tag varchar,unique_client_tag varc"
          "har,specifics blob,server_specifics blob, base_server_specifics BLOB"
          ", server_ordinal_in_parent blob, transaction_version bigint default "
          "0);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
          "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defa"
          "ult -2, cache_guid TEXT , notification_state BLOB, bag_of_chips "
          "blob);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,"
          "-131078,'9010788312004066376x-6609234393368420856x',NULL, NULL);"));

      const char* insert_stmts[V80_ROW_COUNT] = {
      "INSERT INTO 'metas' VALUES(1,-1,0,0," META_PROTO_TIMES_VALS(1) ",'r','"
          "r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(2,669,669,4,"
          META_PROTO_TIMES_VALS(2) ",'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_"
          "2',0,0,1,0,0,1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X"
          "'C28810260A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534"
          "14447414447414447',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(4,681,681,3,"
          META_PROTO_TIMES_VALS(4) ",'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_"
          "4',0,0,1,0,0,1,'Welcome to Chromium','Welcome to Chromium',NULL,NULL"
          ",X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687"
          "474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656"
          "E2F77656C636F6D652E68746D6C1200',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(5,677,677,7,"
          META_PROTO_TIMES_VALS(5) ",'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_"
          "5',0,0,1,0,0,1,'Google','Google',NULL,NULL,X'C28810220A16687474703A2"
          "F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'C28810220A1"
          "6687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464447415347',N"
          "ULL,?,0);",
      "INSERT INTO 'metas' VALUES(6,694,694,6,"
          META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1"
          ",1,0,'The Internet','The Internet',NULL,NULL,X'C2881000',X'C2881000'"
          ",NULL,?,0);",
      "INSERT INTO 'metas' VALUES(7,663,663,0,"
          META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Goog"
          "le Chrome','Google Chrome','google_chrome',NULL,NULL,NULL,NULL,?,0);"
          "",
      "INSERT INTO 'metas' VALUES(8,664,664,0,"
          META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1"
          ",1,0,'Bookmarks','Bookmarks','google_chrome_bookmarks',NULL,X'C28810"
          "00',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(9,665,665,1,"
          META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0"
          ",0,0,1,1,0,'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881"
          "000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(10,666,666,2,"
          META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',"
          "0,0,0,1,1,0,'Other Bookmarks','Other Bookmarks','other_bookmarks',NU"
          "LL,X'C2881000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(11,683,683,8,"
          META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13'"
          ",0,0,0,0,0,0,'Home (The Chromium Projects)','Home (The Chromium Proj"
          "ects)',NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636"
          "8726F6D69756D2E6F72672F6F7468657212084146414756415346',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(12,685,685,9,"
          META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_"
          "ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C"
          "2881000',X'C2881000',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(13,687,687,10,"
          META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_"
          "ID_12',0,0,0,0,0,0,'ICANN | Internet Corporation for Assigned Names "
          "and Numbers','ICANN | Internet Corporation for Assigned Names and Nu"
          "mbers',NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636"
          "F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772"
          "E6963616E6E2E636F6D2F120744414146415346',NULL,?,0);",
      "INSERT INTO 'metas' VALUES(14,692,692,11,"
          META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r'"
          ",0,0,0,0,0,0,'The WebKit Open Source Project','The WebKit Open Sourc"
          "e Project',NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F726"
          "72F1204504E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F7"
          "81205504E473259',NULL,?,0);" };

  for (int i = 0; i < V80_ROW_COUNT; i++) {
    sql::Statement s(connection->GetUniqueStatement(insert_stmts[i]));
    std::string ord = V81_Ordinal(i);
    s.BindBlob(0, ord.data(), ord.length());
    ASSERT_TRUE(s.Run());
    s.Reset(true);
  }
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion86Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',86);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB,"
         " transaction_version BIGINT default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,b"
         "ase_version bigint default -1,server_version bigint default 0,local_e"
         "xternal_id bigint default 0,transaction_version bigint default 0,mtim"
         "e bigint default 0,server_mtime bigint default 0,ctime bigint default"
         " 0,server_ctime bigint default 0,id varchar(255) default 'r',parent_i"
         "d varchar(255) default 'r',server_parent_id varchar(255) default 'r',"
         "is_unsynced bit default 0,is_unapplied_update bit default 0,is_del bi"
         "t default 0,is_dir bit default 0,server_is_dir bit default 0,server_i"
         "s_del bit default 0,non_unique_name varchar,server_non_unique_name va"
         "rchar(255),unique_server_tag varchar,unique_client_tag varchar,unique"
         "_bookmark_tag varchar,specifics blob,server_specifics blob,base_serve"
         "r_specifics blob,server_unique_position blob,unique_position blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
         META_PROTO_TIMES_VALS(1)
         ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,"
         "X'',X'',X'',NULL,X'2200',X'2200');"
      "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
         META_PROTO_TIMES_VALS(6) ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The"
         " Internet','The Internet',NULL,NULL,X'6754307476346749735A5734654D653"
         "273625336557753582F77673D',X'C2881000',X'C2881000',NULL,X'22247FFFFFF"
         "FFFC000006754307476346749735A5734654D653273625336557753582F77673D',X'"
         "22247FFFFFFFFFC000006754307476346749735A5734654D653273625336557753582"
         "F77673D');"
      "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
         META_PROTO_TIMES_VALS(7) ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome"
         "','Google Chrome','google_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'2"
         "200');"
      "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
         META_PROTO_TIMES_VALS(8) ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Boo"
         "kmarks','Bookmarks','google_chrome_bookmarks',NULL,X'',X'C2881000',X'"
         "C2881000',NULL,X'2200',X'2200');"
      "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
         META_PROTO_TIMES_VALS(9) ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Boo"
         "kmark Bar','Bookmark Bar','bookmark_bar',NULL,X'',X'C2881000',X'C2881"
         "000',NULL,X'2200',X'2200');"
      "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
         META_PROTO_TIMES_VALS(10) ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'O"
         "ther Bookmarks','Other Bookmarks','other_bookmarks',NULL,X'',X'C28810"
         "00',X'C2881000',NULL,X'2200',X'2200');"
      "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
         META_PROTO_TIMES_VALS(11) ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'H"
         "ome (The Chromium Projects)','Home (The Chromium Projects)',NULL,NULL"
         ",X'50514C784A456D623579366267644237646A7A2B62314130346E493D',X'C28810"
         "220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F120641474154574"
         "1',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756D2E6F72672F6F74"
         "68657212084146414756415346',NULL,X'22247FFFFFFFFFF0000050514C784A456D"
         "623579366267644237646A7A2B62314130346E493D',X'22247FFFFFFFFFF00000505"
         "14C784A456D623579366267644237646A7A2B62314130346E493D');"
      "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
         META_PROTO_TIMES_VALS(12) ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'E"
         "xtra Bookmarks','Extra Bookmarks',NULL,NULL,X'7867626A704A646134635A6"
         "F616C376A49513338734B46324837773D',X'C2881000',X'C2881000',NULL,X'222"
         "480000000000000007867626A704A646134635A6F616C376A49513338734B46324837"
         "773D',X'222480000000000000007867626A704A646134635A6F616C376A495133387"
         "34B46324837773D');"
      "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
         META_PROTO_TIMES_VALS(13) ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'I"
         "CANN | Internet Corporation for Assigned Names and Numbers','ICANN | "
         "Internet Corporation for Assigned Names and Numbers',NULL,NULL,X'3142"
         "756B572F7741766956504179672B304A614A514B3452384A413D',X'C28810240A156"
         "87474703A2F2F7777772E6963616E6E2E636F6D2F120B504E474158463041414646',"
         "X'C28810200A15687474703A2F2F7777772E6963616E6E2E636F6D2F1207444141464"
         "15346',NULL,X'22247FFFFFFFFFF200003142756B572F7741766956504179672B304"
         "A614A514B3452384A413D',X'22247FFFFFFFFFF200003142756B572F774176695650"
         "4179672B304A614A514B3452384A413D');"
      "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
         META_PROTO_TIMES_VALS(14) ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'T"
         "he WebKit Open Source Project','The WebKit Open Source Project',NULL,"
         "NULL,X'5A5678314E7976364579524D3177494F7236563159552F6E644C553D',X'C2"
         "88101A0A12687474703A2F2F7765626B69742E6F72672F1204504E4758',X'C288101"
         "C0A13687474703A2F2F7765626B69742E6F72672F781205504E473259',NULL,X'222"
         "480000000001000005A5678314E7976364579524D3177494F7236563159552F6E644C"
         "553D',X'222480000000001000005A5678314E7976364579524D3177494F723656315"
         "9552F6E644C553D');"
      "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
         "IL,base_version bigint default -1,server_version bigint default 0,loc"
         "al_external_id bigint default 0,transaction_version bigint default 0,"
         "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
         "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
         "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
         "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
         "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
         "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
         "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
         "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
         "erver_specifics blob,server_unique_position blob,unique_position blob"
         ");"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birt"
         "hday TEXT, db_create_version TEXT, db_create_time INT, next_id INT de"
         "fault -2, cache_guid TEXT, notification_state BLOB, bag_of_chips BLOB"
         ");"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.or"
         "g','c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-13107"
         "8,'9010788312004066376x-6609234393368420856x',NULL,NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion87Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',87);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, tr"
         "ansaction_version BIGINT default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
         "_version bigint default -1,server_version bigint default 0,local_exte"
         "rnal_id bigint default 0,transaction_version bigint default 0,mtime b"
         "igint default 0,server_mtime bigint default 0,ctime bigint default 0,"
         "server_ctime bigint default 0,id varchar(255) default 'r',parent_id v"
         "archar(255) default 'r',server_parent_id varchar(255) default 'r',is_"
         "unsynced bit default 0,is_unapplied_update bit default 0,is_del bit d"
         "efault 0,is_dir bit default 0,server_is_dir bit default 0,server_is_d"
         "el bit default 0,non_unique_name varchar,server_non_unique_name varch"
         "ar(255),unique_server_tag varchar,unique_client_tag varchar,unique_bo"
         "okmark_tag varchar,specifics blob,server_specifics blob,base_server_s"
         "pecifics blob,server_unique_position blob,unique_position blob,attach"
         "ment_metadata blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
         META_PROTO_TIMES_VALS(1)
         ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X''"
         ",X'',X'',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
         META_PROTO_TIMES_VALS(6)
         ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The "
         "Internet','The Internet',NULL,NULL,X'6754307476346749735A5734654D6532"
         "73625336557753582F77673D',X'C2881000',X'C2881000',NULL,X'22247FFFFFFF"
         "FFC000006754307476346749735A5734654D653273625336557753582F77673D',X'2"
         "2247FFFFFFFFFC000006754307476346749735A5734654D653273625336557753582F"
         "77673D',NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
         META_PROTO_TIMES_VALS(7)
         ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome'"
         ",'Google Chrome','google_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'22"
         "00',NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
         META_PROTO_TIMES_VALS(8)
         ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Book"
         "marks','Bookmarks','google_chrome_bookmarks',NULL,X'',X'C2881000',X'C"
         "2881000',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
         META_PROTO_TIMES_VALS(9)
         ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Book"
         "mark Bar','Bookmark Bar','bookmark_bar',NULL,X'',X'C2881000',X'C28810"
         "00',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
         META_PROTO_TIMES_VALS(10)
         ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'Ot"
         "her Bookmarks','Other Bookmarks','other_bookmarks',NULL,X'',X'C288100"
         "0',X'C2881000',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
         META_PROTO_TIMES_VALS(11)
         ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'Home"
         " (The Chromium Projects)','Home (The Chromium Projects)',NULL,NULL,"
         "X'50514C784A456D623579366267644237646A7A2B62314130346E493D',X'C288102"
         "20A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1206414741545741"
         "',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756D2E6F72672F6F746"
         "8657212084146414756415346',NULL,X'22247FFFFFFFFFF0000050514C784A456D6"
         "23579366267644237646A7A2B62314130346E493D',X'22247FFFFFFFFFF000005051"
         "4C784A456D623579366267644237646A7A2B62314130346E493D',NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
         META_PROTO_TIMES_VALS(12)
         ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'Ex"
         "tra Bookmarks','Extra Bookmarks',NULL,NULL,X'7867626A704A646134635A6F"
         "616C376A49513338734B46324837773D',X'C2881000',X'C2881000',NULL,X'2224"
         "80000000000000007867626A704A646134635A6F616C376A49513338734B463248377"
         "73D',X'222480000000000000007867626A704A646134635A6F616C376A4951333873"
         "4B46324837773D',NULL);"
      "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
         META_PROTO_TIMES_VALS(13)
         ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'I"
         "CANN | Internet Corporation for Assigned Names and Numbers','ICANN | "
         "Internet Corporation for Assigned Names and Numbers',NULL,NULL,X'3142"
         "756B572F7741766956504179672B304A614A514B3452384A413D',X'C28810240A156"
         "87474703A2F2F7777772E6963616E6E2E636F6D2F120B504E474158463041414646',"
         "X'C28810200A15687474703A2F2F7777772E6963616E6E2E636F6D2F1207444141464"
         "15346',NULL,X'22247FFFFFFFFFF200003142756B572F7741766956504179672B304"
         "A614A514B3452384A413D',X'22247FFFFFFFFFF200003142756B572F774176695650"
         "4179672B304A614A514B3452384A413D',NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
         META_PROTO_TIMES_VALS(14)
         ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'T"
         "he WebKit Open Source Project','The WebKit Open Source Project',NULL,"
         "NULL,X'5A5678314E7976364579524D3177494F7236563159552F6E644C553D',X'C2"
         "88101A0A12687474703A2F2F7765626B69742E6F72672F1204504E4758',X'C288101"
         "C0A13687474703A2F2F7765626B69742E6F72672F781205504E473259',NULL,X'222"
         "480000000001000005A5678314E7976364579524D3177494F7236563159552F6E644C"
         "553D',X'222480000000001000005A5678314E7976364579524D3177494F723656315"
         "9552F6E644C553D',NULL);"
      "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
         "IL,base_version bigint default -1,server_version bigint default 0,loc"
         "al_external_id bigint default 0,transaction_version bigint default 0,"
         "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
         "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
         "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
         "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
         "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
         "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
         "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
         "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
         "erver_specifics blob,server_unique_position blob,unique_position blob"
         ",attachment_metadata blob);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
         "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defau"
         "lt -2, cache_guid TEXT, notification_state BLOB, bag_of_chips BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
         "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-131078,'"
         "9010788312004066376x-6609234393368420856x',NULL,NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion88Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',88);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB,"
         " transaction_version BIGINT default 0, context BLOB);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1,NULL);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
         "_version bigint default -1,server_version bigint default 0,local_exte"
         "rnal_id bigint default 0,transaction_version bigint default 0,mtime b"
         "igint default 0,server_mtime bigint default 0,ctime bigint default 0,"
         "server_ctime bigint default 0,id varchar(255) default 'r',parent_id v"
         "archar(255) default 'r',server_parent_id varchar(255) default 'r',is_"
         "unsynced bit default 0,is_unapplied_update bit default 0,is_del bit d"
         "efault 0,is_dir bit default 0,server_is_dir bit default 0,server_is_d"
         "el bit default 0,non_unique_name varchar,server_non_unique_name varch"
         "ar(255),unique_server_tag varchar,unique_client_tag varchar,unique_bo"
         "okmark_tag varchar,specifics blob,server_specifics blob,base_server_s"
         "pecifics blob,server_unique_position blob,unique_position blob,attach"
         "ment_metadata blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
         META_PROTO_TIMES_VALS(1)
         ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X''"
         ",X'',X'',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
         META_PROTO_TIMES_VALS(6)
         ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The "
         "Internet','The Internet',NULL,NULL,X'6754307476346749735A5734654D6532"
         "73625336557753582F77673D',X'C2881000',X'C2881000',NULL,X'22247FFFFFFF"
         "FFC000006754307476346749735A5734654D653273625336557753582F77673D',X'2"
         "2247FFFFFFFFFC000006754307476346749735A5734654D653273625336557753582F"
         "77673D',NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
         META_PROTO_TIMES_VALS(7)
         ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome'"
         ",'Google Chrome','google_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'22"
         "00',NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
         META_PROTO_TIMES_VALS(8)
         ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Book"
         "marks','Bookmarks','google_chrome_bookmarks',NULL,X'',X'C2881000',X'C"
         "2881000',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
         META_PROTO_TIMES_VALS(9)
         ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Book"
         "mark Bar','Bookmark Bar','bookmark_bar',NULL,X'',X'C2881000',X'C28810"
         "00',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
         META_PROTO_TIMES_VALS(10)
         ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'Ot"
         "her Bookmarks','Other Bookmarks','other_bookmarks',NULL,X'',X'C288100"
         "0',X'C2881000',NULL,X'2200',X'2200',NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
         META_PROTO_TIMES_VALS(11)
         ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'Home"
         " (The Chromium Projects)','Home (The Chromium Projects)',NULL,NULL,"
         "X'50514C784A456D623579366267644237646A7A2B62314130346E493D',X'C288102"
         "20A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1206414741545741"
         "',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756D2E6F72672F6F746"
         "8657212084146414756415346',NULL,X'22247FFFFFFFFFF0000050514C784A456D6"
         "23579366267644237646A7A2B62314130346E493D',X'22247FFFFFFFFFF000005051"
         "4C784A456D623579366267644237646A7A2B62314130346E493D',NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
         META_PROTO_TIMES_VALS(12)
         ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'Ex"
         "tra Bookmarks','Extra Bookmarks',NULL,NULL,X'7867626A704A646134635A6F"
         "616C376A49513338734B46324837773D',X'C2881000',X'C2881000',NULL,X'2224"
         "80000000000000007867626A704A646134635A6F616C376A49513338734B463248377"
         "73D',X'222480000000000000007867626A704A646134635A6F616C376A4951333873"
         "4B46324837773D',NULL);"
      "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
         META_PROTO_TIMES_VALS(13)
         ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'I"
         "CANN | Internet Corporation for Assigned Names and Numbers','ICANN | "
         "Internet Corporation for Assigned Names and Numbers',NULL,NULL,X'3142"
         "756B572F7741766956504179672B304A614A514B3452384A413D',X'C28810240A156"
         "87474703A2F2F7777772E6963616E6E2E636F6D2F120B504E474158463041414646',"
         "X'C28810200A15687474703A2F2F7777772E6963616E6E2E636F6D2F1207444141464"
         "15346',NULL,X'22247FFFFFFFFFF200003142756B572F7741766956504179672B304"
         "A614A514B3452384A413D',X'22247FFFFFFFFFF200003142756B572F774176695650"
         "4179672B304A614A514B3452384A413D',NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
         META_PROTO_TIMES_VALS(14)
         ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'T"
         "he WebKit Open Source Project','The WebKit Open Source Project',NULL,"
         "NULL,X'5A5678314E7976364579524D3177494F7236563159552F6E644C553D',X'C2"
         "88101A0A12687474703A2F2F7765626B69742E6F72672F1204504E4758',X'C288101"
         "C0A13687474703A2F2F7765626B69742E6F72672F781205504E473259',NULL,X'222"
         "480000000001000005A5678314E7976364579524D3177494F7236563159552F6E644C"
         "553D',X'222480000000001000005A5678314E7976364579524D3177494F723656315"
         "9552F6E644C553D',NULL);"
      "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
         "IL,base_version bigint default -1,server_version bigint default 0,loc"
         "al_external_id bigint default 0,transaction_version bigint default 0,"
         "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
         "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
         "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
         "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
         "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
         "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
         "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
         "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
         "erver_specifics blob,server_unique_position blob,unique_position blob"
         ",attachment_metadata blob);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
         "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defau"
         "lt -2, cache_guid TEXT, notification_state BLOB, bag_of_chips BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
         "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-131078,'"
         "9010788312004066376x-6609234393368420856x',NULL,NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion89Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',89);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, tr"
         "ansaction_version BIGINT default 0, context BLOB);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1,NULL);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
         "_version bigint default -1,server_version bigint default 0,local_exte"
         "rnal_id bigint default 0,transaction_version bigint default 0,mtime b"
         "igint default 0,server_mtime bigint default 0,ctime bigint default 0,"
         "server_ctime bigint default 0,id varchar(255) default 'r',parent_id v"
         "archar(255) default 'r',server_parent_id varchar(255) default 'r',is_"
         "unsynced bit default 0,is_unapplied_update bit default 0,is_del bit d"
         "efault 0,is_dir bit default 0,server_is_dir bit default 0,server_is_d"
         "el bit default 0,non_unique_name varchar,server_non_unique_name varch"
         "ar(255),unique_server_tag varchar,unique_client_tag varchar,unique_bo"
         "okmark_tag varchar,specifics blob,server_specifics blob,base_server_s"
         "pecifics blob,server_unique_position blob,unique_position blob,attach"
         "ment_metadata blob,server_attachment_metadata blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
         META_PROTO_TIMES_VALS(1)
         ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',X'',NULL,X'2200'"
         ",X'2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
         META_PROTO_TIMES_VALS(6)
         ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The Internet','The Internet'"
         ",NULL,NULL,X'6754307476346749735A5734654D653273625336557753582F77673D"
         "',X'C2881000',X'C2881000',NULL,X'22247FFFFFFFFFC000006754307476346749"
         "735A5734654D653273625336557753582F77673D',X'22247FFFFFFFFFC0000067543"
         "07476346749735A5734654D653273625336557753582F77673D',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
         META_PROTO_TIMES_VALS(7)
         ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google"
         "_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
         META_PROTO_TIMES_VALS(8)
         ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Bookmarks','Bookmarks','goog"
         "le_chrome_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'"
         "2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
         META_PROTO_TIMES_VALS(9)
         ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar'"
         ",'bookmark_bar',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'2200'"
         ",NULL,NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
         META_PROTO_TIMES_VALS(10)
         ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'Other Bookmarks','Other Boo"
         "kmarks','other_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'220"
         "0',X'2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
         META_PROTO_TIMES_VALS(11)
         ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'Home (The Chromium Projects"
         ")','Home (The Chromium Projects)',NULL,NULL,X'50514C784A456D623579366"
         "267644237646A7A2B62314130346E493D',X'C28810220A18687474703A2F2F646576"
         "2E6368726F6D69756D2E6F72672F1206414741545741',X'C28810290A1D687474703"
         "A2F2F6465762E6368726F6D69756D2E6F72672F6F7468657212084146414756415346"
         "',NULL,X'22247FFFFFFFFFF0000050514C784A456D623579366267644237646A7A2B"
         "62314130346E493D',X'22247FFFFFFFFFF0000050514C784A456D623579366267644"
         "237646A7A2B62314130346E493D',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
         META_PROTO_TIMES_VALS(12)
         ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'Extra Bookmarks','Extra Boo"
         "kmarks',NULL,NULL,X'7867626A704A646134635A6F616C376A49513338734B46324"
         "837773D',X'C2881000',X'C2881000',NULL,X'222480000000000000007867626A7"
         "04A646134635A6F616C376A49513338734B46324837773D',X'222480000000000000"
         "007867626A704A646134635A6F616C376A49513338734B46324837773D',NULL,NULL"
         ");"
      "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
         META_PROTO_TIMES_VALS(13)
         ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'ICANN | Internet Corporatio"
         "n for Assigned Names and Numbers','ICANN | Internet Corporation for A"
         "ssigned Names and Numbers',NULL,NULL,X'3142756B572F774176695650417967"
         "2B304A614A514B3452384A413D',X'C28810240A15687474703A2F2F7777772E69636"
         "16E6E2E636F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F"
         "2F7777772E6963616E6E2E636F6D2F120744414146415346',NULL,X'22247FFFFFFF"
         "FFF200003142756B572F7741766956504179672B304A614A514B3452384A413D',X'2"
         "2247FFFFFFFFFF200003142756B572F7741766956504179672B304A614A514B345238"
         "4A413D',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
         META_PROTO_TIMES_VALS(14)
         ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'The WebKit Open Source Proj"
         "ect','The WebKit Open Source Project',NULL,NULL,X'5A5678314E797636457"
         "9524D3177494F7236563159552F6E644C553D',X'C288101A0A12687474703A2F2F77"
         "65626B69742E6F72672F1204504E4758',X'C288101C0A13687474703A2F2F7765626"
         "B69742E6F72672F781205504E473259',NULL,X'222480000000001000005A5678314"
         "E7976364579524D3177494F7236563159552F6E644C553D',X'222480000000001000"
         "005A5678314E7976364579524D3177494F7236563159552F6E644C553D',NULL,NULL"
         ");"
      "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
         "IL,base_version bigint default -1,server_version bigint default 0,loc"
         "al_external_id bigint default 0,transaction_version bigint default 0,"
         "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
         "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
         "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
         "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
         "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
         "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
         "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
         "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
         "erver_specifics blob,server_unique_position blob,unique_position blob"
         ",attachment_metadata blob,server_attachment_metadata blob);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
         "y TEXT, db_create_version TEXT, db_create_time INT, next_id INT defau"
         "lt -2, cache_guid TEXT, notification_state BLOB, bag_of_chips BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
         "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-131078,'"
         "9010788312004066376x-6609234393368420856x',NULL,NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion90Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  ASSERT_TRUE(connection->Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',90);"
      "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, tr"
         "ansaction_version BIGINT default 0, context BLOB);"
      "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1,NULL);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
         "_version bigint default -1,server_version bigint default 0,local_exte"
         "rnal_id bigint default 0,transaction_version bigint default 0,mtime b"
         "igint default 0,server_mtime bigint default 0,ctime bigint default 0,"
         "server_ctime bigint default 0,id varchar(255) default 'r',parent_id v"
         "archar(255) default 'r',server_parent_id varchar(255) default 'r',is_"
         "unsynced bit default 0,is_unapplied_update bit default 0,is_del bit d"
         "efault 0,is_dir bit default 0,server_is_dir bit default 0,server_is_d"
         "el bit default 0,non_unique_name varchar,server_non_unique_name varch"
         "ar(255),unique_server_tag varchar,unique_client_tag varchar,unique_bo"
         "okmark_tag varchar,specifics blob,server_specifics blob,base_server_s"
         "pecifics blob,server_unique_position blob,unique_position blob,attach"
         "ment_metadata blob,server_attachment_metadata blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
         META_PROTO_TIMES_VALS(1)
         ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',X'',NULL,X'2200'"
         ",X'2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
         META_PROTO_TIMES_VALS(6)
         ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The Internet','The Internet'"
         ",NULL,NULL,X'6754307476346749735A5734654D653273625336557753582F77673D"
         "',X'C2881000',X'C2881000',NULL,X'22247FFFFFFFFFC000006754307476346749"
         "735A5734654D653273625336557753582F77673D',X'22247FFFFFFFFFC0000067543"
         "07476346749735A5734654D653273625336557753582F77673D',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
         META_PROTO_TIMES_VALS(7)
         ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google"
         "_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
         META_PROTO_TIMES_VALS(8)
         ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Bookmarks','Bookmarks','goog"
         "le_chrome_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'"
         "2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
         META_PROTO_TIMES_VALS(9)
         ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar'"
         ",'bookmark_bar',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'2200'"
         ",NULL,NULL);"
      "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
         META_PROTO_TIMES_VALS(10)
         ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'Other Bookmarks','Other Boo"
         "kmarks','other_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'220"
         "0',X'2200',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
         META_PROTO_TIMES_VALS(11)
         ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'Home (The Chromium Projects"
         ")','Home (The Chromium Projects)',NULL,NULL,X'50514C784A456D623579366"
         "267644237646A7A2B62314130346E493D',X'C28810220A18687474703A2F2F646576"
         "2E6368726F6D69756D2E6F72672F1206414741545741',X'C28810290A1D687474703"
         "A2F2F6465762E6368726F6D69756D2E6F72672F6F7468657212084146414756415346"
         "',NULL,X'22247FFFFFFFFFF0000050514C784A456D623579366267644237646A7A2B"
         "62314130346E493D',X'22247FFFFFFFFFF0000050514C784A456D623579366267644"
         "237646A7A2B62314130346E493D',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
         META_PROTO_TIMES_VALS(12)
         ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'Extra Bookmarks','Extra Boo"
         "kmarks',NULL,NULL,X'7867626A704A646134635A6F616C376A49513338734B46324"
         "837773D',X'C2881000',X'C2881000',NULL,X'222480000000000000007867626A7"
         "04A646134635A6F616C376A49513338734B46324837773D',X'222480000000000000"
         "007867626A704A646134635A6F616C376A49513338734B46324837773D',NULL,NULL"
         ");"
      "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
         META_PROTO_TIMES_VALS(13)
         ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'ICANN | Internet Corporatio"
         "n for Assigned Names and Numbers','ICANN | Internet Corporation for A"
         "ssigned Names and Numbers',NULL,NULL,X'3142756B572F774176695650417967"
         "2B304A614A514B3452384A413D',X'C28810240A15687474703A2F2F7777772E69636"
         "16E6E2E636F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F"
         "2F7777772E6963616E6E2E636F6D2F120744414146415346',NULL,X'22247FFFFFFF"
         "FFF200003142756B572F7741766956504179672B304A614A514B3452384A413D',X'2"
         "2247FFFFFFFFFF200003142756B572F7741766956504179672B304A614A514B345238"
         "4A413D',NULL,NULL);"
      "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
         META_PROTO_TIMES_VALS(14)
         ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'The WebKit Open Source Proj"
         "ect','The WebKit Open Source Project',NULL,NULL,X'5A5678314E797636457"
         "9524D3177494F7236563159552F6E644C553D',X'C288101A0A12687474703A2F2F77"
         "65626B69742E6F72672F1204504E4758',X'C288101C0A13687474703A2F2F7765626"
         "B69742E6F72672F781205504E473259',NULL,X'222480000000001000005A5678314"
         "E7976364579524D3177494F7236563159552F6E644C553D',X'222480000000001000"
         "005A5678314E7976364579524D3177494F7236563159552F6E644C553D',NULL,NULL"
         ");"
      "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
         "IL,base_version bigint default -1,server_version bigint default 0,loc"
         "al_external_id bigint default 0,transaction_version bigint default 0,"
         "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
         "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
         "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
         "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
         "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
         "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
         "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
         "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
         "erver_specifics blob,server_unique_position blob,unique_position blob"
         ",attachment_metadata blob,server_attachment_metadata blob);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
         "y TEXT, cache_guid TEXT, bag_of_chips BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
         "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','9010788312004066376x-66092343"
         "93368420856x',NULL);"));
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion91Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  // clang-format mangles the below query by indenting each time the
  // META_PROTO_TIMES_VALS macro is invoked, making it unreadable.
  // clang-format off
  ASSERT_TRUE(connection->Execute(
    "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
    "INSERT INTO 'share_version' VALUES('nick@chromium.org',91);"
    "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, tr"
       "ansaction_version BIGINT default 0, context BLOB);"
    "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1,NULL);"
    "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
       "_version bigint default -1,server_version bigint default 0,local_exte"
       "rnal_id bigint default 0,transaction_version bigint default 0,mtime b"
       "igint default 0,server_mtime bigint default 0,ctime bigint default 0,"
       "server_ctime bigint default 0,id varchar(255) default 'r',parent_id v"
       "archar(255) default 'r',server_parent_id varchar(255) default 'r',is_"
       "unsynced bit default 0,is_unapplied_update bit default 0,is_del bit d"
       "efault 0,is_dir bit default 0,server_is_dir bit default 0,server_is_d"
       "el bit default 0,non_unique_name varchar,server_non_unique_name varch"
       "ar(255),unique_server_tag varchar,unique_client_tag varchar,unique_bo"
       "okmark_tag varchar,specifics blob,server_specifics blob,base_server_s"
       "pecifics blob,server_unique_position blob,unique_position blob,attach"
       "ment_metadata blob,server_attachment_metadata blob);"
    "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
       META_PROTO_TIMES_VALS(1)
       ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',X'',NULL,X'2200'"
       ",X'2200',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
       META_PROTO_TIMES_VALS(6)
       ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The Internet','The Internet'"
       ",NULL,NULL,X'6754307476346749735A5734654D653273625336557753582F77673D"
       "',X'C2881000',X'C2881000',NULL,X'22247FFFFFFFFFC000006754307476346749"
       "735A5734654D653273625336557753582F77673D',X'22247FFFFFFFFFC0000067543"
       "07476346749735A5734654D653273625336557753582F77673D',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
       META_PROTO_TIMES_VALS(7)
       ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google"
       "_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'2200',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
       META_PROTO_TIMES_VALS(8)
       ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Bookmarks','Bookmarks','goog"
       "le_chrome_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'"
       "2200',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
       META_PROTO_TIMES_VALS(9)
       ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar'"
       ",'bookmark_bar',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'2200'"
       ",NULL,NULL);"
    "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
       META_PROTO_TIMES_VALS(10)
       ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'Other Bookmarks','Other Boo"
       "kmarks','other_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'220"
       "0',X'2200',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
       META_PROTO_TIMES_VALS(11)
       ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'Home (The Chromium Projects"
       ")','Home (The Chromium Projects)',NULL,NULL,X'50514C784A456D623579366"
       "267644237646A7A2B62314130346E493D',X'C28810220A18687474703A2F2F646576"
       "2E6368726F6D69756D2E6F72672F1206414741545741',X'C28810290A1D687474703"
       "A2F2F6465762E6368726F6D69756D2E6F72672F6F7468657212084146414756415346"
       "',NULL,X'22247FFFFFFFFFF0000050514C784A456D623579366267644237646A7A2B"
       "62314130346E493D',X'22247FFFFFFFFFF0000050514C784A456D623579366267644"
       "237646A7A2B62314130346E493D',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
       META_PROTO_TIMES_VALS(12)
       ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'Extra Bookmarks','Extra Boo"
       "kmarks',NULL,NULL,X'7867626A704A646134635A6F616C376A49513338734B46324"
       "837773D',X'C2881000',X'C2881000',NULL,X'222480000000000000007867626A7"
       "04A646134635A6F616C376A49513338734B46324837773D',X'222480000000000000"
       "007867626A704A646134635A6F616C376A49513338734B46324837773D',NULL,NULL"
       ");"
    "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
       META_PROTO_TIMES_VALS(13)
       ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'ICANN | Internet Corporatio"
       "n for Assigned Names and Numbers','ICANN | Internet Corporation for A"
       "ssigned Names and Numbers',NULL,NULL,X'3142756B572F774176695650417967"
       "2B304A614A514B3452384A413D',X'C28810240A15687474703A2F2F7777772E69636"
       "16E6E2E636F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F"
       "2F7777772E6963616E6E2E636F6D2F120744414146415346',NULL,X'22247FFFFFFF"
       "FFF200003142756B572F7741766956504179672B304A614A514B3452384A413D',X'2"
       "2247FFFFFFFFFF200003142756B572F7741766956504179672B304A614A514B345238"
       "4A413D',NULL,NULL);"
    "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
       META_PROTO_TIMES_VALS(14)
       ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'The WebKit Open Source Proj"
       "ect','The WebKit Open Source Project',NULL,NULL,X'5A5678314E797636457"
       "9524D3177494F7236563159552F6E644C553D',X'C288101A0A12687474703A2F2F77"
       "65626B69742E6F72672F1204504E4758',X'C288101C0A13687474703A2F2F7765626"
       "B69742E6F72672F781205504E473259',NULL,X'222480000000001000005A5678314"
       "E7976364579524D3177494F7236563159552F6E644C553D',X'222480000000001000"
       "005A5678314E7976364579524D3177494F7236563159552F6E644C553D',NULL,NULL"
       ");"
    "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
       "IL,base_version bigint default -1,server_version bigint default 0,loc"
       "al_external_id bigint default 0,transaction_version bigint default 0,"
       "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
       "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
       "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
       "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
       "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
       "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
       "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
       "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
       "erver_specifics blob,server_unique_position blob,unique_position blob"
       ",attachment_metadata blob,server_attachment_metadata blob);"
    "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
       "y TEXT, cache_guid TEXT, bag_of_chips BLOB);"
    "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
       "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','9010788312004066376x-66092343"
       "93368420856x',NULL);"));
  // clang-format on
  ASSERT_TRUE(connection->CommitTransaction());
}

void MigrationTest::SetUpVersion92Database(sql::Database* connection) {
  ASSERT_TRUE(connection->is_open());
  ASSERT_TRUE(connection->BeginTransaction());
  // clang-format mangles the below query by indenting each time the
  // META_PROTO_TIMES_VALS macro is invoked, making it unreadable.
  // clang-format off
  ASSERT_TRUE(connection->Execute(
    "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
    "INSERT INTO 'share_version' VALUES('nick@chromium.org',92);"
    "CREATE TABLE models (model_id BLOB primary key, progress_marker BLOB, tr"
       "ansaction_version BIGINT default 0, context BLOB);"
    "INSERT INTO 'models' VALUES(X'C2881000',X'0888810218B605',1,NULL);"
    "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,base"
       "_version bigint default -1,server_version bigint default 0,local_exte"
       "rnal_id bigint default 0,transaction_version bigint default 0,mtime b"
       "igint default 0,server_mtime bigint default 0,ctime bigint default 0,"
       "server_ctime bigint default 0,id varchar(255) default 'r',parent_id v"
       "archar(255) default 'r',server_parent_id varchar(255) default 'r',is_"
       "unsynced bit default 0,is_unapplied_update bit default 0,is_del bit d"
       "efault 0,is_dir bit default 0,server_is_dir bit default 0,server_is_d"
       "el bit default 0,non_unique_name varchar,server_non_unique_name varch"
       "ar(255),unique_server_tag varchar,unique_client_tag varchar,unique_bo"
       "okmark_tag varchar,specifics blob,server_specifics blob,base_server_s"
       "pecifics blob,server_unique_position blob,unique_position blob);"
    "INSERT INTO 'metas' VALUES(1,-1,0,0,0,"
       META_PROTO_TIMES_VALS(1)
       ",'r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'',X'',NULL,X'2200'"
       ",X'2200');"
    "INSERT INTO 'metas' VALUES(6,694,694,6,0,"
       META_PROTO_TIMES_VALS(6)
       ",'s_ID_6','s_ID_9','s_ID_9',0,0,0,1,1,0,'The Internet','The Internet'"
       ",NULL,NULL,X'6754307476346749735A5734654D653273625336557753582F77673D"
       "',X'C2881000',X'C2881000',NULL,X'22247FFFFFFFFFC000006754307476346749"
       "735A5734654D653273625336557753582F77673D',X'22247FFFFFFFFFC0000067543"
       "07476346749735A5734654D653273625336557753582F77673D');"
    "INSERT INTO 'metas' VALUES(7,663,663,0,0,"
       META_PROTO_TIMES_VALS(7)
       ",'s_ID_7','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google"
       "_chrome',NULL,X'',NULL,NULL,NULL,X'2200',X'2200');"
    "INSERT INTO 'metas' VALUES(8,664,664,0,0,"
       META_PROTO_TIMES_VALS(8)
       ",'s_ID_8','s_ID_7','s_ID_7',0,0,0,1,1,0,'Bookmarks','Bookmarks','goog"
       "le_chrome_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'"
       "2200');"
    "INSERT INTO 'metas' VALUES(9,665,665,1,0,"
       META_PROTO_TIMES_VALS(9)
       ",'s_ID_9','s_ID_8','s_ID_8',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar'"
       ",'bookmark_bar',NULL,X'',X'C2881000',X'C2881000',NULL,X'2200',X'2200'"
       ");"
    "INSERT INTO 'metas' VALUES(10,666,666,2,0,"
       META_PROTO_TIMES_VALS(10)
       ",'s_ID_10','s_ID_8','s_ID_8',0,0,0,1,1,0,'Other Bookmarks','Other Boo"
       "kmarks','other_bookmarks',NULL,X'',X'C2881000',X'C2881000',NULL,X'220"
       "0',X'2200');"
    "INSERT INTO 'metas' VALUES(11,683,683,8,0,"
       META_PROTO_TIMES_VALS(11)
       ",'s_ID_11','s_ID_6','s_ID_6',0,0,0,0,0,0,'Home (The Chromium Projects"
       ")','Home (The Chromium Projects)',NULL,NULL,X'50514C784A456D623579366"
       "267644237646A7A2B62314130346E493D',X'C28810220A18687474703A2F2F646576"
       "2E6368726F6D69756D2E6F72672F1206414741545741',X'C28810290A1D687474703"
       "A2F2F6465762E6368726F6D69756D2E6F72672F6F7468657212084146414756415346"
       "',NULL,X'22247FFFFFFFFFF0000050514C784A456D623579366267644237646A7A2B"
       "62314130346E493D',X'22247FFFFFFFFFF0000050514C784A456D623579366267644"
       "237646A7A2B62314130346E493D');"
    "INSERT INTO 'metas' VALUES(12,685,685,9,0,"
       META_PROTO_TIMES_VALS(12)
       ",'s_ID_12','s_ID_6','s_ID_6',0,0,0,1,1,0,'Extra Bookmarks','Extra Boo"
       "kmarks',NULL,NULL,X'7867626A704A646134635A6F616C376A49513338734B46324"
       "837773D',X'C2881000',X'C2881000',NULL,X'222480000000000000007867626A7"
       "04A646134635A6F616C376A49513338734B46324837773D',X'222480000000000000"
       "007867626A704A646134635A6F616C376A49513338734B46324837773D');"
    "INSERT INTO 'metas' VALUES(13,687,687,10,0,"
       META_PROTO_TIMES_VALS(13)
       ",'s_ID_13','s_ID_6','s_ID_6',0,0,0,0,0,0,'ICANN | Internet Corporatio"
       "n for Assigned Names and Numbers','ICANN | Internet Corporation for A"
       "ssigned Names and Numbers',NULL,NULL,X'3142756B572F774176695650417967"
       "2B304A614A514B3452384A413D',X'C28810240A15687474703A2F2F7777772E69636"
       "16E6E2E636F6D2F120B504E474158463041414646',X'C28810200A15687474703A2F"
       "2F7777772E6963616E6E2E636F6D2F120744414146415346',NULL,X'22247FFFFFFF"
       "FFF200003142756B572F7741766956504179672B304A614A514B3452384A413D',X'2"
       "2247FFFFFFFFFF200003142756B572F7741766956504179672B304A614A514B345238"
       "4A413D');"
    "INSERT INTO 'metas' VALUES(14,692,692,11,0,"
       META_PROTO_TIMES_VALS(14)
       ",'s_ID_14','s_ID_6','s_ID_6',0,0,0,0,0,0,'The WebKit Open Source Proj"
       "ect','The WebKit Open Source Project',NULL,NULL,X'5A5678314E797636457"
       "9524D3177494F7236563159552F6E644C553D',X'C288101A0A12687474703A2F2F77"
       "65626B69742E6F72672F1204504E4758',X'C288101C0A13687474703A2F2F7765626"
       "B69742E6F72672F781205504E473259',NULL,X'222480000000001000005A5678314"
       "E7976364579524D3177494F7236563159552F6E644C553D',X'222480000000001000"
       "005A5678314E7976364579524D3177494F7236563159552F6E644C553D');"
    "CREATE TABLE deleted_metas (metahandle bigint primary key ON CONFLICT FA"
       "IL,base_version bigint default -1,server_version bigint default 0,loc"
       "al_external_id bigint default 0,transaction_version bigint default 0,"
       "mtime bigint default 0,server_mtime bigint default 0,ctime bigint def"
       "ault 0,server_ctime bigint default 0,id varchar(255) default 'r',pare"
       "nt_id varchar(255) default 'r',server_parent_id varchar(255) default "
       "'r',is_unsynced bit default 0,is_unapplied_update bit default 0,is_de"
       "l bit default 0,is_dir bit default 0,server_is_dir bit default 0,serv"
       "er_is_del bit default 0,non_unique_name varchar,server_non_unique_nam"
       "e varchar(255),unique_server_tag varchar,unique_client_tag varchar,un"
       "ique_bookmark_tag varchar,specifics blob,server_specifics blob,base_s"
       "erver_specifics blob,server_unique_position blob,unique_position blob"
       ");"
    "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthda"
       "y TEXT, cache_guid TEXT, bag_of_chips BLOB);"
    "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
       "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','9010788312004066376x-66092343"
       "93368420856x',NULL);"));
  // clang-format on
  ASSERT_TRUE(connection->CommitTransaction());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion67To68) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());

  SetUpVersion67Database(&connection);

  // Columns existing before version 67.
  ASSERT_TRUE(connection.DoesColumnExist("metas", "name"));
  ASSERT_TRUE(connection.DoesColumnExist("metas", "unsanitized_name"));
  ASSERT_TRUE(connection.DoesColumnExist("metas", "server_name"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);

  ASSERT_FALSE(dbs.needs_column_refresh());
  EXPECT_TRUE(dbs.MigrateVersion67To68());
  EXPECT_EQ(68, dbs.GetVersion());
  EXPECT_TRUE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion68To69) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion68Database(&connection);

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion68To69());
    EXPECT_EQ(69, dbs.GetVersion());
    EXPECT_TRUE(dbs.needs_column_refresh());
  }

  EXPECT_TRUE(connection.DoesColumnExist("metas", "specifics"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "server_specifics"));
  sql::Statement s(connection.GetUniqueStatement("SELECT non_unique_name,"
      "is_del, is_dir, id, specifics, server_specifics FROM metas "
      "WHERE metahandle = 2"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ("Deleted Item", s.ColumnString(0));
  EXPECT_TRUE(s.ColumnBool(1));
  EXPECT_FALSE(s.ColumnBool(2));
  EXPECT_EQ("s_ID_2", s.ColumnString(3));
  sync_pb::EntitySpecifics specifics;
  specifics.ParseFromArray(s.ColumnBlob(4), s.ColumnByteLength(4));
  ASSERT_TRUE(specifics.has_bookmark());
  EXPECT_EQ("http://www.google.com/", specifics.bookmark().url());
  EXPECT_EQ("AASGASGA", specifics.bookmark().favicon());
  specifics.ParseFromArray(s.ColumnBlob(5), s.ColumnByteLength(5));
  ASSERT_TRUE(specifics.has_bookmark());
  EXPECT_EQ("http://www.google.com/2", specifics.bookmark().url());
  EXPECT_EQ("ASADGADGADG", specifics.bookmark().favicon());
  EXPECT_FALSE(s.Step());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion69To70) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion69Database(&connection);

  ASSERT_TRUE(connection.DoesColumnExist("metas", "singleton_tag"));
  ASSERT_FALSE(connection.DoesColumnExist("metas", "unique_server_tag"));
  ASSERT_FALSE(connection.DoesColumnExist("metas", "unique_client_tag"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion69To70());
    EXPECT_EQ(70, dbs.GetVersion());
    EXPECT_TRUE(dbs.needs_column_refresh());
  }

  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_server_tag"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_client_tag"));
  sql::Statement s(connection.GetUniqueStatement("SELECT id"
      " FROM metas WHERE unique_server_tag = 'google_chrome'"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ("s_ID_7", s.ColumnString(0));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion70To71) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion70Database(&connection);

  ASSERT_TRUE(connection.DoesColumnExist("share_info", "last_sync_timestamp"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info", "initial_sync_ended"));
  ASSERT_FALSE(connection.DoesTableExist("models"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion70To71());
    EXPECT_EQ(71, dbs.GetVersion());
    EXPECT_FALSE(dbs.needs_column_refresh());
  }

  EXPECT_FALSE(connection.DoesColumnExist("share_info", "last_sync_timestamp"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "initial_sync_ended"));
  EXPECT_TRUE(connection.DoesTableExist("models"));
  EXPECT_TRUE(connection.DoesColumnExist("models", "initial_sync_ended"));
  EXPECT_TRUE(connection.DoesColumnExist("models", "last_download_timestamp"));
  EXPECT_TRUE(connection.DoesColumnExist("models", "model_id"));

  sql::Statement s(connection.GetUniqueStatement("SELECT model_id, "
      "initial_sync_ended, last_download_timestamp FROM models"));
  ASSERT_TRUE(s.Step());
  std::string model_id = s.ColumnString(0);
  EXPECT_EQ("C2881000", base::HexEncode(model_id.data(), model_id.size()))
      << "Model ID is expected to be the empty BookmarkSpecifics proto.";
  EXPECT_TRUE(s.ColumnBool(1));
  EXPECT_EQ(694, s.ColumnInt64(2));
  EXPECT_FALSE(s.Step());
}


TEST_F(DirectoryBackingStoreTest, MigrateVersion71To72) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion71Database(&connection);

  ASSERT_TRUE(connection.DoesTableExist("extended_attributes"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion71To72());
    EXPECT_EQ(72, dbs.GetVersion());
    EXPECT_FALSE(dbs.needs_column_refresh());
  }

  EXPECT_FALSE(connection.DoesTableExist("extended_attributes"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion72To73) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion72Database(&connection);

  ASSERT_FALSE(connection.DoesColumnExist("share_info", "notification_state"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion72To73());
    EXPECT_EQ(73, dbs.GetVersion());
    EXPECT_FALSE(dbs.needs_column_refresh());
  }

  EXPECT_TRUE(connection.DoesColumnExist("share_info", "notification_state"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion73To74) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion73Database(&connection);

  ASSERT_FALSE(
      connection.DoesColumnExist("share_info", "autofill_migration_state"));
  ASSERT_FALSE(
      connection.DoesColumnExist("share_info",
          "bookmarks_added_during_autofill_migration"));
  ASSERT_FALSE(
      connection.DoesColumnExist("share_info", "autofill_migration_time"));
  ASSERT_FALSE(
      connection.DoesColumnExist("share_info",
          "autofill_entries_added_during_migration"));

  ASSERT_FALSE(
      connection.DoesColumnExist("share_info",
          "autofill_profiles_added_during_migration"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion73To74());
    EXPECT_EQ(74, dbs.GetVersion());
    EXPECT_FALSE(dbs.needs_column_refresh());
  }

  EXPECT_TRUE(
      connection.DoesColumnExist("share_info", "autofill_migration_state"));
  EXPECT_TRUE(connection.DoesColumnExist(
      "share_info", "bookmarks_added_during_autofill_migration"));
  EXPECT_TRUE(
      connection.DoesColumnExist("share_info", "autofill_migration_time"));
  EXPECT_TRUE(connection.DoesColumnExist(
      "share_info", "autofill_entries_added_during_migration"));

  EXPECT_TRUE(connection.DoesColumnExist(
      "share_info", "autofill_profiles_added_during_migration"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion74To75) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion74Database(&connection);

  ASSERT_FALSE(connection.DoesColumnExist("models", "progress_marker"));
  ASSERT_TRUE(connection.DoesColumnExist("models", "last_download_timestamp"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);

    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion74To75());
    EXPECT_EQ(75, dbs.GetVersion());
    EXPECT_FALSE(dbs.needs_column_refresh());
  }

  EXPECT_TRUE(connection.DoesColumnExist("models", "progress_marker"));
  EXPECT_FALSE(connection.DoesColumnExist("models", "last_download_timestamp"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion75To76) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion75Database(&connection);

  ASSERT_TRUE(
      connection.DoesColumnExist("share_info", "autofill_migration_state"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info",
      "bookmarks_added_during_autofill_migration"));
  ASSERT_TRUE(
      connection.DoesColumnExist("share_info", "autofill_migration_time"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info",
      "autofill_entries_added_during_migration"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info",
      "autofill_profiles_added_during_migration"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_FALSE(dbs.needs_column_refresh());
  EXPECT_TRUE(dbs.MigrateVersion75To76());
  EXPECT_EQ(76, dbs.GetVersion());
  EXPECT_TRUE(dbs.needs_column_refresh());
  // Cannot actual refresh columns due to version 76 not containing all
  // necessary columns.
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion76To77) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion76Database(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_FALSE(dbs.needs_column_refresh());

  ASSERT_EQ(GetExpectedLegacyMetaProtoTimes(INCLUDE_DELETED_ITEMS),
            GetMetaProtoTimes(&connection));
  // Since the proto times are expected to be in a legacy format, they may not
  // be compatible with ProtoTimeToTime, so we don't call ExpectTimes().

  EXPECT_TRUE(dbs.MigrateVersion76To77());
  EXPECT_EQ(77, dbs.GetVersion());

  EXPECT_EQ(GetExpectedMetaProtoTimes(INCLUDE_DELETED_ITEMS),
            GetMetaProtoTimes(&connection));
  // Cannot actually load entries due to version 77 not having all required
  // columns.
  EXPECT_FALSE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion77To78) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion77Database(&connection);

  ASSERT_FALSE(connection.DoesColumnExist("metas", "BASE_SERVER_SPECIFICS"));

  {
    TestDirectoryBackingStore dbs(GetUsername(), &connection);
    ASSERT_FALSE(dbs.needs_column_refresh());
    EXPECT_TRUE(dbs.MigrateVersion77To78());
    EXPECT_EQ(78, dbs.GetVersion());

    EXPECT_FALSE(dbs.needs_column_refresh());
  }

  EXPECT_TRUE(connection.DoesColumnExist("metas", "base_server_specifics"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion78To79) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion78Database(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_FALSE(dbs.needs_column_refresh());
  EXPECT_TRUE(dbs.MigrateVersion78To79());
  EXPECT_EQ(79, dbs.GetVersion());
  EXPECT_FALSE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion79To80) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion79Database(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_FALSE(dbs.needs_column_refresh());
  EXPECT_TRUE(dbs.MigrateVersion79To80());
  EXPECT_EQ(80, dbs.GetVersion());
  EXPECT_FALSE(dbs.needs_column_refresh());

  // Ensure the bag_of_chips has been set.
  Directory::MetahandlesMap handles_map;
  MetahandleSet metahandles_to_purge;
  Directory::KernelLoadInfo load_info;

  ASSERT_TRUE(dbs.Load(&handles_map, &metahandles_to_purge, &load_info));
  // Check that the initial value is the serialization of an empty ChipBag.
  sync_pb::ChipBag chip_bag;
  std::string serialized_chip_bag;
  ASSERT_TRUE(chip_bag.SerializeToString(&serialized_chip_bag));
  EXPECT_EQ(serialized_chip_bag, load_info.kernel_info.legacy_bag_of_chips);
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion80To81) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion80Database(&connection);

  sql::Statement s(connection.GetUniqueStatement(
      "SELECT metahandle, server_position_in_parent "
      "FROM metas WHERE unique_server_tag = 'google_chrome'"));
  ASSERT_TRUE(s.Step());
  ASSERT_EQ(sql::ColumnType::kInteger, s.GetColumnType(1));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion80To81());
  EXPECT_EQ(81, dbs.GetVersion());

  // Test that ordinal values are preserved correctly.
  sql::Statement new_s(connection.GetUniqueStatement(
      "SELECT metahandle, server_ordinal_in_parent "
      "FROM metas WHERE unique_server_tag = 'google_chrome'"));
  ASSERT_TRUE(new_s.Step());
  EXPECT_EQ(sql::ColumnType::kBlob, new_s.GetColumnType(1));

  std::string expected_ordinal = Int64ToNodeOrdinal(1048576).ToInternalValue();
  std::string actual_ordinal;
  new_s.ColumnBlobAsString(1, &actual_ordinal);
  EXPECT_EQ(expected_ordinal, actual_ordinal);
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion81To82) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion81Database(&connection);
  ASSERT_FALSE(connection.DoesColumnExist("models", "transaction_version"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_FALSE(dbs.needs_column_refresh());
  EXPECT_TRUE(dbs.MigrateVersion81To82());
  EXPECT_EQ(82, dbs.GetVersion());
  EXPECT_FALSE(dbs.needs_column_refresh());

  EXPECT_TRUE(connection.DoesColumnExist("models", "transaction_version"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion82To83) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion82Database(&connection);
  ASSERT_FALSE(connection.DoesColumnExist("metas", "transaction_version"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion82To83());
  EXPECT_EQ(83, dbs.GetVersion());

  EXPECT_TRUE(connection.DoesColumnExist("metas", "transaction_version"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion83To84) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion83Database(&connection);
  ASSERT_FALSE(connection.DoesTableExist("deleted_metas"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion83To84());
  EXPECT_EQ(84, dbs.GetVersion());

  EXPECT_TRUE(connection.DoesTableExist("deleted_metas"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion84To85) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion84Database(&connection);
  ASSERT_TRUE(connection.DoesColumnExist("models", "initial_sync_ended"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion84To85());
  EXPECT_EQ(85, dbs.GetVersion());
  EXPECT_FALSE(connection.DoesColumnExist("models", "initial_sync_ended"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion85To86) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion85Database(&connection);
  ASSERT_TRUE(connection.DoesColumnExist("metas", "next_id"));
  ASSERT_TRUE(connection.DoesColumnExist("metas", "prev_id"));
  ASSERT_TRUE(connection.DoesColumnExist("metas", "server_ordinal_in_parent"));
  ASSERT_FALSE(connection.DoesColumnExist("metas", "unique_position"));
  ASSERT_FALSE(connection.DoesColumnExist("metas", "server_unique_position"));
  ASSERT_FALSE(connection.DoesColumnExist("metas", "unique_bookmark_tag"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion85To86());
  EXPECT_EQ(86, dbs.GetVersion());
  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_position"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "server_unique_position"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_bookmark_tag"));
  EXPECT_TRUE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion86To87) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion86Database(&connection);
  ASSERT_FALSE(connection.DoesColumnExist("metas", "attachment_metadata"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion86To87());
  EXPECT_EQ(87, dbs.GetVersion());
  EXPECT_TRUE(connection.DoesColumnExist("metas", "attachment_metadata"));
  EXPECT_TRUE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion87To88) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion87Database(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion87To88());
  EXPECT_EQ(88, dbs.GetVersion());
  EXPECT_TRUE(connection.DoesColumnExist("models", "context"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion88To89) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion88Database(&connection);
  ASSERT_FALSE(
      connection.DoesColumnExist("metas", "server_attachment_metadata"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion88To89());
  EXPECT_EQ(89, dbs.GetVersion());
  EXPECT_TRUE(
      connection.DoesColumnExist("metas", "server_attachment_metadata"));
  EXPECT_TRUE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion89To90) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion89Database(&connection);
  ASSERT_TRUE(connection.DoesColumnExist("share_info", "db_create_version"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info", "db_create_time"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info", "next_id"));
  ASSERT_TRUE(connection.DoesColumnExist("share_info", "notification_state"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion89To90());
  EXPECT_EQ(90, dbs.GetVersion());
  EXPECT_TRUE(dbs.needs_column_refresh());

  EXPECT_TRUE(dbs.RefreshColumns());
  EXPECT_FALSE(dbs.needs_column_refresh());

  EXPECT_FALSE(connection.DoesColumnExist("share_info", "db_create_version"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "db_create_time"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "next_id"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "notification_state"));
}

// 91 is a cleanup with no schema change, similar to 79, so we just check that
// it succeeds.
TEST_F(DirectoryBackingStoreTest, MigrateVersion90To91) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion90Database(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_FALSE(dbs.needs_column_refresh());
  EXPECT_TRUE(dbs.MigrateVersion90To91());
  EXPECT_EQ(91, dbs.GetVersion());
  EXPECT_FALSE(dbs.needs_column_refresh());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion91To92) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion91Database(&connection);
  ASSERT_TRUE(connection.DoesColumnExist("metas", "attachment_metadata"));
  ASSERT_TRUE(
      connection.DoesColumnExist("metas", "server_attachment_metadata"));

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  EXPECT_TRUE(dbs.MigrateVersion91To92());
  EXPECT_EQ(92, dbs.GetVersion());
  EXPECT_TRUE(dbs.needs_column_refresh());

  EXPECT_TRUE(dbs.RefreshColumns());
  EXPECT_FALSE(dbs.needs_column_refresh());

  EXPECT_FALSE(connection.DoesColumnExist("metas", "attachment_metadata"));
  EXPECT_FALSE(
      connection.DoesColumnExist("metas", "server_attachment_metadata"));
}

// The purpose of this test case is to make it easier to get a dump of the
// database so you can implement a SetUpVersionYDatabase method.  Here's what
// you should do:
//
//   1. Say you're going from version X to version Y.  Write the migration
//      method MigrateVersionXToY.
//   2. Update the test below to call SetUpVersionXDatabase and then
//      MigrateVersionXToY. You now have a database at version Y. Let's dump it.
//   3. Set a breakpoint to stop execution just after the connection is
//      destroyed.  Examine temp_dir_ to find the version Y database that was
//      created on disk. E.g. (gdb) p temp_dir_.GetPath().value().c_str()
//   4. Dump the database using the sqlite3 command line tool:
//        > .output foo_dump.sql
//        > .dump
//   5. Replace the timestamp columns with META_PROTO_TIMES(x) (or
//      LEGACY_META_PROTO_TIMES(x) if before Version 77). Use this dump to write
//      a SetupVersionYDatabase method.
TEST_F(DirectoryBackingStoreTest, MigrateToLatestAndDump) {
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    SetUpVersion91Database(&connection);  // Update this.

    TestDirectoryBackingStore dbs(GetUsername(), &connection);
    EXPECT_TRUE(dbs.MigrateVersion91To92());  // Update this.
    EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));
    EXPECT_EQ(92, dbs.GetVersion());  // Update this.
    EXPECT_FALSE(dbs.needs_column_refresh());
  }
  // Set breakpoint here.
}

TEST_F(DirectoryBackingStoreTest, DetectInvalidPosition) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpVersion86Database(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  ASSERT_EQ(86, dbs.GetVersion());

  // Insert row with bad position.
  sql::Statement s(connection.GetUniqueStatement(
      "INSERT INTO metas "
      "( id, metahandle, is_dir, ctime, mtime,"
      "  unique_position, server_unique_position) "
      "VALUES('c-invalid', 9999, 1, 0, 0, 'BAD_POS', 'BAD_POS')"));
  ASSERT_TRUE(s.Run());

  // Trying to unpack this entry should signal that the DB is corrupted.
  Directory::MetahandlesMap handles_map;
  MetahandleSet metahandles_to_purge;
  Directory::KernelLoadInfo kernel_load_info;
  EXPECT_EQ(FAILED_DATABASE_CORRUPT,
            dbs.Load(&handles_map, &metahandles_to_purge, &kernel_load_info));
}

TEST_F(DirectoryBackingStoreTest, DetectCorruptedRoot) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());
  SetUpCorruptedRootDatabase(&connection);

  TestDirectoryBackingStore dbs(GetUsername(), &connection);

  Directory::MetahandlesMap handles_map;
  MetahandleSet metahandles_to_purge;
  Directory::KernelLoadInfo kernel_load_info;
  EXPECT_EQ(FAILED_DATABASE_CORRUPT,
            dbs.Load(&handles_map, &metahandles_to_purge, &kernel_load_info));
}

TEST_P(MigrationTest, ToCurrentVersion) {
  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  // Assume all old versions have an old page size.
  connection.set_page_size(4096);
  switch (GetParam()) {
    case 67:
      SetUpVersion67Database(&connection);
      break;
    case 68:
      SetUpVersion68Database(&connection);
      break;
    case 69:
      SetUpVersion69Database(&connection);
      break;
    case 70:
      SetUpVersion70Database(&connection);
      break;
    case 71:
      SetUpVersion71Database(&connection);
      break;
    case 72:
      SetUpVersion72Database(&connection);
      break;
    case 73:
      SetUpVersion73Database(&connection);
      break;
    case 74:
      SetUpVersion74Database(&connection);
      break;
    case 75:
      SetUpVersion75Database(&connection);
      break;
    case 76:
      SetUpVersion76Database(&connection);
      break;
    case 77:
      SetUpVersion77Database(&connection);
      break;
    case 78:
      SetUpVersion78Database(&connection);
      break;
    case 79:
      SetUpVersion79Database(&connection);
      break;
    case 80:
      SetUpVersion80Database(&connection);
      break;
    case 81:
      SetUpVersion81Database(&connection);
      break;
    case 82:
      SetUpVersion82Database(&connection);
      break;
    case 83:
      SetUpVersion83Database(&connection);
      break;
    case 84:
      SetUpVersion84Database(&connection);
      break;
    case 85:
      SetUpVersion85Database(&connection);
      break;
    case 86:
      SetUpVersion86Database(&connection);
      break;
    case 87:
      SetUpVersion87Database(&connection);
      break;
    case 88:
      SetUpVersion88Database(&connection);
      break;
    case 89:
      SetUpVersion89Database(&connection);
      break;
    case 90:
      SetUpVersion90Database(&connection);
      break;
    case 91:
      SetUpVersion91Database(&connection);
      break;
    case 92:
      SetUpVersion92Database(&connection);
      break;
    default:
      // If you see this error, it may mean that you've increased the
      // database version number but you haven't finished adding unit tests
      // for the database migration code.  You need to need to supply a
      // SetUpVersionYDatabase function with a dump of the test database
      // at the new schema.  See the MigrateToLatestAndDump test case.
      FAIL() << "Need to supply database dump for version " << GetParam();
  }
  connection.Close();

  syncable::Directory::KernelLoadInfo dir_info;
  Directory::MetahandlesMap handles_map;
  MetahandleSet metahandles_to_purge;

  {
    OnDiskDirectoryBackingStore dbs(GetUsername(), TestCacheGuidGenerator(),
                                    GetDatabasePath());
    ASSERT_EQ(OPENED_EXISTING,
              dbs.Load(&handles_map, &metahandles_to_purge, &dir_info));
    if (!metahandles_to_purge.empty())
      dbs.DeleteEntries(DirectoryBackingStore::METAS_TABLE,
                        metahandles_to_purge);
    EXPECT_FALSE(dbs.needs_column_refresh());
    EXPECT_EQ(kCurrentDBVersion, dbs.GetVersion());
    int pageSize = 0;
    EXPECT_TRUE(dbs.GetDatabasePageSize(&pageSize));
    EXPECT_EQ(kCurrentPageSizeKB, pageSize);
  }

  ASSERT_TRUE(connection.Open(GetDatabasePath()));

  // Columns deleted in Version 67.
  EXPECT_FALSE(connection.DoesColumnExist("metas", "name"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "unsanitized_name"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "server_name"));

  // Columns added in Version 68.
  EXPECT_TRUE(connection.DoesColumnExist("metas", "specifics"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "server_specifics"));

  // Columns deleted in Version 68.
  EXPECT_FALSE(connection.DoesColumnExist("metas", "is_bookmark_object"));
  EXPECT_FALSE(
      connection.DoesColumnExist("metas", "server_is_bookmark_object"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "bookmark_favicon"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "bookmark_url"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "server_bookmark_url"));

  // Renamed a column in Version 70
  EXPECT_FALSE(connection.DoesColumnExist("metas", "singleton_tag"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_server_tag"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_client_tag"));

  // Removed extended attributes in Version 72.
  EXPECT_FALSE(connection.DoesTableExist("extended_attributes"));

  // Column replaced in version 75.
  EXPECT_TRUE(connection.DoesColumnExist("models", "progress_marker"));
  EXPECT_FALSE(connection.DoesColumnExist("models", "last_download_timestamp"));

  // Columns removed in version 76.
  EXPECT_FALSE(
      connection.DoesColumnExist("share_info", "autofill_migration_state"));
  EXPECT_FALSE(connection.DoesColumnExist(
      "share_info", "bookmarks_added_during_autofill_migration"));
  EXPECT_FALSE(
      connection.DoesColumnExist("share_info", "autofill_migration_time"));
  EXPECT_FALSE(connection.DoesColumnExist(
      "share_info", "autofill_entries_added_during_migration"));
  EXPECT_FALSE(connection.DoesColumnExist(
      "share_info", "autofill_profiles_added_during_migration"));

  // Column added in version 78.
  EXPECT_TRUE(connection.DoesColumnExist("metas", "base_server_specifics"));

  // Column added in version 82.
  EXPECT_TRUE(connection.DoesColumnExist("models", "transaction_version"));

  // Column added in version 83.
  EXPECT_TRUE(connection.DoesColumnExist("metas", "transaction_version"));

  // Table added in version 84.
  EXPECT_TRUE(connection.DoesTableExist("deleted_metas"));

  // Column removed in version 85.
  EXPECT_FALSE(connection.DoesColumnExist("models", "initial_sync_ended"));

  // Columns removed in version 86.
  EXPECT_FALSE(connection.DoesColumnExist("metas", "next_id"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "prev_id"));
  EXPECT_FALSE(connection.DoesColumnExist("metas", "server_ordinal_in_parent"));

  // Column added in version 88.
  EXPECT_TRUE(connection.DoesColumnExist("models", "context"));

  // Columns removed in version 90.
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "db_create_version"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "db_create_time"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "next_id"));
  EXPECT_FALSE(connection.DoesColumnExist("share_info", "notification_state"));

  // Columns removed in version 92.
  EXPECT_FALSE(connection.DoesColumnExist("metas", "attachment_metadata"));
  EXPECT_FALSE(
      connection.DoesColumnExist("metas", "server_attachment_metadata"));

  // Check download_progress state (v75 migration)
  EXPECT_EQ(694, dir_info.kernel_info.download_progress[BOOKMARKS]
                     .timestamp_token_for_migration());
  EXPECT_FALSE(dir_info.kernel_info.download_progress[BOOKMARKS].has_token());
  EXPECT_EQ(32904,
            dir_info.kernel_info.download_progress[BOOKMARKS].data_type_id());
  EXPECT_FALSE(dir_info.kernel_info.download_progress[THEMES]
                   .has_timestamp_token_for_migration());
  EXPECT_TRUE(dir_info.kernel_info.download_progress[THEMES].has_token());
  EXPECT_TRUE(dir_info.kernel_info.download_progress[THEMES].token().empty());
  EXPECT_EQ(41210,
            dir_info.kernel_info.download_progress[THEMES].data_type_id());

  // Check metas
  EXPECT_EQ(GetExpectedMetaProtoTimes(DONT_INCLUDE_DELETED_ITEMS),
            GetMetaProtoTimes(&connection));
  ExpectTimes(handles_map, GetExpectedMetaTimes());

  auto it = handles_map.find(1);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(1, it->second->ref(META_HANDLE));
  EXPECT_TRUE(it->second->ref(ID).IsRoot());
  EXPECT_FALSE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());

  // Items 2, 4, and 5 were deleted.
  it = handles_map.find(2);
  EXPECT_TRUE(it == handles_map.end());
  it = handles_map.find(4);
  EXPECT_TRUE(it == handles_map.end());
  it = handles_map.find(5);
  EXPECT_TRUE(it == handles_map.end());

  it = handles_map.find(6);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(6, it->second->ref(META_HANDLE));
  EXPECT_TRUE(it->second->ref(IS_DIR));
  EXPECT_TRUE(it->second->ref(SERVER_IS_DIR));
  EXPECT_FALSE(it->second->ref(SPECIFICS).bookmark().has_url());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).bookmark().has_url());
  EXPECT_FALSE(it->second->ref(SPECIFICS).bookmark().has_favicon());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).bookmark().has_favicon());
  EXPECT_TRUE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_EQ(UniquePosition::kSuffixLength,
            it->second->ref(UNIQUE_BOOKMARK_TAG).length());

  it = handles_map.find(7);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(7, it->second->ref(META_HANDLE));
  EXPECT_EQ("google_chrome", it->second->ref(UNIQUE_SERVER_TAG));
  EXPECT_FALSE(it->second->ref(SPECIFICS).has_bookmark());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).has_bookmark());
  // Make sure we didn't assign positions to google_chrome.
  EXPECT_FALSE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());

  it = handles_map.find(8);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(8, it->second->ref(META_HANDLE));
  EXPECT_EQ("google_chrome_bookmarks", it->second->ref(UNIQUE_SERVER_TAG));
  EXPECT_TRUE(it->second->ref(SPECIFICS).has_bookmark());
  EXPECT_TRUE(it->second->ref(SERVER_SPECIFICS).has_bookmark());
  EXPECT_EQ(it->second->ref(ID).value(), "s_ID_8");
  // Make sure we didn't mistake the bookmark root node for a real bookmark.
  EXPECT_FALSE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());

  it = handles_map.find(9);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(9, it->second->ref(META_HANDLE));
  EXPECT_EQ("bookmark_bar", it->second->ref(UNIQUE_SERVER_TAG));
  EXPECT_TRUE(it->second->ref(SPECIFICS).has_bookmark());
  EXPECT_TRUE(it->second->ref(SERVER_SPECIFICS).has_bookmark());
  // Make sure we didn't assign positions to bookmark_bar.
  EXPECT_FALSE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());

  it = handles_map.find(10);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(10, it->second->ref(META_HANDLE));
  EXPECT_FALSE(it->second->ref(IS_DEL));
  EXPECT_TRUE(it->second->ref(SPECIFICS).has_bookmark());
  EXPECT_TRUE(it->second->ref(SERVER_SPECIFICS).has_bookmark());
  EXPECT_FALSE(it->second->ref(SPECIFICS).bookmark().has_url());
  EXPECT_FALSE(it->second->ref(SPECIFICS).bookmark().has_favicon());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).bookmark().has_url());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).bookmark().has_favicon());
  EXPECT_EQ("other_bookmarks", it->second->ref(UNIQUE_SERVER_TAG));
  EXPECT_EQ("Other Bookmarks", it->second->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Other Bookmarks", it->second->ref(SERVER_NON_UNIQUE_NAME));
  EXPECT_EQ(it->second->ref(ID).value(), "s_ID_10");
  // Make sure we didn't assign positions to server-created folders, either.
  EXPECT_FALSE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_FALSE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());

  it = handles_map.find(11);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(11, it->second->ref(META_HANDLE));
  EXPECT_FALSE(it->second->ref(IS_DEL));
  EXPECT_FALSE(it->second->ref(IS_DIR));
  EXPECT_TRUE(it->second->ref(SPECIFICS).has_bookmark());
  EXPECT_TRUE(it->second->ref(SERVER_SPECIFICS).has_bookmark());
  EXPECT_EQ("http://dev.chromium.org/",
            it->second->ref(SPECIFICS).bookmark().url());
  EXPECT_EQ("AGATWA", it->second->ref(SPECIFICS).bookmark().favicon());
  EXPECT_EQ("http://dev.chromium.org/other",
            it->second->ref(SERVER_SPECIFICS).bookmark().url());
  EXPECT_EQ("AFAGVASF", it->second->ref(SERVER_SPECIFICS).bookmark().favicon());
  EXPECT_EQ("", it->second->ref(UNIQUE_SERVER_TAG));
  EXPECT_EQ("Home (The Chromium Projects)", it->second->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Home (The Chromium Projects)",
            it->second->ref(SERVER_NON_UNIQUE_NAME));
  EXPECT_TRUE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_EQ(UniquePosition::kSuffixLength,
            it->second->ref(UNIQUE_BOOKMARK_TAG).length());

  it = handles_map.find(12);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(12, it->second->ref(META_HANDLE));
  EXPECT_FALSE(it->second->ref(IS_DEL));
  EXPECT_TRUE(it->second->ref(IS_DIR));
  EXPECT_EQ("Extra Bookmarks", it->second->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Extra Bookmarks", it->second->ref(SERVER_NON_UNIQUE_NAME));
  EXPECT_TRUE(it->second->ref(SPECIFICS).has_bookmark());
  EXPECT_TRUE(it->second->ref(SERVER_SPECIFICS).has_bookmark());
  EXPECT_FALSE(it->second->ref(SPECIFICS).bookmark().has_url());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).bookmark().has_url());
  EXPECT_FALSE(it->second->ref(SPECIFICS).bookmark().has_favicon());
  EXPECT_FALSE(it->second->ref(SERVER_SPECIFICS).bookmark().has_favicon());
  EXPECT_TRUE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_EQ(UniquePosition::kSuffixLength,
            it->second->ref(UNIQUE_BOOKMARK_TAG).length());

  it = handles_map.find(13);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(13, it->second->ref(META_HANDLE));
  EXPECT_TRUE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_EQ(UniquePosition::kSuffixLength,
            it->second->ref(UNIQUE_BOOKMARK_TAG).length());

  it = handles_map.find(14);
  ASSERT_TRUE(it != handles_map.end());
  EXPECT_EQ(14, it->second->ref(META_HANDLE));
  EXPECT_TRUE(it->second->ref(UNIQUE_POSITION).IsValid());
  EXPECT_TRUE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
  EXPECT_EQ(UniquePosition::kSuffixLength,
            it->second->ref(UNIQUE_BOOKMARK_TAG).length());

  EXPECT_EQ(static_cast<size_t>(10), handles_map.size());

  // Make sure that the syncable::Directory and the migration code agree on
  // which items should or should not have unique position values.  This test
  // may become obsolete if the directory's definition of that function
  // changes, but, until then, this is a useful test.
  for (it = handles_map.begin(); it != handles_map.end(); it++) {
    SCOPED_TRACE(it->second->ref(ID));
    if (it->second->ShouldMaintainPosition()) {
      EXPECT_TRUE(it->second->ref(UNIQUE_POSITION).IsValid());
      EXPECT_TRUE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
      EXPECT_FALSE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());
    } else {
      EXPECT_FALSE(it->second->ref(UNIQUE_POSITION).IsValid());
      EXPECT_FALSE(it->second->ref(SERVER_UNIQUE_POSITION).IsValid());
      EXPECT_TRUE(it->second->ref(UNIQUE_BOOKMARK_TAG).empty());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(DirectoryBackingStore,
                         MigrationTest,
                         testing::Range(67, kCurrentDBVersion + 1));

TEST_F(DirectoryBackingStoreTest, ModelTypeIds) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    std::string model_id =
        TestDirectoryBackingStore::ModelTypeEnumToModelId(type);
    EXPECT_EQ(type, TestDirectoryBackingStore::ModelIdToModelTypeEnum(
                        model_id.data(), model_id.size()));
  }
}

namespace {

class OnDiskDirectoryBackingStoreForTest : public OnDiskDirectoryBackingStore {
 public:
  OnDiskDirectoryBackingStoreForTest(const std::string& dir_name,
                                     const base::FilePath& backing_filepath);
  ~OnDiskDirectoryBackingStoreForTest() override;
  bool DidFailFirstOpenAttempt();

 protected:
  void ReportFirstTryOpenFailure() override;

 private:
  bool first_open_failed_;
};

OnDiskDirectoryBackingStoreForTest::OnDiskDirectoryBackingStoreForTest(
    const std::string& dir_name,
    const base::FilePath& backing_filepath)
    : OnDiskDirectoryBackingStore(dir_name,
                                  TestCacheGuidGenerator(),
                                  backing_filepath),
      first_open_failed_(false) {}

OnDiskDirectoryBackingStoreForTest::~OnDiskDirectoryBackingStoreForTest() { }

void OnDiskDirectoryBackingStoreForTest::ReportFirstTryOpenFailure() {
  // Do nothing, just like we would in release-mode.  In debug mode, we DCHECK.
  first_open_failed_ = true;
}

bool OnDiskDirectoryBackingStoreForTest::DidFailFirstOpenAttempt() {
  return first_open_failed_;
}

}  // namespace

// This is a whitebox test intended to exercise the code path where the on-disk
// directory load code decides to delete the current directory and start fresh.
//
// This is considered "minor" corruption because the database recreation is
// expected to succeed.  The alternative, where recreation does not succeed (ie.
// due to read-only file system), is not tested here.
TEST_F(DirectoryBackingStoreTest, MinorCorruption) {
  {
    OnDiskDirectoryBackingStore dbs(GetUsername(), TestCacheGuidGenerator(),
                                    GetDatabasePath());
    EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));
  }

  // Corrupt the root node.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(connection.Execute(
            "UPDATE metas SET parent_id='bogus' WHERE id = 'r';"));
  }

  {
    OnDiskDirectoryBackingStoreForTest dbs(GetUsername(), GetDatabasePath());

    EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));
    EXPECT_TRUE(dbs.DidFailFirstOpenAttempt());
  }
}

TEST_F(DirectoryBackingStoreTest, MinorCorruptionAndUpgrade) {
  {
    OnDiskDirectoryBackingStore dbs(GetUsername(), TestCacheGuidGenerator(),
                                    GetDatabasePath());
    EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));
  }

  // Make the node look outdated with an invalid version.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(connection.Execute("UPDATE share_version SET data = 0;"));
    ASSERT_TRUE(connection.Execute("PRAGMA page_size=4096;"));
    ASSERT_TRUE(connection.Execute("VACUUM;"));
  }

  {
    OnDiskDirectoryBackingStoreForTest dbs(GetUsername(), GetDatabasePath());
    dbs.SetCatastrophicErrorHandler(base::DoNothing());

    EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));
    EXPECT_TRUE(dbs.DidFailFirstOpenAttempt());

    int page_size = 0;
    EXPECT_TRUE(dbs.GetDatabasePageSize(&page_size));
    EXPECT_EQ(kCurrentPageSizeKB, page_size);
  }
}

TEST_F(DirectoryBackingStoreTest, DeleteEntries) {
  sql::Database connection;
  ASSERT_TRUE(connection.OpenInMemory());

  SetUpCurrentDatabaseAndCheckVersion(&connection);
  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  Directory::MetahandlesMap handles_map;
  MetahandleSet metahandles_to_purge;
  Directory::KernelLoadInfo kernel_load_info;

  dbs.Load(&handles_map, &metahandles_to_purge, &kernel_load_info);
  size_t initial_size = handles_map.size();
  ASSERT_LT(0U, initial_size) << "Test requires handles_map to delete.";
  int64_t first_to_die = handles_map.begin()->second->ref(META_HANDLE);
  MetahandleSet to_delete;
  to_delete.insert(first_to_die);
  EXPECT_TRUE(dbs.DeleteEntries(to_delete));

  handles_map.clear();
  metahandles_to_purge.clear();
  dbs.LoadEntries(&handles_map, &metahandles_to_purge);

  EXPECT_EQ(initial_size - 1, handles_map.size());
  bool delete_failed = false;
  for (auto it = handles_map.begin(); it != handles_map.end(); ++it) {
    if (it->first == first_to_die) {
      delete_failed = true;
      break;
    }
  }
  EXPECT_FALSE(delete_failed);

  to_delete.clear();
  for (auto it = handles_map.begin(); it != handles_map.end(); ++it) {
    to_delete.insert(it->first);
  }

  EXPECT_TRUE(dbs.DeleteEntries(to_delete));

  handles_map.clear();
  metahandles_to_purge.clear();
  dbs.LoadEntries(&handles_map, &metahandles_to_purge);
  EXPECT_EQ(0U, handles_map.size());
}

TEST_F(DirectoryBackingStoreTest, IncreaseDatabasePageSizeFrom4KTo32K) {
  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));

  SetUpCurrentDatabaseAndCheckVersion(&connection);
  TestDirectoryBackingStore dbs(GetUsername(), &connection);
  Directory::MetahandlesMap handles_map;
  MetahandleSet metahandles_to_purge;
  Directory::KernelLoadInfo kernel_load_info;

  DirOpenResult open_result =
      dbs.Load(&handles_map, &metahandles_to_purge, &kernel_load_info);
  EXPECT_EQ(open_result, OPENED_EXISTING);

  // Set up database's page size to 4096
  EXPECT_TRUE(dbs.db_->Execute("PRAGMA page_size=4096;"));
  EXPECT_TRUE(dbs.Vacuum());

  // Check if update is successful.
  int pageSize = 0;
  EXPECT_TRUE(dbs.GetDatabasePageSize(&pageSize));
  EXPECT_NE(kCurrentPageSizeKB, pageSize);
  EXPECT_TRUE(dbs.UpdatePageSizeIfNecessary());
  pageSize = 0;
  EXPECT_TRUE(dbs.GetDatabasePageSize(&pageSize));
  EXPECT_EQ(kCurrentPageSizeKB, pageSize);
}

// See that a catastrophic error handler remains set across instances of the
// underlying sql:Connection.
TEST_F(DirectoryBackingStoreTest, CatastrophicErrorHandler_KeptAcrossReset) {
  OnDiskDirectoryBackingStoreForTest dbs(GetUsername(), GetDatabasePath());
  // See that by default there is no catastrophic error handler.
  ASSERT_FALSE(dbs.db_->has_error_callback());
  // Set one and see that it was set.
  dbs.SetCatastrophicErrorHandler(
      base::Bind(&CatastrophicErrorHandler, nullptr));
  EXPECT_TRUE(dbs.db_->has_error_callback());
  // Recreate the Connection and see that the handler remains set.
  dbs.ResetAndCreateConnection();
  EXPECT_TRUE(dbs.db_->has_error_callback());
}

// Verify that database corruption encountered during Load will trigger the
// catastrohpic error handler.
TEST_F(DirectoryBackingStoreTest,
       CatastrophicErrorHandler_InvocationDuringLoad) {
  bool was_called = false;
  const base::Closure handler =
      base::Bind(&CatastrophicErrorHandler, &was_called);
  {
    OnDiskDirectoryBackingStoreForTest dbs(GetUsername(), GetDatabasePath());
    dbs.SetCatastrophicErrorHandler(handler);
    EXPECT_TRUE(dbs.db_->has_error_callback());
    // Load the DB, and save one entry.
    ASSERT_TRUE(LoadAndIgnoreReturnedData(&dbs));
    EXPECT_FALSE(dbs.DidFailFirstOpenAttempt());
    Directory::SaveChangesSnapshot snapshot;
    snapshot.dirty_metas.insert(CreateEntry(2, ""));
    ASSERT_TRUE(dbs.SaveChanges(snapshot));
  }

  base::RunLoop().RunUntilIdle();
  // No catastrophic errors have happened. See that it hasn't be called yet.
  EXPECT_FALSE(was_called);

  // Corrupt the DB. Some forms of corruption (like this one) will be detected
  // upon loading the Sync DB.
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(GetDatabasePath()));

  {
    OnDiskDirectoryBackingStoreForTest dbs(GetUsername(), GetDatabasePath());
    dbs.SetCatastrophicErrorHandler(handler);
    EXPECT_TRUE(dbs.db_->has_error_callback());
    EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));

    // See that the first open failed as expected.
    EXPECT_TRUE(dbs.DidFailFirstOpenAttempt());
  }

  // At this point the handler has been posted but not executed.
  EXPECT_FALSE(was_called);
  // Pump the message loop and see that it is executed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
}

// Verify that database corruption encountered during SaveChanges will trigger
// the catastrohpic error handler.
TEST_F(DirectoryBackingStoreTest,
       CatastrophicErrorHandler_InvocationDuringSaveChanges) {
  bool was_called = false;
  const base::Closure handler =
      base::Bind(&CatastrophicErrorHandler, &was_called);
  // Create a DB with many entries.
  OnDiskDirectoryBackingStoreForTest dbs(GetUsername(), GetDatabasePath());
  dbs.SetCatastrophicErrorHandler(handler);
  EXPECT_TRUE(dbs.db_->has_error_callback());
  EXPECT_TRUE(LoadAndIgnoreReturnedData(&dbs));
  EXPECT_FALSE(dbs.DidFailFirstOpenAttempt());
  Directory::SaveChangesSnapshot snapshot;
  const std::string suffix(400, 'o');
  for (size_t i = 0; i < 100; ++i) {
    snapshot.dirty_metas.insert(CreateEntry(i, suffix));
  }
  ASSERT_TRUE(dbs.SaveChanges(snapshot));
  // Corrupt the database on disk.
  ASSERT_TRUE(sql::test::CorruptSizeInHeaderWithLock(GetDatabasePath()));
  // Attempt to save all those entries again. See that it fails (because of the
  // corruption).
  EXPECT_FALSE(dbs.SaveChanges(snapshot));
  // At this point the handler has been posted but not executed.
  EXPECT_FALSE(was_called);
  // Pump the message loop and see that it is executed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_called);
}

}  // namespace syncable
}  // namespace syncer
