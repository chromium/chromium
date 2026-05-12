// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/function_ref.h"
#include "base/rand_util.h"
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



// A test harness parameterized on the options for creating a PersistentCache.
class PersistentCachePerftest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_storage_.emplace(Client::kTest, BackendType::kSqlite,
                             temp_dir_.GetPath());
  }

 public:
  // Returns a string representing the test parameter for story names.
  static std::string GetParamName(std::tuple<bool, bool> param) {
    auto [single_connection, journal_mode_wal] = param;
    if (single_connection && journal_mode_wal) {
      return "JournalModeWal";
    } else if (single_connection && !journal_mode_wal) {
      return "SingleConnection";
    } else if (!single_connection && journal_mode_wal) {
      return "MultipleConnectionsWal";
    } else {
      return "MultipleConnections";
    }
  }

  // Returns a new cache configured according to the test's parameter.
  std::unique_ptr<PersistentCache> MakeCache() {
    auto [single_connection, journal_mode_wal] = GetParam();
    return CreateCache(single_connection, journal_mode_wal);
  }

  // Returns a new cache with the given options.
  std::unique_ptr<PersistentCache> CreateCache(bool single_connection,
                                               bool journal_mode_wal) {
    if (auto pending_backend = backend_storage_->MakePendingBackend(
            base::FilePath(kBaseName), single_connection, journal_mode_wal);
        pending_backend.has_value()) {
      if (auto cache_result =
              PersistentCache::Bind(Client::kTest, *std::move(pending_backend));
          cache_result.has_value()) {
        return *std::move(cache_result);
      }
    }
    ADD_FAILURE() << "Failed to make PendingBackend or Bind it";
    return nullptr;
  }

  // Returns true if caches created in this configuration can be shared across
  // multiple connections.
  static bool CanShareConnections() { return !std::get<0>(GetParam()); }

  // Returns true if caches created in this configuration use the write-ahead
  // log.
  static bool IsWalMode() { return std::get<1>(GetParam()); }

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

    ReportMeasurement(base::StrCat({operation_name, GetParamName(GetParam())}),
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

  void ReportMeasurement(std::string operation_name,
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

  static base::FilePath GetBaseName(int i) {
    return base::FilePath(kBaseName).InsertBeforeExtensionASCII(
        base::NumberToString(i));
  }

  BackendStorage& backend_storage() { return *backend_storage_; }

 private:
  static constexpr base::FilePath::StringViewType kBaseName =
      FILE_PATH_LITERAL("perftest");

  base::ScopedTempDir temp_dir_;
  std::optional<BackendStorage> backend_storage_;
  bool under_measurment_ = false;
};

TEST_P(PersistentCachePerftest, Create) {
  static constexpr int kIterationCount = 1024;

  auto backend_names =
      base::HeapArray<base::FilePath>::WithSize(kIterationCount);
  std::ranges::generate(backend_names,
                        [i = 0] mutable { return GetBaseName(i++); });
  auto caches = base::HeapArray<std::unique_ptr<PersistentCache>>::WithSize(
      kIterationCount);

  auto [single_connection, journal_mode_wal] = GetParam();
  int success_count = 0;
  RunAndTimeTest("Create", kIterationCount, [&] {
    for (size_t i = 0; i < kIterationCount; ++i) {
      if (auto pending_backend = backend_storage().MakePendingBackend(
              backend_names[i], single_connection, journal_mode_wal);
          pending_backend.has_value()) {
        if (auto cache_result = PersistentCache::Bind(
                Client::kTest, *std::move(pending_backend));
            cache_result.has_value()) {
          caches[i] = std::move(cache_result.value());
          ++success_count;
        }
      }
    }
  });

  ASSERT_EQ(success_count, kIterationCount);
}

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
          if (PersistentCache::Bind(Client::kTest,
                                    *ShareReadWriteConnection(cache))
                  .has_value()) {
            ++success_count;
          }
        }
      });

  ASSERT_EQ(success_count, kIterationCount);
}

TEST_P(PersistentCachePerftest, Insert) {
  int kIterationCount = 1024;

  if (!IsWalMode() && HasExpensiveCommits()) {
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

TEST_P(PersistentCachePerftest, WALPerformance) {
  if (!IsWalMode()) {
    GTEST_SKIP();
  }

  static constexpr int kTotalCount = 2048;
  static constexpr int kHalfCount = kTotalCount / 2;
  std::unique_ptr<PersistentCache> cache = MakeCache();
  auto* backend =
      static_cast<SqliteBackendImpl*>(cache->GetBackendForTesting());

  // Disable automatic checkpointing.
  ASSERT_OK(backend->ExecuteStatementForTesting("PRAGMA wal_autocheckpoint=0"));

  std::vector<std::string> keys = GenerateKeys(kTotalCount);
  base::HeapArray<uint8_t> value = MakeValue();

  // 1. Insert first half of the data (goes to WAL).
  for (int i = 0; i < kHalfCount; ++i) {
    ASSERT_OK(cache->Insert(base::as_byte_span(keys[i]), value));
  }

  // 2. Perform a truncating checkpoint to move data from WAL to database.
  ASSERT_OK(
      backend->ExecuteStatementForTesting("PRAGMA wal_checkpoint(TRUNCATE)"));

  // 3. Insert second half of the data (goes to WAL).
  for (int i = kHalfCount; i < kTotalCount; ++i) {
    ASSERT_OK(cache->Insert(base::as_byte_span(keys[i]), value));
  }

  // Shuffle keys for random access.
  base::RandomShuffle(keys.begin(), keys.end());

  // 4. Measure performance with mixed WAL and DB data.
  base::ElapsedTimer mixed_timer;
  base::ElapsedThreadTimer mixed_thread_timer;
  for (const auto& key : keys) {
    auto result = cache->Find(base::as_byte_span(key), [&value](size_t size) {
      return value.as_span();
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
  }
  base::TimeDelta mixed_elapsed = mixed_timer.Elapsed();
  base::TimeDelta mixed_thread_elapsed = mixed_thread_timer.Elapsed();

  // 5. Perform final truncating checkpoint.
  ASSERT_OK(
      backend->ExecuteStatementForTesting("PRAGMA wal_checkpoint(TRUNCATE)"));

  // 6. Measure performance with all data in DB.
  base::ElapsedTimer db_timer;
  base::ElapsedThreadTimer db_thread_timer;
  for (const auto& key : keys) {
    auto result = cache->Find(base::as_byte_span(key), [&value](size_t size) {
      return value.as_span();
    });
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
  }
  base::TimeDelta db_elapsed = db_timer.Elapsed();
  base::TimeDelta db_thread_elapsed = db_thread_timer.Elapsed();

  // 7. Report the difference (Mixed - DB = Overhead).
  ReportMeasurement(
      "WALOverhead", kTotalCount,
      std::max(base::TimeDelta(), mixed_elapsed - db_elapsed),
      std::max(base::TimeDelta(), mixed_thread_elapsed - db_thread_elapsed));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PersistentCachePerftest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<PersistentCachePerftest::ParamType>& info) {
      return PersistentCachePerftest::GetParamName(info.param);
    });

}  // namespace persistent_cache
