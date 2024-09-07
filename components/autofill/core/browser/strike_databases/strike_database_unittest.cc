// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Note: This class is NOT the same as test_strike_database.h. This is an
// actual implementation of StrikeDatabase, but with helper functions
// added for easier test setup. If you want a TestStrikeDatabase, please use the
// one in test_strike_database.h.  This one is purely for this unit test class.
class TestStrikeDatabase : public StrikeDatabase {
 public:
  TestStrikeDatabase(leveldb_proto::ProtoDatabaseProvider* db_provider,
                     base::FilePath profile_path)
      : StrikeDatabase(db_provider, profile_path) {
    database_initialized_ = true;
  }

  TestStrikeDatabase(const TestStrikeDatabase&) = delete;
  TestStrikeDatabase& operator=(const TestStrikeDatabase&) = delete;

  void AddProtoEntries(
      std::vector<std::pair<std::string, StrikeData>> entries_to_add,
      const SetValueCallback& callback) {
    std::unique_ptr<leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector>
        entries(new leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector());
    for (std::pair<std::string, StrikeData> entry : entries_to_add) {
      entries->push_back(entry);
    }
    db_->UpdateEntries(
        /*entries_to_save=*/std::move(entries),
        /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
        callback);
  }
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

// Runs tests against the actual StrikeDatabase class, complete with
// ProtoDatabase.
class StrikeDatabaseTest : public ::testing::Test {
 public:
  StrikeDatabaseTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_ = std::make_unique<TestStrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
  }

  void AddProtoEntries(
      std::vector<std::pair<std::string, StrikeData>> entries_to_add) {
    base::RunLoop run_loop;
    strike_database_->AddProtoEntries(
        entries_to_add,
        base::BindRepeating(&StrikeDatabaseTest::OnAddProtoEntries,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnAddProtoEntries(base::RepeatingClosure run_loop_closure,
                         bool success) {
    run_loop_closure.Run();
  }

  void OnGetProtoStrikes(base::RepeatingClosure run_loop_closure,
                         int num_strikes) {
    num_strikes_ = num_strikes;
    run_loop_closure.Run();
  }

  int GetProtoStrikes(std::string key) {
    base::RunLoop run_loop;
    strike_database_->GetProtoStrikes(
        key,
        base::BindRepeating(&StrikeDatabaseTest::OnGetProtoStrikes,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return num_strikes_;
  }

  void OnClearAllProtoStrikesForKey(base::RepeatingClosure run_loop_closure,
                                    bool success) {
    run_loop_closure.Run();
  }

  void ClearAllProtoStrikesForKey(const std::string key) {
    base::RunLoop run_loop;
    strike_database_->ClearAllProtoStrikesForKey(
        key,
        base::BindRepeating(&StrikeDatabaseTest::OnClearAllProtoStrikesForKey,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnClearAllProtoStrikesForKeys(base::RepeatingClosure run_loop_closure,
                                     bool success) {
    run_loop_closure.Run();
  }

  void ClearAllProtoStrikesForKeys(const std::vector<std::string>& keys) {
    base::RunLoop run_loop;
    strike_database_->ClearAllProtoStrikesForKeys(
        keys,
        base::BindRepeating(&StrikeDatabaseTest::OnClearAllProtoStrikesForKeys,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnClearAllProtoStrikes(base::RepeatingClosure run_loop_closure,
                              bool success) {
    run_loop_closure.Run();
  }

  void ClearAllProtoStrikes() {
    base::RunLoop run_loop;
    strike_database_->ClearAllProtoStrikes(
        base::BindRepeating(&StrikeDatabaseTest::OnClearAllProtoStrikesForKey,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<TestStrikeDatabase> strike_database_;

 private:
  base::HistogramTester histogram_tester_;
  int num_strikes_;
  std::unique_ptr<StrikeData> strike_data_;
};

TEST_F(StrikeDatabaseTest, GetStrikesForMissingKeyTest) {
  const std::string key = "12345";
  int strikes = GetProtoStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, GetStrikeForNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key|.
  const std::string key = "12345";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.emplace_back(key, data);
  AddProtoEntries(entries);

  int strikes = GetProtoStrikes(key);
  EXPECT_EQ(3, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForMissingKeyTest) {
  const std::string key = "12345";
  ClearAllProtoStrikesForKey(key);
  int strikes = GetProtoStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key|.
  const std::string key = "12345";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.emplace_back(key, data);
  AddProtoEntries(entries);

  int strikes = GetProtoStrikes(key);
  EXPECT_EQ(3, strikes);
  ClearAllProtoStrikesForKey(key);
  strikes = GetProtoStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForMultipleNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes for three different keys.
  const std::string key1 = "12345";
  const std::string key2 = "67890";
  const std::string key3 = "99000";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.emplace_back(key1, data);
  entries.emplace_back(key2, data);
  entries.emplace_back(key3, data);
  AddProtoEntries(entries);

  EXPECT_EQ(3, GetProtoStrikes(key1));
  EXPECT_EQ(3, GetProtoStrikes(key2));
  EXPECT_EQ(3, GetProtoStrikes(key3));
  std::vector<std::string> keys_to_clear({key1, key2});
  ClearAllProtoStrikesForKeys(keys_to_clear);
  EXPECT_EQ(0, GetProtoStrikes(key1));
  EXPECT_EQ(0, GetProtoStrikes(key2));
  // The strikes for the third key should not have been reset.
  EXPECT_EQ(3, GetProtoStrikes(key3));
}

TEST_F(StrikeDatabaseTest, ClearStrikesForMultipleNonZeroStrikesEntriesTest) {
  // Set up database with 3 pre-existing strikes at |key1|, and 5 pre-existing
  // strikes at |key2|.
  const std::string key1 = "12345";
  const std::string key2 = "13579";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data1;
  data1.set_num_strikes(3);
  entries.emplace_back(key1, data1);
  StrikeData data2;
  data2.set_num_strikes(5);
  entries.emplace_back(key2, data2);
  AddProtoEntries(entries);

  int strikes = GetProtoStrikes(key1);
  EXPECT_EQ(3, strikes);
  strikes = GetProtoStrikes(key2);
  EXPECT_EQ(5, strikes);
  ClearAllProtoStrikesForKey(key1);
  strikes = GetProtoStrikes(key1);
  EXPECT_EQ(0, strikes);
  strikes = GetProtoStrikes(key2);
  EXPECT_EQ(5, strikes);
}

TEST_F(StrikeDatabaseTest, ClearAllProtoStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key1|, and 5 pre-existing
  // strikes at |key2|.
  const std::string key1 = "12345";
  const std::string key2 = "13579";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data1;
  data1.set_num_strikes(3);
  entries.push_back(std::make_pair(key1, data1));
  StrikeData data2;
  data2.set_num_strikes(5);
  entries.push_back(std::make_pair(key2, data2));
  AddProtoEntries(entries);

  EXPECT_EQ(3, GetProtoStrikes(key1));
  EXPECT_EQ(5, GetProtoStrikes(key2));
  ClearAllProtoStrikes();
  EXPECT_EQ(0, GetProtoStrikes(key1));
  EXPECT_EQ(0, GetProtoStrikes(key2));
}

TEST_F(StrikeDatabaseTest, GetAllStrikeKeysForProject) {
  const std::string key1 = "project_12345";
  const std::string key2 = "project_13579";
  const std::string key3 = "otherproject_13579";
  strike_database_->AddStrikes(1, key1);
  strike_database_->AddStrikes(2, key2);
  strike_database_->AddStrikes(2, key3);
  std::vector<std::string> expected_keys({key1, key2});
  EXPECT_EQ(strike_database_->GetAllStrikeKeysForProject("project"),
            expected_keys);
  expected_keys = {key3};
  EXPECT_EQ(strike_database_->GetAllStrikeKeysForProject("otherproject"),
            expected_keys);
  ClearAllProtoStrikes();
}

TEST_F(StrikeDatabaseTest, ClearStrikesForKeys) {
  const std::string key1 = "project_12345";
  const std::string key2 = "project_13579";
  const std::string key3 = "otherproject_13579";
  strike_database_->AddStrikes(1, key1);
  strike_database_->AddStrikes(2, key2);
  strike_database_->AddStrikes(2, key3);
  strike_database_->ClearStrikesForKeys(std::vector<std::string>({key1, key2}));
  std::vector<std::string> expected_keys({});
  EXPECT_EQ(strike_database_->GetAllStrikeKeysForProject("project"),
            expected_keys);
  expected_keys.emplace_back(key3);
  EXPECT_EQ(strike_database_->GetAllStrikeKeysForProject("otherproject"),
            expected_keys);
  ClearAllProtoStrikes();
}

// Test to ensure that the timestamp of strike being added is logged and
// retrieved correctly.
TEST_F(StrikeDatabaseTest, LastUpdateTimestamp) {
  strike_database_->AddStrikes(1, "fake key");
  base::Time strike_added_timestamp =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
          strike_database_->GetLastUpdatedTimestamp("fake key")));
  EXPECT_FALSE(strike_added_timestamp.is_null());
  task_environment_.AdvanceClock(base::Microseconds(5));
  strike_database_->AddStrikes(1, "fake key");
  EXPECT_LT(strike_added_timestamp,
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
                strike_database_->GetLastUpdatedTimestamp("fake key"))));
  ClearAllProtoStrikes();
}

}  // namespace autofill
