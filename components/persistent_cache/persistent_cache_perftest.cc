// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace persistent_cache {

// The variations of cache options available for creating/testing
// PersistentCache.
enum class CacheOption {
  kMultipleConnections,
  kSingleConnection,
  kJournalModeWal,
};

// A printer for `CacheOption`; used by GoogleTest for more friendly output and
// to suffix the story name for performance measurements.
void PrintTo(CacheOption cache_option, std::ostream* os) {
  switch (cache_option) {
    case CacheOption::kMultipleConnections:
      *os << "MultipleConnections";
      break;
    case CacheOption::kSingleConnection:
      *os << "SingleConnection";
      break;
    case CacheOption::kJournalModeWal:
      *os << "JournalModeWal";
      break;
  }
}

// A test harness parameterized on the options for creating a PersistentCache.
class PersistentCachePerftest : public testing::TestWithParam<CacheOption> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_storage_.emplace(Client::kTest, BackendType::kSqlite,
                             temp_dir_.GetPath());
  }

  // Returns a new cache configured according to the test's parameter.
  std::unique_ptr<PersistentCache> MakeCache() {
    switch (GetParam()) {
      case CacheOption::kMultipleConnections:
        return CreateCache(/*single_connection=*/false,
                           /*journal_mode_wal=*/false);
      case CacheOption::kSingleConnection:
        return CreateCache(/*single_connection=*/true,
                           /*journal_mode_wal=*/false);
      case CacheOption::kJournalModeWal:
        return CreateCache(/*single_connection=*/true,
                           /*journal_mode_wal=*/true);
    }
  }

  // Returns a new cache with the given options.
  std::unique_ptr<PersistentCache> CreateCache(bool single_connection,
                                               bool journal_mode_wal) {
    if (auto pending_backend = backend_storage_->MakePendingBackend(
            base::FilePath(kBaseName), single_connection, journal_mode_wal);
        pending_backend.has_value()) {
      return PersistentCache::Bind(Client::kTest, *std::move(pending_backend));
    }
    ADD_FAILURE() << "Failed to make PendingBackend";
    return nullptr;
  }

  // Returns true if caches created in this configuration can be shared across
  // multiple connections.
  static bool CanShareConnections() {
    return GetParam() == CacheOption::kMultipleConnections;
  }

  std::optional<PendingBackend> ShareReadWriteConnection(
      PersistentCache& cache) {
    return backend_storage_->ShareReadWriteConnection(base::FilePath(kBaseName),
                                                      cache);
  }

  void RunAndTimeTest(std::string_view operation_name,
                      int iteration_count,
                      base::FunctionRef<void()> test_body) {
    base::AutoReset<bool> resetter(&under_measurment_, true);
    base::ElapsedTimer elapsed_timer;
    base::ElapsedThreadTimer elapsed_thread_timer;

    test_body();

    ReportMeasurment(
        base::StrCat({operation_name, testing::PrintToString(GetParam())}),
        iteration_count, elapsed_timer.Elapsed(),
        elapsed_thread_timer.Elapsed());
  }

  // Pregenerates keys. Use to avoid timing allocation overhead.
  std::vector<std::string> GenerateKeys(int iteration_count) {
    CHECK(!under_measurment_);

    std::vector<std::string> keys(iteration_count);
    std::generate(keys.begin(), keys.end(),
                  [i = 0]() mutable { return base::NumberToString(i++); });
    return keys;
  }

  // Generates a value buffer to be inserted according to params. Should be done
  // outside of timing to avoid measuring overhead.
  base::HeapArray<uint8_t> MakeValue() {
    CHECK(!under_measurment_);

    // Median size of entries for a use case of PersistentCache as reported by
    // UMA on November 7th 2025.
    static constexpr size_t kValueSize = 6958;
    auto value = base::HeapArray<uint8_t>::Uninit(kValueSize);

    // Fill the data with random bytes to avoid unknown optimizations for
    // identical pages.
    base::RandBytes(value);
    return value;
  }

  // Returns true if this platform has expensive database commits.
  static bool HasExpensiveCommits() {
#if BUILDFLAG(IS_MAC)
    // Commits are slow on macOS 12. Speculation: perhaps it does not benefit
    // from F_BARRIERFSYNC.
    return base::mac::MacOSMajorVersion() < 13;
#elif BUILDFLAG(IS_WIN)
    return true;
#else
    // Android and other POSIX systems appear to benefit from batch atomic
    // writes.
    return false;
#endif
  }

 private:
  static constexpr base::FilePath::StringViewType kBaseName =
      FILE_PATH_LITERAL("perftest");

  void ReportMeasurment(std::string operation_name,
                        int iteration_count,
                        base::TimeDelta elapsed_time,
                        base::TimeDelta elapsed_thread_time) {
    const std::string reporter_name("PersistentCache");
    perf_test::PerfResultReporter reporter(reporter_name, operation_name);
    reporter.RegisterImportantMetric(".wall_time", "us");
    reporter.AddResult(
        ".wall_time",
        static_cast<size_t>(elapsed_time.InMicroseconds() / iteration_count));
    reporter.RegisterImportantMetric(".thread_time", "us");
    reporter.AddResult(
        ".thread_time",
        static_cast<size_t>(elapsed_thread_time.InMicroseconds() /
                            iteration_count));
  }

  base::ScopedTempDir temp_dir_;
  std::optional<BackendStorage> backend_storage_;
  bool under_measurment_ = false;
};

TEST_P(PersistentCachePerftest, OpenClose) {
  if (!CanShareConnections()) {
    // TODO(crbug.com/377475540): Switch from sharing a connection below to
    // Bind/Unbind so that the same file handles are used repeatedly to
    // open/close the database.
    GTEST_SKIP();
  }

  static constexpr int kIterationCount = 1024;

  std::unique_ptr<PersistentCache> cache = MakeCache();

  int success_count = 0;
  RunAndTimeTest(
      "OpenClose", kIterationCount, [this, &cache = *cache, &success_count] {
        for (size_t i = 0; i < kIterationCount; ++i) {
          auto persistent_cache_under_test = PersistentCache::Bind(
              Client::kTest, *ShareReadWriteConnection(cache));
          if (persistent_cache_under_test) {
            ++success_count;
          }
        }
      });

  ASSERT_EQ(success_count, kIterationCount);
}

TEST_P(PersistentCachePerftest, Insert) {
  int kIterationCount = 1024;

  if (GetParam() != CacheOption::kJournalModeWal && HasExpensiveCommits()) {
    // Insertions take an egregiously long time when commits are expensive.
    // Scale back the number of iterations in that case.
    kIterationCount /= 4;
  }

  std::unique_ptr<PersistentCache> cache = MakeCache();
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  int success_count = 0;
  RunAndTimeTest("Insert", kIterationCount, [&] {
    success_count = std::ranges::count_if(keys, [&cache = *cache,
                                                 &value](const auto& key) {
      return cache.Insert(base::as_byte_span(key), value.as_span()).has_value();
    });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

TEST_P(PersistentCachePerftest, Find) {
  static constexpr int kIterationCount = 1024;

  // Open the cache in WAL mode and fill it.
  std::unique_ptr<PersistentCache> cache =
      CreateCache(/*single_connection=*/true, /*journal_mode_wal=*/true);
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  // Fill the cache.
  for (const auto& key : keys) {
    ASSERT_OK(cache->Insert(base::as_byte_span(key), value, {}));
  }

  // Switch the cache back to using a rollback journal and close it. This will
  // perform a checkpoint and allow the database to be opened without the
  // write-ahead log file.
  ASSERT_OK(static_cast<SqliteBackendImpl*>(cache->GetBackendForTesting())
                ->ExecuteStatementForTesting("PRAGMA journal_mode=TRUNCATE"));
  cache.reset();
  cache = MakeCache();

  // Shuffle the keys around to avoid taking advantage of file-system caching
  // behavior.
  base::RandomShuffle(keys.begin(), keys.end());

  int success_count = 0;
  RunAndTimeTest("Find", kIterationCount, [&] {
    success_count =
        std::ranges::count_if(keys, [&cache = *cache](const auto& key) {
          return cache
              .Find(base::as_byte_span(key),
                    [](size_t content_size) { return base::span<uint8_t>(); })
              .has_value();
        });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

INSTANTIATE_TEST_SUITE_P(,
                         PersistentCachePerftest,
                         testing::Values(CacheOption::kMultipleConnections,
                                         CacheOption::kSingleConnection,
                                         CacheOption::kJournalModeWal),
                         testing::PrintToStringParamName());

}  // namespace persistent_cache
