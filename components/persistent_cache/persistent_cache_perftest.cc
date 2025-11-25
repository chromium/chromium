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
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/buildflag.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace persistent_cache {

class PersistentCachePerftest : public testing::Test {
 protected:
  static constexpr base::FilePath::StringViewType kBaseName =
      FILE_PATH_LITERAL("perftest");

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_storage_.emplace(BackendType::kSqlite, temp_dir_.GetPath());
  }

  std::unique_ptr<PersistentCache> CreateCache(bool single_connection = false,
                                               bool journal_mode_wal = false) {
    if (auto pending_backend = backend_storage_->MakePendingBackend(
            base::FilePath(kBaseName), single_connection, journal_mode_wal);
        pending_backend.has_value()) {
      return PersistentCache::Bind(*std::move(pending_backend));
    }
    ADD_FAILURE() << "Failed to make PendingBackend";
    return nullptr;
  }

  std::optional<PendingBackend> ShareReadWriteConnection(
      PersistentCache& cache) {
    return backend_storage_->ShareReadWriteConnection(base::FilePath(kBaseName),
                                                      cache);
  }

  void RunAndTimeTest(std::string operation_name,
                      int iteration_count,
                      base::FunctionRef<void()> test_body) {
    base::AutoReset<bool> resetter(&under_measurment_, true);
    base::ElapsedTimer elapsed_timer;
    base::ElapsedThreadTimer elapsed_thread_timer;

    test_body();

    ReportMeasurment(operation_name, iteration_count, elapsed_timer.Elapsed(),
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

 private:
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

// Only compile and run these tests on configurations that are monitored.
#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)) && \
    !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER)

TEST_F(PersistentCachePerftest, OpenClose) {
  auto persistent_cache = CreateCache();

  static constexpr int kIterationCount = 1024;

  int success_count = 0;
  RunAndTimeTest("OpenClose", kIterationCount, [&] {
    for (size_t i = 0; i < kIterationCount; ++i) {
      auto persistent_cache_under_test =
          PersistentCache::Bind(*ShareReadWriteConnection(*persistent_cache));
      if (persistent_cache_under_test) {
        ++success_count;
      }
    }
  });

  ASSERT_EQ(success_count, kIterationCount);
}

TEST_F(PersistentCachePerftest, Insert) {
  auto persistent_cache = CreateCache();

  static constexpr int kIterationCount = 1024;
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  int success_count = 0;
  RunAndTimeTest("Insert", kIterationCount, [&] {
    success_count = std::ranges::count_if(
        keys, [&cache = *persistent_cache, &value](const auto& key) {
          return cache.Insert(key, value.as_span()).has_value();
        });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

TEST_F(PersistentCachePerftest, InsertSingleConnection) {
  auto persistent_cache = CreateCache(/*single_connection=*/true);

  static constexpr int kIterationCount = 1024;
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  int success_count = 0;
  RunAndTimeTest("InsertSingleConnection", kIterationCount, [&] {
    success_count = std::ranges::count_if(
        keys, [&cache = *persistent_cache, &value](const auto& key) {
          return cache.Insert(key, value.as_span()).has_value();
        });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

TEST_F(PersistentCachePerftest, InsertWal) {
  auto persistent_cache =
      CreateCache(/*single_connection=*/true, /*journal_mode_wal=*/true);

  static constexpr int kIterationCount = 1024;
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  int success_count = 0;
  RunAndTimeTest("InsertWal", kIterationCount, [&] {
    success_count = std::ranges::count_if(
        keys, [&cache = *persistent_cache, &value](const auto& key) {
          return cache.Insert(key, value.as_span()).has_value();
        });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

TEST_F(PersistentCachePerftest, Find) {
  auto persistent_cache = CreateCache();

  static constexpr int kIterationCount = 1024;
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  // Fill the cache.
  for (const std::string& key : keys) {
    ASSERT_THAT(persistent_cache->Insert(key, value.as_span()),
                base::test::HasValue());
  }

  // Shuffle the keys around to avoid taking advantage of file-system caching
  // behavior.
  base::RandomShuffle(keys.begin(), keys.end());

  int success_count = 0;
  RunAndTimeTest("Find", kIterationCount, [&] {
    success_count = std::ranges::count_if(keys, [&cache = *persistent_cache](
                                                    const auto& key) {
      return cache
          .Find(key, [](size_t content_size) { return base::span<uint8_t>(); })
          .has_value();
    });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

TEST_F(PersistentCachePerftest, FindSingleConnection) {
  auto persistent_cache = CreateCache(/*single_connection=*/true);

  static constexpr int kIterationCount = 1024;
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  // Fill the cache.
  for (const std::string& key : keys) {
    ASSERT_THAT(persistent_cache->Insert(key, value.as_span()),
                base::test::HasValue());
  }

  // Shuffle the keys around to avoid taking advantage of file-system caching
  // behavior.
  base::RandomShuffle(keys.begin(), keys.end());

  int success_count = 0;
  RunAndTimeTest("FindSingleConnection", kIterationCount, [&] {
    success_count = std::ranges::count_if(keys, [&cache = *persistent_cache](
                                                    const auto& key) {
      return cache
          .Find(key, [](size_t content_size) { return base::span<uint8_t>(); })
          .has_value();
    });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

TEST_F(PersistentCachePerftest, FindWal) {
  auto persistent_cache =
      CreateCache(/*single_connection=*/true, /*journal_mode_wal=*/true);

  static constexpr int kIterationCount = 1024;
  std::vector<std::string> keys = GenerateKeys(kIterationCount);
  base::HeapArray<uint8_t> value = MakeValue();

  // Fill the cache.
  for (const std::string& key : keys) {
    ASSERT_THAT(persistent_cache->Insert(key, value.as_span()),
                base::test::HasValue());
  }

  // Shuffle the keys around to avoid taking advantage of file-system caching
  // behavior.
  base::RandomShuffle(keys.begin(), keys.end());

  int success_count = 0;
  RunAndTimeTest("FindWal", kIterationCount, [&] {
    success_count = std::ranges::count_if(keys, [&cache = *persistent_cache](
                                                    const auto& key) {
      return cache
          .Find(key, [](size_t content_size) { return base::span<uint8_t>(); })
          .has_value();
    });
  });
  ASSERT_EQ(success_count, kIterationCount);
}

#endif  // (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)) &&
        // !defined(ADDRESS_SANITIZER) && !defined(THREAD_SANITIZER)

}  // namespace persistent_cache
