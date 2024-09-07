// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace safe_browsing {

namespace {

constexpr char kMetricPrefixV4Store[] = "V4Store.";
constexpr char kMetricGetMatchingHashPrefixMs[] = "get_matching_hash_prefix";

perf_test::PerfResultReporter SetUpV4StoreReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixV4Store, story);
  reporter.RegisterImportantMetric(kMetricGetMatchingHashPrefixMs, "ms");
  return reporter;
}

}  // namespace

class V4StorePerftest : public testing::Test {};

TEST_F(V4StorePerftest, StressTest) {
// Debug builds can be quite slow. Use a smaller number of prefixes to test.
#if defined(NDEBUG)
  const size_t kNumPrefixes = 2000000;
#else
  const size_t kNumPrefixes = 20000;
#endif

  static_assert(kMaxHashPrefixLength == crypto::kSHA256Length,
                "SHA256 produces a valid FullHashStr");
  CHECK(base::IsValidForType<size_t>(
      base::CheckMul(kNumPrefixes, kMaxHashPrefixLength)));

  // Keep the full hashes as one big string to avoid tons of allocations /
  // deallocations in the test.
  std::string full_hashes(kNumPrefixes * kMaxHashPrefixLength, 0);
  std::string_view full_hashes_piece = std::string_view(full_hashes);
  std::vector<std::string> prefixes;
  for (size_t i = 0; i < kNumPrefixes; i++) {
    size_t index = i * kMaxHashPrefixLength;
    crypto::SHA256HashString(base::StringPrintf("%zu", i), &full_hashes[index],
                             kMaxHashPrefixLength);
    prefixes.push_back(full_hashes.substr(index, kMinHashPrefixLength));
  }

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath store_path =
      temp_dir.GetPath().AppendASCII("V4StoreTest.store");

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto store = std::make_unique<V4Store>(task_runner, store_path);
  std::sort(prefixes.begin(), prefixes.end());
  store->hash_prefix_map_->Clear();
  store->hash_prefix_map_->Append(kMinHashPrefixLength, base::StrCat(prefixes));

  V4StoreFileFormat file_format;
  ASSERT_EQ(WRITE_SUCCESS, store->WriteToDisk(&file_format));

  size_t matches = 0;
  auto reporter = SetUpV4StoreReporter("stress_test");
  base::ElapsedTimer timer;
  for (size_t i = 0; i < kNumPrefixes; i++) {
    size_t index = i * kMaxHashPrefixLength;
    std::string_view full_hash =
        full_hashes_piece.substr(index, kMaxHashPrefixLength);
    matches += !store->GetMatchingHashPrefix(full_hash).empty();
  }
  reporter.AddResult(kMetricGetMatchingHashPrefixMs,
                     timer.Elapsed().InMillisecondsF());

  EXPECT_EQ(kNumPrefixes, matches);
}

}  // namespace safe_browsing
