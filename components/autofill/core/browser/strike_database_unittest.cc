// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Note: This class is NOT the same as test_strike_database.h. This is an actual
// implementation of StrikeDatabase, but with helper functions added for easier
// test setup.
class TestStrikeDatabase : public StrikeDatabase {
 public:
  TestStrikeDatabase(const base::FilePath& database_dir)
      : StrikeDatabase(database_dir) {}

  void AddEntries(
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStrikeDatabase);
};

}  // anonymous namespace

// Runs tests against the actual StrikeDatabase class, complete with
// ProtoDatabase.
class StrikeDatabaseTest : public ::testing::Test {
 public:
  StrikeDatabaseTest() : strike_database_(InitFilePath()) {}

  void AddEntries(
      std::vector<std::pair<std::string, StrikeData>> entries_to_add) {
    base::RunLoop run_loop;
    strike_database_.AddEntries(
        entries_to_add,
        base::BindRepeating(&StrikeDatabaseTest::OnAddEntries,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnAddEntries(base::RepeatingClosure run_loop_closure, bool success) {
    run_loop_closure.Run();
  }

  void OnGetStrikes(base::RepeatingClosure run_loop_closure, int num_strikes) {
    num_strikes_ = num_strikes;
    run_loop_closure.Run();
  }

  int GetStrikes(std::string key) {
    base::RunLoop run_loop;
    strike_database_.GetStrikes(
        key,
        base::BindRepeating(&StrikeDatabaseTest::OnGetStrikes,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return num_strikes_;
  }

  void OnAddStrike(base::RepeatingClosure run_loop_closure, int num_strikes) {
    num_strikes_ = num_strikes;
    run_loop_closure.Run();
  }

  int AddStrike(std::string key) {
    base::RunLoop run_loop;
    strike_database_.AddStrike(
        key,
        base::BindRepeating(&StrikeDatabaseTest::OnAddStrike,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return num_strikes_;
  }

  void OnClearAllStrikesForKey(base::RepeatingClosure run_loop_closure,
                               bool success) {
    run_loop_closure.Run();
  }

  void ClearAllStrikesForKey(const std::string key) {
    base::RunLoop run_loop;
    strike_database_.ClearAllStrikesForKey(
        key,
        base::BindRepeating(&StrikeDatabaseTest::OnClearAllStrikesForKey,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnClearAllStrikes(base::RepeatingClosure run_loop_closure,
                         bool success) {
    run_loop_closure.Run();
  }

  void ClearAllStrikes() {
    base::RunLoop run_loop;
    strike_database_.ClearAllStrikes(
        base::BindRepeating(&StrikeDatabaseTest::OnClearAllStrikesForKey,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestStrikeDatabase strike_database_;

 private:
  static const base::FilePath InitFilePath() {
    base::ScopedTempDir temp_dir_;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath file_path =
        temp_dir_.GetPath().AppendASCII("StrikeDatabaseTest");
    return file_path;
  }

  base::HistogramTester histogram_tester_;
  int num_strikes_;
  std::unique_ptr<StrikeData> strike_data_;
};

TEST_F(StrikeDatabaseTest, AddStrikeTest) {
  const std::string key = "12345";
  int strikes = AddStrike(key);
  EXPECT_EQ(1, strikes);
  strikes = AddStrike(key);
  EXPECT_EQ(2, strikes);
}

TEST_F(StrikeDatabaseTest, GetStrikeForZeroStrikesTest) {
  const std::string key = "12345";
  int strikes = GetStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, GetStrikeForNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key|.
  const std::string key = "12345";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.push_back(std::make_pair(key, data));
  AddEntries(entries);

  int strikes = GetStrikes(key);
  EXPECT_EQ(3, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForZeroStrikesTest) {
  const std::string key = "12345";
  ClearAllStrikesForKey(key);
  int strikes = GetStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key|.
  const std::string key = "12345";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.push_back(std::make_pair(key, data));
  AddEntries(entries);

  int strikes = GetStrikes(key);
  EXPECT_EQ(3, strikes);
  ClearAllStrikesForKey(key);
  strikes = GetStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForMultipleNonZeroStrikesEntriesTest) {
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
  AddEntries(entries);

  int strikes = GetStrikes(key1);
  EXPECT_EQ(3, strikes);
  strikes = GetStrikes(key2);
  EXPECT_EQ(5, strikes);
  ClearAllStrikesForKey(key1);
  strikes = GetStrikes(key1);
  EXPECT_EQ(0, strikes);
  strikes = GetStrikes(key2);
  EXPECT_EQ(5, strikes);
}

TEST_F(StrikeDatabaseTest, ClearAllStrikesTest) {
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
  AddEntries(entries);

  EXPECT_EQ(3, GetStrikes(key1));
  EXPECT_EQ(5, GetStrikes(key2));
  ClearAllStrikes();
  EXPECT_EQ(0, GetStrikes(key1));
  EXPECT_EQ(0, GetStrikes(key2));
}

TEST_F(StrikeDatabaseTest, GetKeyForCreditCardSave) {
  const std::string last_four = "1234";
  EXPECT_EQ("creditCardSave__1234",
            strike_database_.GetKeyForCreditCardSave(last_four));
}

TEST_F(StrikeDatabaseTest, GetPrefixFromKey) {
  const std::string key = "creditCardSave__1234";
  EXPECT_EQ("creditCardSave", strike_database_.GetPrefixFromKey(key));
}

TEST_F(StrikeDatabaseTest, CreditCardSaveNthStrikeAddedHistogram) {
  const std::string last_four1 = "1234";
  const std::string last_four2 = "9876";
  const std::string key1 = "NotACreditCard";
  // 1st strike added for |last_four1|.
  AddStrike(strike_database_.GetKeyForCreditCardSave(last_four1));
  // 2nd strike added for |last_four1|.
  AddStrike(strike_database_.GetKeyForCreditCardSave(last_four1));
  // 1st strike added for |last_four2|.
  AddStrike(strike_database_.GetKeyForCreditCardSave(last_four2));
  // Shouldn't be counted in histogram since key doesn't have prefix for credit
  // cards.
  AddStrike(key1);
  std::vector<base::Bucket> buckets = GetHistogramTester()->GetAllSamples(
      "Autofill.StrikeDatabase.NthStrikeAdded.CreditCardSave");
  // There should be two buckets, one for 1st strike, one for 2nd strike count.
  ASSERT_EQ(2U, buckets.size());
  // Both |last_four1| and |last_four2| have 1st strikes recorded.
  EXPECT_EQ(2, buckets[0].count);
  // Only |last_four1| has 2nd strike recorded.
  EXPECT_EQ(1, buckets[1].count);
}

}  // namespace autofill
