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
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace safe_browsing {

namespace {

constexpr char kMetricPrefixV4Store[] = "V4Store.";
constexpr char kMetricGetMatchingHashPrefixMs[] = "get_matching_hash_prefix";
constexpr char kMetricVerifyChecksumMs[] = "verify_checksum";
constexpr char kMetricMergeUpdateMs[] = "merge_update";

perf_test::PerfResultReporter SetUpV4StoreReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixV4Store, story);
  reporter.RegisterImportantMetric(kMetricGetMatchingHashPrefixMs, "ms");
  return reporter;
}

perf_test::PerfResultReporter SetUpVerifyChecksumReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixV4Store, story);
  reporter.RegisterImportantMetric(kMetricVerifyChecksumMs, "ms");
  return reporter;
}

perf_test::PerfResultReporter SetUpMergeUpdateReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixV4Store, story);
  reporter.RegisterImportantMetric(kMetricMergeUpdateMs, "ms");
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

  static_assert(kMaxHashPrefixLength == crypto::hash::kSha256Size,
                "SHA256 produces a valid FullHashStr");
  CHECK(base::IsValidForType<size_t>(
      base::CheckMul(kNumPrefixes, kMaxHashPrefixLength)));

  // Keep the full hashes as one big string to avoid tons of allocations /
  // deallocations in the test.
  std::string full_hashes(kNumPrefixes * kMaxHashPrefixLength, 0);
  std::string_view full_hashes_piece = std::string_view(full_hashes);
  std::vector<std::string> prefixes;
  prefixes.reserve(kNumPrefixes);
  for (size_t i = 0; i < kNumPrefixes; i++) {
    size_t index = i * kMaxHashPrefixLength;
    auto result_buffer = base::as_writable_byte_span(full_hashes)
                             .subspan(index, kMaxHashPrefixLength);
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::byte_span_from_ref(i), result_buffer);
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

TEST_F(V4StorePerftest, VerifyChecksumFast) {
  const size_t kNumPrefixes = 1000000;
  std::string full_hashes(kNumPrefixes * kMaxHashPrefixLength, 0);
  std::vector<std::string> prefixes;
  prefixes.reserve(kNumPrefixes);
  for (size_t i = 0; i < kNumPrefixes; i++) {
    size_t index = i * kMaxHashPrefixLength;
    auto result_buffer = base::as_writable_byte_span(full_hashes)
                             .subspan(index, kMaxHashPrefixLength);
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::byte_span_from_ref(i), result_buffer);
    prefixes.push_back(full_hashes.substr(index, kMinHashPrefixLength));
  }
  std::sort(prefixes.begin(), prefixes.end());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath store_path =
      temp_dir.GetPath().AppendASCII("V4StoreTestVerifyChecksum.store");

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto store = std::make_unique<V4Store>(task_runner, store_path);
  store->hash_prefix_map_->Clear();
  store->hash_prefix_map_->Append(kMinHashPrefixLength, base::StrCat(prefixes));
  store->expected_checksum_ = std::string(32, '0');

  V4StoreFileFormat file_format;
  ASSERT_EQ(WRITE_SUCCESS, store->WriteToDisk(&file_format));

  auto reporter = SetUpVerifyChecksumReporter("verify_checksum_fast");
  base::ElapsedTimer timer;
  store->VerifyChecksum();
  reporter.AddResult(kMetricVerifyChecksumMs,
                     timer.Elapsed().InMillisecondsF());
}

TEST_F(V4StorePerftest, MergeUpdateFast) {
  const size_t kNumOldPrefixes = 1000000;
  const size_t kNumNewPrefixes = 1000;
  const size_t kNumRemovals = 100;

  std::string old_full_hashes(kNumOldPrefixes * kMaxHashPrefixLength, 0);
  std::vector<std::string> old_prefixes;
  old_prefixes.reserve(kNumOldPrefixes);
  for (size_t i = 0; i < kNumOldPrefixes; i++) {
    size_t index = i * kMaxHashPrefixLength;
    auto result_buffer = base::as_writable_byte_span(old_full_hashes)
                             .subspan(index, kMaxHashPrefixLength);
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::byte_span_from_ref(i), result_buffer);
    old_prefixes.push_back(old_full_hashes.substr(index, kMinHashPrefixLength));
  }
  std::sort(old_prefixes.begin(), old_prefixes.end());

  std::string new_full_hashes(kNumNewPrefixes * kMaxHashPrefixLength, 0);
  std::vector<std::string> new_prefixes;
  new_prefixes.reserve(kNumNewPrefixes);
  for (size_t i = 0; i < kNumNewPrefixes; i++) {
    size_t index = i * kMaxHashPrefixLength;
    auto result_buffer = base::as_writable_byte_span(new_full_hashes)
                             .subspan(index, kMaxHashPrefixLength);
    size_t val = i + kNumOldPrefixes + 100000;
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::byte_span_from_ref(val), result_buffer);
    new_prefixes.push_back(new_full_hashes.substr(index, kMinHashPrefixLength));
  }
  std::sort(new_prefixes.begin(), new_prefixes.end());

  ::google::protobuf::RepeatedField<int32_t> raw_removals;
  for (size_t i = 0; i < kNumRemovals; i++) {
    raw_removals.Add(i * (kNumOldPrefixes / kNumRemovals));
  }

  HashPrefixMapView old_map;
  std::string old_str = base::StrCat(old_prefixes);
  old_map[kMinHashPrefixLength] = old_str;

  HashPrefixMapView new_map;
  std::string new_str = base::StrCat(new_prefixes);
  new_map[kMinHashPrefixLength] = new_str;

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto store = std::make_unique<V4Store>(task_runner, base::FilePath());

  auto reporter = SetUpMergeUpdateReporter("merge_update_fast");
  base::ElapsedTimer timer;
  store->MergeUpdate(old_map, new_map, &raw_removals, "");
  reporter.AddResult(kMetricMergeUpdateMs, timer.Elapsed().InMillisecondsF());
}

}  // namespace safe_browsing
