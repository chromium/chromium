// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/buildflag.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/sqlite/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace persistent_cache {

using PersistentCachePerftest = testing::Test;

#if !BUILDFLAG(IS_FUCHSIA)

TEST_F(PersistentCachePerftest, OpenClose) {
  test_utils::TestHelper provider;
  std::unique_ptr<Backend> backend =
      provider.CreateBackendWithFiles(BackendType::kSqlite);
  ASSERT_TRUE(backend);
  PersistentCache persistent_cache(std::move(backend));

  // Ensures there are entries in the cache.
  const char* kKey = "foo";
  persistent_cache.Insert(kKey, base::byte_span_from_cstring("1"));
  auto entry = persistent_cache.Find(kKey);
  ASSERT_TRUE(entry);

  base::ElapsedTimer timer;
  const int kAmountOfIteration = 16 * 1024;
  for (size_t i = 0; i < kAmountOfIteration; ++i) {
    auto persistent_cache_under_test =
        PersistentCache::Open(*persistent_cache.ExportReadWriteBackendParams());
  }

  perf_test::PerfResultReporter reporter("PersistentCache", "OpenClose");
  reporter.RegisterImportantMetric(".wall_time", "us");
  reporter.AddResult(".wall_time",
                     static_cast<size_t>(timer.Elapsed().InMicroseconds() /
                                         kAmountOfIteration));
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

}  // namespace persistent_cache
