// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/perf_time_logger.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_database_impl.h"
#include "components/leveldb_proto/internal/unique_proto_database.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using base::ScopedTempDir;
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

using KeyEntryVector = ProtoDatabase<TestProto>::KeyEntryVector;
using KeyEntryVectorMap =
    std::map<std::string, std::unique_ptr<KeyEntryVector>>;

struct TestParams {
  int num_entries;
  int data_size;
  int batch_size;
  bool single_db;
};

struct PerfStats {
  double time_ms = 0;
  uint64_t max_memory_used_bytes = 0;
  uint64_t memory_summed_bytes = 0;
  int num_runs = 0;
};

static const std::string kSingleDBName = "singledb";
static constexpr char kMetricNumRunsCount[] = "num_runs";
static constexpr char kMetricTimeMs[] = "time";
static constexpr char kMetricMaxMemoryUseBytes[] = "max_memory_use";
static constexpr char kMetricAverageMemoryUseBytes[] = "average_memory_use";
static constexpr char kMetricTotalMemoryUseBytes[] = "total_memory_use";
static constexpr char kMetricMemUseAfterWritesBytes[] =
    "memory_use_after_writes";
static constexpr char kMetricMemUseAfterLoadBytes[] = "memory_use_after_load";
static constexpr char kMetricTotalTimeTakenMs[] = "total_time_taken";
static constexpr char kMetricMaxIndTimeTakenMs[] = "max_individual_time_taken";

static const int kSmallDataSize = 10;
static const int kMediumDataSize = 100;
static const int kLargeDataSize = 1000;

static const int kDefaultNumDBs = 5;
static const int kSmallNumEntries = 300;
static const int kLargeNumEntries = 3000;

static const std::vector<TestParams> kFewEntriesDistributionTestParams = {
    {2, kSmallDataSize, 1, false},  {1, kSmallDataSize, 1, false},
    {1, kSmallDataSize, 1, false},  {3, kSmallDataSize, 1, false},
    {4, kSmallDataSize, 1, false},  {5, kSmallDataSize, 1, false},
    {8, kSmallDataSize, 1, false},  {10, kSmallDataSize, 1, false},
    {10, kSmallDataSize, 1, false},
};

static const std::vector<TestParams> kManyEntriesDistributionTestParams = {
    {20, kSmallDataSize, 1, false},  {10, kSmallDataSize, 1, false},
    {10, kSmallDataSize, 1, false},  {30, kSmallDataSize, 1, false},
    {40, kSmallDataSize, 1, false},  {50, kSmallDataSize, 1, false},
    {80, kSmallDataSize, 1, false},  {100, kSmallDataSize, 1, false},
    {100, kSmallDataSize, 1, false},
};

class TestDatabase {
 public:
  TestDatabase(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               const base::FilePath& path)
      : db_(std::make_unique<ProtoDatabaseImpl<TestProto>>(
            ProtoDbType::TEST_DATABASE0,
            path,
            task_runner)) {
    base::RunLoop run_init_db;
    db_->Init(base::BindOnce(
        [](base::OnceClosure signal, Enums::InitStatus status) {
          bool success = status == Enums::kOK;
          EXPECT_TRUE(success);
          std::move(signal).Run();
        },
        run_init_db.QuitClosure()));
    run_init_db.Run();

    is_initialized_ = true;
  }

  bool is_initialized() const { return is_initialized_; }
  ProtoDatabaseImpl<TestProto>* proto_db() const { return db_.get(); }

 private:
  bool is_initialized_ = false;
  std::unique_ptr<ProtoDatabaseImpl<TestProto>> db_;
};

}  // namespace

class ProtoDBPerfTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  void TearDown() override {
    ShutdownDBs();
  }

  void ShutdownDBs() {
    dbs_.clear();
    base::RunLoop().RunUntilIdle();
    PruneBlockCache();
    uint64_t mem;
    GetApproximateMemoryUsage(&mem);
    ASSERT_EQ(mem, 0U);
  }

  void GetDatabase(const std::string& name, TestDatabase** db) {
    auto db_it = dbs_.find(name);
    ASSERT_FALSE(db_it == dbs_.end());
    *db = db_it->second.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  // Initializes a DB named |name| in a dedicated directory. The same directory
  // will be used for all instances created for the same |name| for the lifetime
  // of the test.
  void InitDB(const std::string& name) {
    if (!base::Contains(temp_dirs_, name)) {
      auto temp_dir = std::make_unique<ScopedTempDir>();
      EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
      temp_dirs_[name] = std::move(temp_dir);
    }
    auto db = std::make_unique<TestDatabase>(task_runner_,
                                             temp_dirs_[name]->GetPath());
    EXPECT_TRUE(db->is_initialized());
    dbs_[name] = std::move(db);
  }

  void InsertSuccessCallback(base::OnceClosure signal,
                             uint64_t* memory_use,
                             bool success) {
    EXPECT_TRUE(success);
    GetApproximateMemoryUsage(memory_use);
    std::move(signal).Run();
  }

  void InsertEntries(const std::string& db_name,
                     std::unique_ptr<KeyEntryVector> entries,
                     PerfStats* stats) {
    auto db_it = dbs_.find(db_name);
    ASSERT_TRUE(db_it != dbs_.end());
    TestDatabase* db = db_it->second.get();

    base::RunLoop run_update_entries;
    base::ElapsedTimer timer;
    uint64_t memory_use;
    db->proto_db()->UpdateEntries(
        std::move(entries), std::make_unique<std::vector<std::string>>(),
        base::BindOnce(&ProtoDBPerfTest::InsertSuccessCallback,
                       base::Unretained(this), run_update_entries.QuitClosure(),
                       &memory_use));
    run_update_entries.Run();
    stats->max_memory_used_bytes =
        std::max(stats->max_memory_used_bytes, memory_use);
    stats->memory_summed_bytes += memory_use;
    stats->time_ms += timer.Elapsed().InMillisecondsF();
    stats->num_runs++;
  }

  std::vector<std::string> GenerateDBNames(int count) {
    std::vector<std::string> names;
    for (int i = 0; i < count; i++) {
      names.push_back(base::StringPrintf("test%03d_", i));
    }
    return names;
  }

  // Since we don't have access to the internal memtables of the individual DBs,
  // but we do have access to the global shared block cache, we always subtract
  // the block cache size from each individual estimate and then add it at the
  // end.
  void GetApproximateMemoryUsage(uint64_t* approx_mem_use) {
    uint64_t total_usage = 0;
    uint64_t block_cache_size =
        leveldb_chrome::GetSharedBrowserBlockCache()->TotalCharge();
    for (auto it = dbs_.begin(); it != dbs_.end(); ++it) {
      uint64_t usage;
      ASSERT_TRUE(GetApproximateMemoryUsageOfDB(it->second.get(), &usage));
      total_usage += usage - block_cache_size;
    }
    *approx_mem_use = total_usage + block_cache_size;
  }

  void RunAlternatingInsertTests(const std::vector<TestParams>& params,
                                 const std::string& test_name,
                                 int num_dbs) {
    auto db_names = GenerateDBNames(num_dbs);

    for (auto& p : params) {
      std::string test_modifier = base::StringPrintf(
          "%d_%d_%d", p.batch_size, p.num_entries, p.data_size);
      RunAlternatingInsertTest(test_name, test_modifier, db_names, p);
    }
  }

  // Runs a test to alternately insert elements with different prefixes into
  // either a database for each prefix or a single DB.
  void RunAlternatingInsertTest(const std::string& test_name,
                                const std::string& story_name,
                                const std::vector<std::string>& prefixes,
                                const TestParams& params) {
    // Make the entries for each database first.
    KeyEntryVectorMap entries =
        GenerateTestEntries(prefixes, params.num_entries, params.data_size);

    InitDBs(params.single_db, prefixes);

    int remaining = params.num_entries;
    PerfStats stats;
    while (remaining > 0) {
      int begin_index = params.num_entries - remaining;
      int end_index =
          std::min(begin_index + params.batch_size, params.num_entries);

      for (auto& prefix : prefixes) {
        auto db_name = params.single_db ? kSingleDBName : prefix;

        auto begin_it = entries[prefix]->begin() + begin_index;
        auto end_it = entries[prefix]->begin() + end_index;
        auto batch_entries = std::make_unique<KeyEntryVector>(begin_it, end_it);

        InsertEntries(db_name, std::move(batch_entries), &stats);
      }
      remaining -= params.batch_size;
    }

    perf_test::PerfResultReporter reporter =
        SetUpReporter(test_name, story_name);
    reporter.AddResult(kMetricNumRunsCount,
                       static_cast<size_t>(stats.num_runs));
    reporter.AddResult(kMetricTimeMs, stats.time_ms);
    reporter.AddResult(kMetricMaxMemoryUseBytes,
                       static_cast<size_t>(stats.max_memory_used_bytes));
    uint64_t average_memory_use = stats.memory_summed_bytes / stats.num_runs;
    reporter.AddResult(kMetricAverageMemoryUseBytes,
                       static_cast<size_t>(average_memory_use));
  }

  // Used to measure the impact on memory in the case where the distribution of
  // entries isn't equal amongst individual databases.
  void RunDistributionTestAndCleanup(const std::string& story_name,
                                     const std::vector<TestParams>& test_params,
                                     bool single_db) {
    std::vector<std::string> prefixes;
    for (int i = 0; i < static_cast<int>(test_params.size()); i++) {
      prefixes.emplace_back(base::StringPrintf("test%03d_", i));
    }

    InitDBs(single_db, prefixes);

    PerfStats stats;
    for (int i = 0; i < static_cast<int>(test_params.size()); i++) {
      auto entries = GenerateTestEntries(
          prefixes[i], test_params[i].num_entries, test_params[i].data_size);
      auto db_name = single_db ? kSingleDBName : prefixes[i];

      int remaining = test_params[i].num_entries;
      while (remaining > 0) {
        int begin_index = test_params[i].num_entries - remaining;
        int end_index = std::min(begin_index + test_params[i].batch_size,
                                 test_params[i].num_entries);

        auto begin_it = entries->begin() + begin_index;
        auto end_it = entries->begin() + end_index;
        auto batch_entries = std::make_unique<KeyEntryVector>(begin_it, end_it);

        InsertEntries(db_name, std::move(batch_entries), &stats);
        remaining -= test_params[i].batch_size;
      }
    }

    perf_test::PerfResultReporter reporter =
        SetUpReporter("Distribution", story_name);
    reporter.AddResult(kMetricTotalMemoryUseBytes,
                       static_cast<size_t>(stats.max_memory_used_bytes));
    ShutdownDBs();
  }

  void RunLoadEntriesSingleTestAndCleanup(unsigned int num_dbs,
                                          unsigned int num_entries,
                                          size_t data_size,
                                          std::set<unsigned int> dbs_to_load,
                                          std::string test_modifier,
                                          unsigned int* num_entries_loaded,
                                          bool fill_read_cache = true) {
    std::vector<std::string> prefixes = GenerateDBNames(num_dbs);
    PrefillDatabase(kSingleDBName, prefixes, num_entries, data_size);
    uint64_t memory_use_before;
    GetApproximateMemoryUsage(&memory_use_before);

    ShutdownDBs();

    InitDB(kSingleDBName);
    TestDatabase* db;
    GetDatabase(kSingleDBName, &db);

    uint64_t time_ms = 0;
    uint64_t max_time_ms = 0;
    if (dbs_to_load.size() == 0) {
      // The case where we just load all the DBs, so we give it an empty prefix.
      LoadEntriesFromDB(db, "", fill_read_cache, num_entries_loaded, &time_ms);
    } else {
      for (auto& db_to_load : dbs_to_load) {
        unsigned int curr_num_entries_loaded = 0;
        uint64_t curr_time_ms = 0;
        LoadEntriesFromDB(db, base::StringPrintf("test%03u_", db_to_load),
                          fill_read_cache, &curr_num_entries_loaded,
                          &curr_time_ms);
        *num_entries_loaded += curr_num_entries_loaded;
        max_time_ms = std::max(max_time_ms, curr_time_ms);
        time_ms += curr_time_ms;
      }
    }
    uint64_t memory_use_after;
    GetApproximateMemoryUsage(&memory_use_after);

    auto story_name =
        base::StringPrintf("%u_%u_%zu", num_dbs, num_entries, data_size);
    perf_test::PerfResultReporter reporter =
        SetUpReporter(test_modifier, story_name);
    reporter.AddResult(kMetricMemUseAfterWritesBytes,
                       static_cast<size_t>(memory_use_before));
    reporter.AddResult(kMetricMemUseAfterLoadBytes,
                       static_cast<size_t>(memory_use_after));
    reporter.AddResult(kMetricTotalTimeTakenMs, static_cast<size_t>(time_ms));
    reporter.AddResult(kMetricMaxIndTimeTakenMs,
                       static_cast<size_t>(max_time_ms));

    ShutdownDBs();
  }

  void RunLoadEntriesMultiTestAndCleanup(unsigned int num_dbs,
                                         unsigned int num_entries,
                                         size_t data_size,
                                         std::set<unsigned int> dbs_to_load,
                                         std::string test_modifier,
                                         unsigned int* num_entries_loaded,
                                         bool fill_read_cache = true) {
    std::vector<std::string> prefixes = GenerateDBNames(num_dbs);
    for (unsigned int i = 0; i < num_dbs; i++) {
      std::vector<std::string> single_prefix = {prefixes[i]};
      PrefillDatabase(prefixes[i], single_prefix, num_entries, data_size);
    }
    uint64_t memory_use_before;
    GetApproximateMemoryUsage(&memory_use_before);

    ShutdownDBs();

    uint64_t time_ms = 0;
    uint64_t max_time_ms = 0;
    for (unsigned int i = 0; i < num_dbs; i++) {
      if (dbs_to_load.size() > 0 && dbs_to_load.find(i) == dbs_to_load.end())
        continue;
      InitDB(prefixes[i]);
      TestDatabase* db;
      GetDatabase(prefixes[i], &db);

      unsigned int curr_num_entries_loaded = 0;
      uint64_t curr_time_ms = 0;
      LoadEntriesFromDB(db, std::string(), fill_read_cache,
                        &curr_num_entries_loaded, &curr_time_ms);
      *num_entries_loaded += curr_num_entries_loaded;
      max_time_ms = std::max(max_time_ms, curr_time_ms);
      time_ms += curr_time_ms;
    }

    uint64_t memory_use_after;
    GetApproximateMemoryUsage(&memory_use_after);
    auto story_name =
        base::StringPrintf("%u_%u_%zu", num_dbs, num_entries, data_size);
    perf_test::PerfResultReporter reporter =
        SetUpReporter(test_modifier, story_name);
    reporter.AddResult(kMetricMemUseAfterWritesBytes,
                       static_cast<size_t>(memory_use_before));
    reporter.AddResult(kMetricMemUseAfterLoadBytes,
                       static_cast<size_t>(memory_use_after));
    reporter.AddResult(kMetricTotalTimeTakenMs, static_cast<size_t>(time_ms));
    reporter.AddResult(kMetricMaxIndTimeTakenMs,
                       static_cast<size_t>(max_time_ms));

    ShutdownDBs();
  }

  void InitDBs(bool single_db, const std::vector<std::string>& prefixes) {
    if (single_db) {
      InitDB(kSingleDBName);
    } else {
      for (auto& prefix : prefixes) {
        InitDB(prefix);
      }
    }
  }

  PerfStats PrefillDatabase(const std::string& name,
                            std::vector<std::string>& prefixes,
                            int num_entries,
                            int data_size) {
    InitDB(name);

    auto entries = GenerateTestEntries(prefixes, num_entries, data_size);
    PerfStats stats;
    for (auto& prefix : prefixes) {
      InsertEntries(name, std::move(entries[prefix]), &stats);
    }
    return stats;
  }

  PerfStats CombinePerfStats(const PerfStats& a, const PerfStats& b) {
    PerfStats out;
    out.time_ms = a.time_ms + b.time_ms;
    out.num_runs = a.num_runs + b.num_runs;
    out.memory_summed_bytes = a.memory_summed_bytes + b.memory_summed_bytes;
    out.max_memory_used_bytes =
        std::max(a.max_memory_used_bytes, b.max_memory_used_bytes);
    return out;
  }

 private:
  void PruneBlockCache() {
    leveldb_chrome::GetSharedBrowserBlockCache()->Prune();
  }

  KeyEntryVectorMap GenerateTestEntries(
      const std::vector<std::string>& prefixes,
      int num_entries,
      int data_size) {
    KeyEntryVectorMap entries;
    for (const auto& prefix : prefixes) {
      entries[prefix] = GenerateTestEntries(prefix, num_entries, data_size);
    }
    return entries;
  }

  std::unique_ptr<KeyEntryVector> GenerateTestEntries(const std::string& prefix,
                                                      int num_entries,
                                                      int data_size) {
    auto entries = std::make_unique<KeyEntryVector>();
    AddEntriesToVector(prefix, entries.get(), 0, num_entries, data_size);
    return entries;
  }

  void LoadEntriesFromDB(TestDatabase* db,
                         std::string prefix,
                         bool fill_read_cache,
                         unsigned int* num_entries_loaded,
                         uint64_t* time_ms) {
    base::ElapsedTimer timer;
    base::RunLoop run_load_entries;
    leveldb::ReadOptions options;
    options.fill_cache = fill_read_cache;
    db->proto_db()->LoadEntriesWithFilter(
        KeyFilter(), options, prefix,
        base::BindOnce(
            [](base::OnceClosure signal, unsigned int* num_entries_loaded,
               bool success, std::unique_ptr<std::vector<TestProto>> entries) {
              EXPECT_TRUE(success);
              *num_entries_loaded = entries->size();
              std::move(signal).Run();
            },
            run_load_entries.QuitClosure(), num_entries_loaded));
    run_load_entries.Run();
    *time_ms = timer.Elapsed().InMilliseconds();
  }

  void AddEntriesToVector(const std::string& prefix,
                          KeyEntryVector* entries,
                          int start_id,
                          int num,
                          int data_size) {
    for (int i = 0; i < num; i++) {
      char data[data_size];
      std::fill_n(data, data_size - 1, ':');
      data[data_size - 1] = '\0';
      std::string entry_name =
          base::StringPrintf("%s_entry_%d", prefix.c_str(), (start_id + i));
      TestProto proto;
      proto.set_id(entry_name);
      proto.set_data(std::string(data));
      entries->emplace_back(std::make_pair(entry_name, std::move(proto)));
    }
  }

  bool GetApproximateMemoryUsageOfDB(TestDatabase* db, uint64_t* memory_use) {
    return db->proto_db()
        ->db_wrapper_for_testing()
        ->db_for_testing()
        ->GetApproximateMemoryUse(memory_use);
  }

  perf_test::PerfResultReporter SetUpReporter(const std::string& test_type,
                                              const std::string& story_name) {
    perf_test::PerfResultReporter reporter("ProtoDB_" + test_type + ".",
                                           story_name);
    reporter.RegisterImportantMetric(kMetricNumRunsCount, "count");
    reporter.RegisterImportantMetric(kMetricTimeMs, "ms");
    reporter.RegisterImportantMetric(kMetricMaxMemoryUseBytes, "bytes");
    reporter.RegisterImportantMetric(kMetricAverageMemoryUseBytes, "bytes");
    reporter.RegisterImportantMetric(kMetricTotalMemoryUseBytes, "bytes");
    reporter.RegisterImportantMetric(kMetricMemUseAfterWritesBytes, "bytes");
    reporter.RegisterImportantMetric(kMetricMemUseAfterLoadBytes, "bytes");
    reporter.RegisterImportantMetric(kMetricTotalTimeTakenMs, "ms");
    reporter.RegisterImportantMetric(kMetricMaxIndTimeTakenMs, "ms");
    return reporter;
  }

  std::map<std::string, std::unique_ptr<ScopedTempDir>> temp_dirs_;
  std::map<std::string, std::unique_ptr<TestDatabase>> dbs_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

// Flakily times out on Windows and Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InsertMultipleDBsAlternating_Individual_100b \
  DISABLED_InsertMultipleDBsAlternating_Individual_100b
#else
#define MAYBE_InsertMultipleDBsAlternating_Individual_100b \
  InsertMultipleDBsAlternating_Individual_100b
#endif
TEST_F(ProtoDBPerfTest, MAYBE_InsertMultipleDBsAlternating_Individual_100b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kMediumDataSize, 1, false};
  RunAlternatingInsertTests({params}, "InsertMultipleDBsAlternating_Individual",
                            5);
}

// Flakily times out on Windows and Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InsertMultipleDBsAlternating_Individual_1000b \
  DISABLED_InsertMultipleDBsAlternating_Individual_1000b
#else
#define MAYBE_InsertMultipleDBsAlternating_Individual_1000b \
  InsertMultipleDBsAlternating_Individual_1000b
#endif
TEST_F(ProtoDBPerfTest, MAYBE_InsertMultipleDBsAlternating_Individual_1000b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kLargeDataSize, 1, false};
  RunAlternatingInsertTests({params}, "InsertMultipleDBsAlternating_Individual",
                            5);
}

// Flakily times out on Windows and Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InsertSingleDBAlternating_Individual_100b \
  DISABLED_InsertSingleDBAlternating_Individual_100b
#else
#define MAYBE_InsertSingleDBAlternating_Individual_100b \
  InsertSingleDBAlternating_Individual_100b
#endif
TEST_F(ProtoDBPerfTest, MAYBE_InsertSingleDBAlternating_Individual_100b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kMediumDataSize, 1, true};
  RunAlternatingInsertTests({params}, "InsertSingleDBAlternating_Individual",
                            5);
}

// Flakily times out on Windows and Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_InsertSingleDBAlternating_Individual_1000b \
  DISABLED_InsertSingleDBAlternating_Individual_1000b
#else
#define MAYBE_InsertSingleDBAlternating_Individual_1000b \
  InsertSingleDBAlternating_Individual_1000b
#endif
TEST_F(ProtoDBPerfTest, MAYBE_InsertSingleDBAlternating_Individual_1000b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kLargeDataSize, 1, true};
  RunAlternatingInsertTests({params}, "InsertSingleDBAlternating_Individual",
                            5);
}

TEST_F(ProtoDBPerfTest, InsertMultipleDBsAlternating_LargeBatch_100b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kMediumDataSize, 200, false};
  RunAlternatingInsertTests({params}, "InsertMultipleDBsAlternating_LargeBatch",
                            5);
}

TEST_F(ProtoDBPerfTest, InsertMultipleDBsAlternating_LargeBatch_1000b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kLargeDataSize, 200, false};
  RunAlternatingInsertTests({params}, "InsertMultipleDBsAlternating_LargeBatch",
                            5);
}

TEST_F(ProtoDBPerfTest, InsertSingleDBAlternating_LargeBatch_100b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kMediumDataSize, 200, true};
  RunAlternatingInsertTests({params}, "InsertSingleDBAlternating_LargeBatch",
                            5);
}

TEST_F(ProtoDBPerfTest, InsertSingleDBAlternating_LargeBatch_1000b) {
  // num_entries, data_size, batch_size, single_db.
  TestParams params = {200, kLargeDataSize, 200, true};
  RunAlternatingInsertTests({params}, "InsertSingleDBAlternating_LargeBatch",
                            5);
}

TEST_F(ProtoDBPerfTest, DistributionTestSmall_FewEntries_Single) {
  RunDistributionTestAndCleanup("Small_FewEntries_Single",
                                kFewEntriesDistributionTestParams, true);
}

TEST_F(ProtoDBPerfTest, DistributionTestSmall_FewEntries_Multi) {
  RunDistributionTestAndCleanup("Small_FewEntries_Multi",
                                kFewEntriesDistributionTestParams, false);
}

// Flakily times out on Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DistributionTestSmall_ManyEntries_Single \
  DISABLED_DistributionTestSmall_ManyEntries_Single
#else
#define MAYBE_DistributionTestSmall_ManyEntries_Single \
  DistributionTestSmall_ManyEntries_Single
#endif
TEST_F(ProtoDBPerfTest, MAYBE_DistributionTestSmall_ManyEntries_Single) {
  RunDistributionTestAndCleanup("Small_ManyEntries_Single",
                                kManyEntriesDistributionTestParams, true);
}

// Flakily times out on Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DistributionTestSmall_ManyEntries_Multi \
  DISABLED_DistributionTestSmall_ManyEntries_Multi
#else
#define MAYBE_DistributionTestSmall_ManyEntries_Multi \
  DistributionTestSmall_ManyEntries_Multi
#endif
TEST_F(ProtoDBPerfTest, MAYBE_DistributionTestSmall_ManyEntries_Multi) {
  RunDistributionTestAndCleanup("Small_ManyEntries_Multi",
                                kManyEntriesDistributionTestParams, false);
}

// Flakily times out on Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DistributionTestSmall_ManyEntries_Batch_Single \
  DISABLED_DistributionTestSmall_ManyEntries_Batch_Single
#else
#define MAYBE_DistributionTestSmall_ManyEntries_Batch_Single \
  DistributionTestSmall_ManyEntries_Batch_Single
#endif
TEST_F(ProtoDBPerfTest, MAYBE_DistributionTestSmall_ManyEntries_Batch_Single) {
  RunDistributionTestAndCleanup("Small_ManyEntries_Batch_Single",
                                kManyEntriesDistributionTestParams, true);
}

// Flakily times out on Mac, see http://crbug.com/918874.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DistributionTestSmall_ManyEntries_Batch_Multi \
  DISABLED_DistributionTestSmall_ManyEntries_Batch_Multi
#else
#define MAYBE_DistributionTestSmall_ManyEntries_Batch_Multi \
  DistributionTestSmall_ManyEntries_Batch_Multi
#endif
TEST_F(ProtoDBPerfTest, MAYBE_DistributionTestSmall_ManyEntries_Batch_Multi) {
  RunDistributionTestAndCleanup("Small_ManyEntries_Batch_Multi",
                                kManyEntriesDistributionTestParams, false);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(kDefaultNumDBs, kSmallNumEntries,
                                     kSmallDataSize, {}, "LoadEntriesSingle",
                                     &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                     kSmallDataSize, {}, "LoadEntriesSingle",
                                     &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                     kMediumDataSize, {}, "LoadEntriesSingle",
                                     &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_Small) {
  // Load only the entries that start with a particular prefix.
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_Medium) {
  // Load only the entries that start with a particular prefix.
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kLargeNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_Large) {
  // Load only the entries that start with a particular prefix.
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kLargeNumEntries, kMediumDataSize, {1},
      "LoadEntriesSingle_OnePrefix", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kSmallNumEntries,
                                    kSmallDataSize, {}, "LoadEntriesMulti",
                                    &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kSmallDataSize, {}, "LoadEntriesMulti",
                                    &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kMediumDataSize, {}, "LoadEntriesMulti",
                                    &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kSmallNumEntries,
                                    kSmallDataSize, {1},
                                    "LoadEntriesMulti_OnePrefix", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kSmallDataSize, {1},
                                    "LoadEntriesMulti_OnePrefix", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kMediumDataSize, {1},
                                    "LoadEntriesMulti_OnePrefix", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(kDefaultNumDBs, kSmallNumEntries,
                                     kSmallDataSize, {},
                                     "LoadEntriesSingle_SkipReadCache",
                                     &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                     kSmallDataSize, {},
                                     "LoadEntriesSingle_SkipReadCache",
                                     &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                     kMediumDataSize, {},
                                     "LoadEntriesSingle_SkipReadCache",
                                     &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_SkipReadCache_Small) {
  // Load only the entries that start with a particular prefix.
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_SkipReadCache_Medium) {
  // Load only the entries that start with a particular prefix.
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kLargeNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_SkipReadCache_Large) {
  // Load only the entries that start with a particular prefix.
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kLargeNumEntries, kMediumDataSize, {1},
      "LoadEntriesSingle_OnePrefix_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kSmallNumEntries,
                                    kSmallDataSize, {},
                                    "LoadEntriesMulti_SkipReadCache",
                                    &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kSmallDataSize, {},
                                    "LoadEntriesMulti_SkipReadCache",
                                    &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kMediumDataSize, {},
                                    "LoadEntriesMulti_SkipReadCache",
                                    &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kSmallNumEntries,
                                    kSmallDataSize, {1},
                                    "LoadEntriesMulti_OnePrefix_SkipReadCache",
                                    &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kSmallDataSize, {1},
                                    "LoadEntriesMulti_OnePrefix_SkipReadCache",
                                    &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(kDefaultNumDBs, kLargeNumEntries,
                                    kMediumDataSize, {1},
                                    "LoadEntriesMulti_OnePrefix_SkipReadCache",
                                    &num_entries, false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_DifferingNumDBs_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_DifferingNumDBs_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_OnePrefix_DifferingNumDBs_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_DifferingNumDBs_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesMulti_OnePrefix_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_DifferingNumDBs_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesMulti_OnePrefix_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_OnePrefix_DifferingNumDBs_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesMulti_OnePrefix_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesSingle_OnePrefix_DifferingNumDBs_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesSingle_OnePrefix_DifferingNumDBs_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesSingle_OnePrefix_DifferingNumDBs_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesSingle_OnePrefix_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesMulti_OnePrefix_DifferingNumDBs_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesMulti_OnePrefix_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesMulti_OnePrefix_DifferingNumDBs_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesMulti_OnePrefix_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesMulti_OnePrefix_DifferingNumDBs_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {1},
      "LoadEntriesMulti_OnePrefix_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_DifferingNumDBs_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesSingle_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_DifferingNumDBs_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesSingle_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_DifferingNumDBs_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesSingle_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_DifferingNumDBs_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesMulti_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_DifferingNumDBs_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesMulti_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_DifferingNumDBs_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesMulti_DifferingNumDBs", &num_entries);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_DifferingNumDBs_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesSingle_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest,
       LoadEntriesSingle_DifferingNumDBs_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesSingle_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesSingle_DifferingNumDBs_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesSingleTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesSingle_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_DifferingNumDBs_SkipReadCache_Small) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesMulti_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_DifferingNumDBs_SkipReadCache_Medium) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 2, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesMulti_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

TEST_F(ProtoDBPerfTest, LoadEntriesMulti_DifferingNumDBs_SkipReadCache_Large) {
  unsigned int num_entries = 0;
  RunLoadEntriesMultiTestAndCleanup(
      kDefaultNumDBs * 4, kSmallNumEntries, kSmallDataSize, {},
      "LoadEntriesMulti_DifferingNumDBs_SkipReadCache", &num_entries,
      false /* fill_read_cache */);
  ASSERT_NE(num_entries, 0U);
}

}  // namespace leveldb_proto
