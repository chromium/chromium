// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocklist/opt_out_blocklist/sql/opt_out_store_sql.h"

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blocklist {

namespace {

const base::FilePath::CharType kOptOutFilename[] = FILE_PATH_LITERAL("OptOut");

}  // namespace

class OptOutStoreSQLTest : public testing::Test {
 public:
  OptOutStoreSQLTest() = default;
  ~OptOutStoreSQLTest() override = default;

  // Called when |store_| is done loading.
  void OnLoaded(std::unique_ptr<BlocklistData> blocklist_data) {
    blocklist_data_ = std::move(blocklist_data);
  }

  // Initializes the store and get the data from it.
  void Load() {
    // Choose reasonable constants.
    std::unique_ptr<BlocklistData> data = std::make_unique<BlocklistData>(
        std::make_unique<BlocklistData::Policy>(base::Minutes(5), 1, 1),
        std::make_unique<BlocklistData::Policy>(base::Days(30), 10, 6u),
        std::make_unique<BlocklistData::Policy>(base::Days(30), 4, 2u), nullptr,
        10, allowed_types_);

    store_->LoadBlockList(
        std::move(data),
        base::BindOnce(&OptOutStoreSQLTest::OnLoaded, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Destroys the database connection and |store_|.
  void DestroyStore() {
    store_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Creates a store that operates on one thread.
  void Create() {
    store_ = std::make_unique<OptOutStoreSQL>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        temp_dir_.GetPath().Append(kOptOutFilename));
  }

  // Sets up initialization of |store_|.
  void CreateAndLoad() {
    Create();
    Load();
  }

  void SetEnabledTypes(BlocklistData::AllowedTypesAndVersions allowed_types) {
    allowed_types_ = std::move(allowed_types);
  }

  // Creates a directory for the test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Delete |store_| if it hasn't been deleted.
  void TearDown() override { DestroyStore(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // The backing SQL store.
  std::unique_ptr<OptOutStoreSQL> store_;

  // The map returned from |store_|.
  std::unique_ptr<BlocklistData> blocklist_data_;

  // The directory for the database.
  base::ScopedTempDir temp_dir_;

 private:
  BlocklistData::AllowedTypesAndVersions allowed_types_;
};

TEST_F(OptOutStoreSQLTest, TestErrorRecovery) {
  // Creates the database and corrupt to test the recovery method.
  std::string test_host = "host.com";
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  store_->AddEntry(true, test_host, 1, base::Time::Now());
  base::RunLoop().RunUntilIdle();
  DestroyStore();

  // Corrupts the database by adjusting the header size.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(
      temp_dir_.GetPath().Append(kOptOutFilename)));
  base::RunLoop().RunUntilIdle();

  allowed_types.clear();
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  // The data should be recovered.
  EXPECT_EQ(1U, blocklist_data_->block_list_item_host_map().size());
  const auto& iter =
      blocklist_data_->block_list_item_host_map().find(test_host);

  EXPECT_NE(blocklist_data_->block_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());
}

TEST_F(OptOutStoreSQLTest, TestPersistance) {
  // Tests if data is stored as expected in the SQLite database.
  std::string test_host = "host.com";
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  base::Time now = base::Time::Now();
  store_->AddEntry(true, test_host, 1, now);
  base::RunLoop().RunUntilIdle();

  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk.
  DestroyStore();

  // Reload and test for persistence
  allowed_types.clear();
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  EXPECT_EQ(1U, blocklist_data_->block_list_item_host_map().size());
  const auto& iter =
      blocklist_data_->block_list_item_host_map().find(test_host);

  EXPECT_NE(blocklist_data_->block_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());
  EXPECT_EQ(now, iter->second.most_recent_opt_out_time().value());
}

TEST_F(OptOutStoreSQLTest, TestMaxRows) {
  // Tests that the number of rows are culled down to the row limit at each
  // load.
  std::string test_host_a = "host_a.com";
  std::string test_host_b = "host_b.com";
  std::string test_host_c = "host_c.com";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  size_t row_limit = 2;
  std::string row_limit_string = base::NumberToString(row_limit);
  command_line->AppendSwitchASCII("max-opt-out-rows", row_limit_string);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  base::SimpleTestClock clock;

  // Create three different entries with different hosts.
  store_->AddEntry(true, test_host_a, 1, clock.Now());
  clock.Advance(base::Seconds(1));

  store_->AddEntry(true, test_host_b, 1, clock.Now());
  base::Time host_b_time = clock.Now();
  clock.Advance(base::Seconds(1));

  store_->AddEntry(false, test_host_c, 1, clock.Now());
  base::RunLoop().RunUntilIdle();
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk.
  DestroyStore();

  // Reload and test for persistence
  allowed_types.clear();
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  // The delete happens after the load, so it is possible to load more than
  // |row_limit| into the in memory map.
  EXPECT_EQ(row_limit + 1, blocklist_data_->block_list_item_host_map().size());

  DestroyStore();
  allowed_types.clear();
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();

  EXPECT_EQ(row_limit, blocklist_data_->block_list_item_host_map().size());
  const auto& iter_host_b =
      blocklist_data_->block_list_item_host_map().find(test_host_b);
  const auto& iter_host_c =
      blocklist_data_->block_list_item_host_map().find(test_host_c);

  EXPECT_EQ(blocklist_data_->block_list_item_host_map().end(),
            blocklist_data_->block_list_item_host_map().find(test_host_a));
  EXPECT_NE(blocklist_data_->block_list_item_host_map().end(), iter_host_b);
  EXPECT_NE(blocklist_data_->block_list_item_host_map().end(), iter_host_c);
  EXPECT_EQ(host_b_time,
            iter_host_b->second.most_recent_opt_out_time().value());
  EXPECT_EQ(1U, iter_host_b->second.OptOutRecordsSizeForTesting());
}

TEST_F(OptOutStoreSQLTest, TestMaxRowsPerHost) {
  // Tests that each host is limited to |row_limit| rows.
  std::string test_host = "host.com";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  size_t row_limit = 2;
  std::string row_limit_string = base::NumberToString(row_limit);
  command_line->AppendSwitchASCII("max-opt-out-rows-per-host",
                                  row_limit_string);
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  base::SimpleTestClock clock;

  base::Time last_opt_out_time;
  for (size_t i = 0; i < row_limit; i++) {
    store_->AddEntry(true, test_host, 1, clock.Now());
    last_opt_out_time = clock.Now();
    clock.Advance(base::Seconds(1));
  }

  clock.Advance(base::Seconds(1));
  store_->AddEntry(false, test_host, 1, clock.Now());

  base::RunLoop().RunUntilIdle();
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk.
  DestroyStore();

  // Reload and test for persistence.
  allowed_types.clear();
  allowed_types.insert({1, 0});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();

  EXPECT_EQ(1U, blocklist_data_->block_list_item_host_map().size());
  const auto& iter =
      blocklist_data_->block_list_item_host_map().find(test_host);

  EXPECT_NE(blocklist_data_->block_list_item_host_map().end(), iter);
  EXPECT_EQ(last_opt_out_time, iter->second.most_recent_opt_out_time().value());
  EXPECT_EQ(row_limit, iter->second.OptOutRecordsSizeForTesting());
  clock.Advance(base::Seconds(1));
  // If both entries' opt out states are stored correctly, then this should not
  // be block listed.
  EXPECT_FALSE(iter->second.IsBlockListed(clock.Now()));
}

TEST_F(OptOutStoreSQLTest, TestTypesVersionUpdateClearsBlocklistEntry) {
  // Tests if data is cleared for new version of type.
  std::string test_host = "host.com";
  BlocklistData::AllowedTypesAndVersions allowed_types;
  allowed_types.insert({1, 1});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  base::Time now = base::Time::Now();
  store_->AddEntry(true, test_host, 1, now);
  base::RunLoop().RunUntilIdle();

  // Force data write to database then reload it and verify block list entry
  // is present.
  DestroyStore();
  allowed_types.clear();
  allowed_types.insert({1, 1});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  const auto& iter =
      blocklist_data_->block_list_item_host_map().find(test_host);
  EXPECT_NE(blocklist_data_->block_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());

  DestroyStore();
  allowed_types.clear();
  allowed_types.insert({1, 2});
  SetEnabledTypes(std::move(allowed_types));
  CreateAndLoad();
  const auto& iter2 =
      blocklist_data_->block_list_item_host_map().find(test_host);
  EXPECT_EQ(blocklist_data_->block_list_item_host_map().end(), iter2);
}

}  // namespace blocklist
