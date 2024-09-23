// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <string_view>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/omnibox/browser/history_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace history {

namespace {

// Not threadsafe.
std::string GenerateFakeHashedString(size_t sym_count) {
  static constexpr char kSyms[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,/=+?#";
  static std::mt19937 engine;
  std::uniform_int_distribution<size_t> index_distribution(
      0, std::size(kSyms) - 2 /* trailing \0 */);

  std::string res;
  res.reserve(sym_count);

  std::generate_n(std::back_inserter(res), sym_count, [&index_distribution] {
    return kSyms[index_distribution(engine)];
  });

  return res;
}

URLRow GeneratePopularURLRow() {
  static constexpr char kPopularUrl[] =
      "http://long.popular_url_with.many_variations/";

  constexpr size_t kFakeHashLength = 10;
  std::string fake_hash = GenerateFakeHashedString(kFakeHashLength);

  URLRow row{GURL(kPopularUrl + fake_hash)};
  EXPECT_TRUE(row.url().is_valid());
  row.set_title(base::UTF8ToUTF16("Page " + fake_hash));
  row.set_visit_count(1);
  row.set_typed_count(1);
  row.set_last_visit(base::Time::Now() - base::Days(1));
  return row;
}

using StringPieces = std::vector<std::string_view>;

StringPieces AllPrefixes(const std::string& str) {
  std::vector<std::string_view> res;
  res.reserve(str.size());
  for (auto char_it = str.begin(); char_it != str.end(); ++char_it)
    res.push_back(base::MakeStringPiece(str.begin(), char_it));
  return res;
}

}  // namespace

class HQPPerfTestOnePopularURL : public testing::Test {
 protected:
  HQPPerfTestOnePopularURL() = default;
  HQPPerfTestOnePopularURL(const HQPPerfTestOnePopularURL&) = delete;
  HQPPerfTestOnePopularURL& operator=(const HQPPerfTestOnePopularURL&) = delete;

  void SetUp() override;
  void TearDown() override;

  // Populates history with variations of the same URL.
  void PrepareData();

  // Runs HQP on a banch of consecutive pieces of an input string and times
  // them. Resulting timings printed in groups.
  template <typename PieceIt>
  void RunAllTests(PieceIt first, PieceIt last);

  // Encapsulates calls to performance infrastructure.
  void PrintMeasurements(const std::string& trace_name,
                         const std::vector<base::TimeDelta>& measurements);

  history::HistoryBackend* history_backend() {
    return client_->GetHistoryService()->history_backend_.get();
  }

 private:
  base::TimeDelta RunTest(const std::u16string& text);

  base::ScopedTempDir history_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<HistoryQuickProvider> provider_;
};

void HQPPerfTestOnePopularURL::SetUp() {
  if (base::ThreadTicks::IsSupported())
    base::ThreadTicks::WaitUntilInitialized();
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  CHECK(history_dir_.CreateUniqueTempDir());
  client_->set_history_service(
      history::CreateHistoryService(history_dir_.GetPath(), true));
  client_->set_bookmark_model(bookmarks::TestBookmarkClient::CreateModel());

  ASSERT_NE(client_->GetHistoryService(), nullptr);
  ASSERT_NO_FATAL_FAILURE(PrepareData());

  client_->set_in_memory_url_index(std::make_unique<InMemoryURLIndex>(
      client_->GetBookmarkModel(), client_->GetHistoryService(), nullptr,
      history_dir_.GetPath(), SchemeSet()));
  client_->GetInMemoryURLIndex()->Init();

  // Block until History has processed InMemoryURLIndex initialization.
  history::BlockUntilHistoryProcessesPendingRequests(
      client_->GetHistoryService());
  ASSERT_TRUE(client_->GetInMemoryURLIndex()->restored());

  provider_ = new HistoryQuickProvider(client_.get());
}

void HQPPerfTestOnePopularURL::TearDown() {
  base::RunLoop run_loop;
  auto* history_service = client_->GetHistoryService();
  history_service->SetOnBackendDestroyTask(run_loop.QuitClosure());
  provider_ = nullptr;
  client_.reset();
  run_loop.Run();
  base::ThreadPoolInstance::Get()->FlushForTesting();
  task_environment_.RunUntilIdle();
}

void HQPPerfTestOnePopularURL::PrepareData() {
// Note: On debug builds these tests can be slow. Use a smaller data set in
// that case. See crbug.com/822624.
#if defined NDEBUG
  constexpr size_t kSimilarUrlCount = 10000;
#else
  LOG(ERROR) << "HQP performance test is running on a debug build, results may "
                "not be accurate.";
  constexpr size_t kSimilarUrlCount = 100;
#endif
  for (size_t i = 0; i < kSimilarUrlCount; ++i)
    AddFakeURLToHistoryDB(history_backend()->db(), GeneratePopularURLRow());
}

void HQPPerfTestOnePopularURL::PrintMeasurements(
    const std::string& story_name,
    const std::vector<base::TimeDelta>& measurements) {
  auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

  std::string durations;
  for (const auto& measurement : measurements)
    durations +=
        base::NumberToString(measurement.InMillisecondsRoundedUp()) + ',';
  // Strip off trailing comma.
  durations.pop_back();

  auto metric_prefix = std::string(test_info->test_suite_name()) + "_" +
                       std::string(test_info->name());
  perf_test::PerfResultReporter reporter(metric_prefix, story_name);
  reporter.RegisterImportantMetric(".duration", "ms");
  reporter.AddResultList(".duration", durations);
}

base::TimeDelta HQPPerfTestOnePopularURL::RunTest(const std::u16string& text) {
  base::RunLoop().RunUntilIdle();
  AutocompleteInput input(text, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  if (base::ThreadTicks::IsSupported()) {
    base::ThreadTicks start = base::ThreadTicks::Now();
    provider_->Start(input, false);
    return base::ThreadTicks::Now() - start;
  }

  base::Time start = base::Time::Now();
  provider_->Start(input, false);
  return base::Time::Now() - start;
}

template <typename PieceIt>
void HQPPerfTestOnePopularURL::RunAllTests(PieceIt first, PieceIt last) {
  constexpr size_t kTestGroupSize = 5;
  std::vector<base::TimeDelta> measurements;
  measurements.reserve(kTestGroupSize);

  for (PieceIt group_start = first; group_start != last;) {
    PieceIt group_end = std::min(group_start + kTestGroupSize, last);

    std::transform(group_start, group_end, std::back_inserter(measurements),
                   [this](std::string_view prefix) {
                     return RunTest(base::UTF8ToUTF16(prefix));
                   });

    PrintMeasurements(base::NumberToString(group_start->size()) + '-' +
                          base::NumberToString((group_end - 1)->size()),
                      measurements);

    measurements.clear();
    group_start = group_end;
  }
}

TEST_F(HQPPerfTestOnePopularURL, Typing) {
  std::string test_url = GeneratePopularURLRow().url().spec();
  StringPieces prefixes = AllPrefixes(test_url);
  RunAllTests(prefixes.begin(), prefixes.end());
}

TEST_F(HQPPerfTestOnePopularURL, Backspacing) {
  std::string test_url = GeneratePopularURLRow().url().spec();
  StringPieces prefixes = AllPrefixes(test_url);
  RunAllTests(prefixes.rbegin(), prefixes.rend());
}

}  // namespace history
