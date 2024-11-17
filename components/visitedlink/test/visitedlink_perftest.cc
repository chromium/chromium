// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_file_util.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "url/gurl.h"


namespace visitedlink {

namespace {

static constexpr char kMetricAddAndQueryMs[] = "add_and_query";
static constexpr char kMetricTableInitMs[] = "table_initialization";
static constexpr char kMetricLinkInitMs[] = "link_init";
static constexpr char kMetricDatabaseFlushMs[] = "database_flush";
static constexpr char kMetricColdLoadTimeMs[] = "cold_load_time";
static constexpr char kMetricHotLoadTimeMs[] = "hot_load_time";
static constexpr char kMetricAddURLTimeMs[] = "add_url_time";
static constexpr char kMetricAddURLsTimeMs[] = "add_urls_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& metric_suffix) {
  perf_test::PerfResultReporter reporter("VisitedLink.", metric_suffix);
  reporter.RegisterImportantMetric(kMetricAddAndQueryMs, "ms");
  reporter.RegisterImportantMetric(kMetricTableInitMs, "ms");
  reporter.RegisterImportantMetric(kMetricLinkInitMs, "ms");
  reporter.RegisterImportantMetric(kMetricDatabaseFlushMs, "ms");
  reporter.RegisterImportantMetric(kMetricColdLoadTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricHotLoadTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricAddURLTimeMs, "ms");
  reporter.RegisterImportantMetric(kMetricAddURLsTimeMs, "ms");
  return reporter;
}

// Designed like base/test/perf_time_logger but uses testing/perf instead of
// base/test/perf* to report timings.
class TimeLogger {
 public:
  explicit TimeLogger(std::string metric_suffix);

  TimeLogger(const TimeLogger&) = delete;
  TimeLogger& operator=(const TimeLogger&) = delete;

  ~TimeLogger();
  void Done();

 private:
  bool logged_;
  std::string metric_suffix_;
  base::ElapsedTimer timer_;
};

TimeLogger::TimeLogger(std::string metric_suffix)
    : logged_(false), metric_suffix_(std::move(metric_suffix)) {}

TimeLogger::~TimeLogger() {
  if (!logged_)
    Done();
}

void TimeLogger::Done() {
  // We use a floating-point millisecond value because it is more
  // intuitive than microseconds and we want more precision than
  // integer milliseconds.
  perf_test::PerfResultReporter reporter = SetUpReporter("baseline_story");
  reporter.AddResult(metric_suffix_, timer_.Elapsed().InMillisecondsF());
  logged_ = true;
}

// how we generate URLs, note that the two strings should be the same length
const int kAddCount = 10000;
const int kLoadTestInitialCount = 250000;
const char kAddedPrefix[] =
    "http://www.google.com/stuff/something/"
    "foo?session=85025602345625&id=1345142319023&seq=";
const char kUnaddedPrefix[] =
    "http://www.google.org/stuff/something/"
    "foo?session=39586739476365&id=2347624314402&seq=";

// Returns a URL with the given prefix and index
GURL TestURL(const char* prefix, int i) {
  return GURL(base::StringPrintf("%s%d", prefix, i));
}

// We have no readers, so all methods on this listener are a no-ops.
class DummyVisitedLinkEventListener : public VisitedLinkWriter::Listener {
 public:
  DummyVisitedLinkEventListener() = default;
  void NewTable(base::ReadOnlySharedMemoryRegion*) override {}
  void Add(VisitedLinkCommon::Fingerprint) override {}
  void Reset(bool invalidate_hashes) override {}
};

// this checks IsVisited for the URLs starting with the given prefix and
// within the given range
void CheckVisited(VisitedLinkWriter& writer,
                  const char* prefix,
                  int begin,
                  int end) {
  for (int i = begin; i < end; i++)
    writer.IsVisited(TestURL(prefix, i));
}

// Fills that writer's table with URLs starting with the given prefix and
// within the given range
void FillTable(VisitedLinkWriter& writer,
               const char* prefix,
               int begin,
               int end,
               int batch_size = 1) {
  if (batch_size > 1) {
    std::vector<GURL> urls;
    urls.reserve(batch_size);
    for (int i = begin; i < end; i += batch_size) {
      for (int j = i; j < end && j < i + batch_size; j++)
        urls.push_back(TestURL(prefix, j));
      writer.AddURLs(urls);
      urls.clear();
    }
  } else {
    for (int i = begin; i < end; i++)
      writer.AddURL(TestURL(prefix, i));
  }
}

class VisitedLinkPerfTest : public testing::Test {
 protected:
  base::FilePath db_path_;
  void SetUp() override { ASSERT_TRUE(base::CreateTemporaryFile(&db_path_)); }
  void TearDown() override { base::DeleteFile(db_path_); }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

} // namespace

// This test tests adding many things to a database, and how long it takes
// to query the database with different numbers of things in it. The time
// is the total time to do all the operations, and as such, it is only
// useful for a regression test. If there is a regression, it might be
// useful to make another set of tests to test these things in isolation.
TEST_F(VisitedLinkPerfTest, TestAddAndQuery) {
  // init
  VisitedLinkWriter writer(new DummyVisitedLinkEventListener(), nullptr, true,
                           true, db_path_, 0);
  ASSERT_TRUE(writer.Init());
  content::RunAllTasksUntilIdle();

  TimeLogger timer(kMetricAddAndQueryMs);

  // first check without anything in the table
  CheckVisited(writer, kAddedPrefix, 0, kAddCount);

  // now fill half the table
  const int half_size = kAddCount / 2;
  FillTable(writer, kAddedPrefix, 0, half_size);

  // check the table again, half of these URLs will be visited, the other half
  // will not
  CheckVisited(writer, kAddedPrefix, 0, kAddCount);

  // fill the rest of the table
  FillTable(writer, kAddedPrefix, half_size, kAddCount);

  // check URLs, doing half visited, half unvisited
  CheckVisited(writer, kAddedPrefix, 0, kAddCount);
  CheckVisited(writer, kUnaddedPrefix, 0, kAddCount);
}

// Tests how long it takes to write and read a large database to and from disk.
// TODO(crbug.com/40719465): Fix flakiness on macOS and Android.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_TestBigTable DISABLED_TestBigTable
#else
#define MAYBE_TestBigTable TestBigTable
#endif
TEST_F(VisitedLinkPerfTest, MAYBE_TestBigTable) {
  base::test::ScopedDisableRunLoopTimeout disable_run_timeout;
  // create a big DB
  {
    TimeLogger table_initialization_timer(kMetricTableInitMs);

    auto writer = std::make_unique<VisitedLinkWriter>(
        new DummyVisitedLinkEventListener(), nullptr, true, true, db_path_, 0);

    // time init with empty table
    TimeLogger initTimer(kMetricLinkInitMs);
    bool success = writer->Init();
    content::RunAllTasksUntilIdle();
    initTimer.Done();
    ASSERT_TRUE(success);

    // add a bunch of stuff
    // TODO(maruel): This is very inefficient because the file gets rewritten
    // many time and this is the actual bottleneck of this test. The file should
    // only get written that the end of the FillTable call, not 4169(!) times.
    FillTable(*writer, kAddedPrefix, 0, kLoadTestInitialCount);
    content::RunAllTasksUntilIdle();

    // time writing the file out out
    TimeLogger flushTimer(kMetricDatabaseFlushMs);
    writer->RewriteFile();
    writer.reset();  // Will post a task to fclose() the file and thus flush it.
    content::RunAllTasksUntilIdle();
    flushTimer.Done();

    table_initialization_timer.Done();
  }

  // test loading the DB back.
  // make sure the file has to be re-loaded
  base::EvictFileFromSystemCache(db_path_);

  // cold load (no OS cache, hopefully)
  {
    TimeLogger cold_load_timer(kMetricColdLoadTimeMs);

    VisitedLinkWriter writer(new DummyVisitedLinkEventListener(), nullptr, true,
                             true, db_path_, 0);
    bool success = writer.Init();
    content::RunAllTasksUntilIdle();

    cold_load_timer.Done();
    ASSERT_TRUE(success);
  }

  // hot load (with OS caching the file in memory)
  TimeLogger hot_load_timer(kMetricHotLoadTimeMs);

  VisitedLinkWriter writer(new DummyVisitedLinkEventListener(), nullptr, true,
                           true, db_path_, 0);
  bool success = writer.Init();
  content::RunAllTasksUntilIdle();

  hot_load_timer.Done();
  ASSERT_TRUE(success);

  // Add some more URLs one-by-one.
  TimeLogger add_url_timer(kMetricAddURLTimeMs);
  FillTable(writer, kAddedPrefix, writer.GetUsedCount(),
            writer.GetUsedCount() + kAddCount);
  content::RunAllTasksUntilIdle();
  add_url_timer.Done();

  TimeLogger add_urls_timer(kMetricAddURLsTimeMs);
  // Add some more URLs in groups of 2.
  int batch_size = 2;
  FillTable(writer, kAddedPrefix, writer.GetUsedCount(),
            writer.GetUsedCount() + kAddCount, batch_size);
  // Add some more URLs in a big batch.
  batch_size = kAddCount;
  FillTable(writer, kAddedPrefix, writer.GetUsedCount(),
            writer.GetUsedCount() + kAddCount, batch_size);
  content::RunAllTasksUntilIdle();
  add_urls_timer.Done();
}

}  // namespace visitedlink
