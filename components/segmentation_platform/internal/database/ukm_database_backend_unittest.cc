// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/segmentation_platform/internal/database/ukm_database_test_utils.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::segmentation_platform::processing::ProcessedValue;
using ::segmentation_platform::test_util::UrlMatcher;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

// Set path to protected file which fails to open the database.
#if BUILDFLAG(IS_POSIX)
constexpr base::FilePath::CharType kBadFilePath[] = FILE_PATH_LITERAL("/usr");
#else
constexpr base::FilePath::CharType kBadFilePath[] =
    FILE_PATH_LITERAL("C:\\Windows");
#endif

// Stats about the database tables.
struct DatabaseStats {
  // Total number of metrics in metrics table.
  int total_metrics = 0;
  // Number of metrics grouped by event ID.
  base::flat_map<MetricsRowEventId, int> metric_count_for_event_id;
  // Number of metrics grouped by URL ID.
  base::flat_map<UrlId, int> metric_count_for_url_id;
};

// Computes stats about the database tables.
DatabaseStats GetDatabaseStats(sql::Database& db) {
  DatabaseStats stats;
  auto all_rows = test_util::GetAllMetricsRows(db);
  stats.total_metrics = all_rows.size();
  for (const auto& row : all_rows) {
    stats.metric_count_for_event_id[row.event_id]++;
    stats.metric_count_for_url_id[row.url_id]++;
  }
  return stats;
}

// Checks if the metrics table has exactly `expected_count` metrics with the
// `url_id`.
void ExpectEntriesWithUrlId(sql::Database& db,
                            UrlId url_id,
                            int expected_count) {
  DatabaseStats stats = GetDatabaseStats(db);
  EXPECT_THAT(stats.metric_count_for_url_id,
              Contains(std::make_pair(url_id, expected_count)));
}

// Creates a sample UKM entry, with a single event.
ukm::mojom::UkmEntryPtr GetSampleUkmEntry(ukm::SourceId source_id = 10) {
  ukm::mojom::UkmEntryPtr entry = ukm::mojom::UkmEntry::New();
  entry->source_id = source_id;
  entry->event_hash = 20;
  entry->metrics[30] = 100;
  entry->metrics[31] = 101;
  entry->metrics[32] = 102;
  return entry;
}

UmaMetricEntry GetSampleMetricsRow() {
  return UmaMetricEntry{.type = proto::SignalType::HISTOGRAM_VALUE,
                        .name_hash = 10,
                        .time = base::Time::Now(),
                        .value = 100};
}

}  // namespace

class UkmDatabaseBackendTest : public testing::Test {
 public:
  UkmDatabaseBackendTest() = default;
  ~UkmDatabaseBackendTest() override = default;

  void SetUp() override {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateAndInitBackend();
  }

  void CreateAndInitBackend() {
    backend_ = std::make_unique<UkmDatabaseBackend>(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ukm_database")),
        /*in_memory=*/false, task_runner_);
    base::RunLoop wait_for_init;
    backend_->InitDatabase(base::BindOnce(
        [](base::OnceClosure quit,
           scoped_refptr<base::SequencedTaskRunner> task_runner, bool success) {
          EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
          std::move(quit).Run();
          ASSERT_TRUE(success);
        },
        wait_for_init.QuitClosure(), task_runner_));
    wait_for_init.Run();
  }

  void TearDown() override {
    backend_.reset();
    task_runner_.reset();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void ExpectQueryResult(UkmDatabase::QueryList&& queries,
                         bool expect_success,
                         const processing::IndexedTensors& expected_values) {
    base::RunLoop wait_for_query3;
    backend_->RunReadOnlyQueries(
        std::move(queries),
        base::BindOnce(
            [](base::OnceClosure quit, bool expect_success,
               const processing::IndexedTensors& expected_values, bool success,
               processing::IndexedTensors tensors) {
              EXPECT_EQ(expect_success, success);
              EXPECT_EQ(expected_values, tensors);
              std::move(quit).Run();
            },
            wait_for_query3.QuitClosure(), expect_success, expected_values));
    wait_for_query3.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<UkmDatabaseBackend> backend_;
};

TEST_F(UkmDatabaseBackendTest, EntriesWithoutUrls) {
  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry();

  backend_->StoreUkmEntry(std::move(entry1));
  DatabaseStats stats1 = GetDatabaseStats(backend_->db());
  ASSERT_EQ(stats1.metric_count_for_event_id.size(), 1u);
  EXPECT_EQ(stats1.metric_count_for_event_id.begin()->second, 3);
  EXPECT_THAT(stats1.metric_count_for_url_id,
              ElementsAre(std::make_pair(UrlId(), 3)));

  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry();
  backend_->StoreUkmEntry(std::move(entry2));
  DatabaseStats stats2 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats2.metric_count_for_event_id.size(), 2u);
  EXPECT_EQ(stats2.metric_count_for_event_id.begin()->second, 3);
  EXPECT_EQ(stats2.metric_count_for_event_id.rbegin()->second, 3);
  EXPECT_THAT(stats2.metric_count_for_url_id,
              ElementsAre(std::make_pair(UrlId(), 6)));

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, UrlIdsForEntries) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId3 = 20;
  const ukm::SourceId kSourceId4 = 30;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry3 = GetSampleUkmEntry(kSourceId3);
  ukm::mojom::UkmEntryPtr entry4 = GetSampleUkmEntry(kSourceId4);

  // Add entry before adding source URL.
  backend_->StoreUkmEntry(std::move(entry1));
  ExpectEntriesWithUrlId(backend_->db(), UrlId(), 3);

  // Updating URL should update metrics with the URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 3);

  // Adding more entries with source ID1 should use the existing URL ID.
  backend_->StoreUkmEntry(std::move(entry2));
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 6);

  // Updating URL for source ID2, then adding entries, should use the URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl2, false, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry3));
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 6);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 3);

  // Adding more entries without related URL should use invalid URL ID.
  backend_->StoreUkmEntry(std::move(entry4));
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 6);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 3);
  ExpectEntriesWithUrlId(backend_->db(), UrlId(), 3);

  // Empty because URL was not validated.
  test_util::AssertUrlsInTable(backend_->db(), {});

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, UpdateSourceUrl) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const UrlId kUrlId3 = UkmUrlTable::GenerateUrlId(kUrl3);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId2);

  // Add 2 entries with associated source URLs.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 3);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 3);

  EXPECT_TRUE(backend_->has_transaction_for_testing());

  // Updating existing URL should replace existing entries with new URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl2, false, /*profile_id*/ "");
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 6);

  // Updating source URL again should those entries with the new URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl3, false, /*profile_id*/ "");
  ExpectEntriesWithUrlId(backend_->db(), kUrlId3, 3);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 3);

  // Empty because URL was not validated.
  test_util::AssertUrlsInTable(backend_->db(), {});

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, ValidatedUrl) {
  const GURL kUrl1("https://www.url1.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const ukm::SourceId kSourceId1 = 30;

  // Adding non-validated URL should not write to db.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  EXPECT_FALSE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  // Adding validated URL should write to db.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, true, /*profile_id*/ "");
  EXPECT_TRUE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  // Adding URL as non-validated again will not remove from db.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  EXPECT_TRUE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, UrlValidation) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;

  // Adding not-validated URL then validating it, should add URL to table.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  EXPECT_FALSE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));
  backend_->OnUrlValidated(kUrl1, /*profile_id*/ "");
  EXPECT_TRUE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  // Validating URL then adding URL with not-validated flag should not add to
  // table.
  backend_->OnUrlValidated(kUrl2, /*profile_id*/ "");
  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1}});
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false, /*profile_id*/ "");
  EXPECT_FALSE(backend_->url_table_for_testing().IsUrlInTable(kUrlId2));

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, RemoveUrls) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;
  const ukm::SourceId kSourceId3 = 30;
  const ukm::SourceId kSourceId4 = 40;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId2);
  ukm::mojom::UkmEntryPtr entry3 = GetSampleUkmEntry(kSourceId3);
  ukm::mojom::UkmEntryPtr entry4 = GetSampleUkmEntry(kSourceId4);

  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl3, false, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  backend_->StoreUkmEntry(std::move(entry3));
  backend_->StoreUkmEntry(std::move(entry4));
  backend_->OnUrlValidated(kUrl1, /*profile_id*/ "");
  backend_->OnUrlValidated(kUrl2, /*profile_id*/ "");

  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1},
                                UrlMatcher{.url_id = kUrlId2, .url = kUrl2}});
  DatabaseStats stats1 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats1.total_metrics, 12);
  EXPECT_EQ(stats1.metric_count_for_event_id.size(), 4u);
  EXPECT_EQ(stats1.metric_count_for_url_id.size(), 4u);

  // Removing URLs that were not added does nothing.
  backend_->RemoveUrls({GURL()}, /*all_urls=*/false);
  backend_->RemoveUrls({GURL("https://www.other.com")}, /*all_urls=*/false);

  DatabaseStats stats2 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats2.total_metrics, 12);
  EXPECT_EQ(stats2.metric_count_for_event_id.size(), 4u);
  EXPECT_EQ(stats2.metric_count_for_url_id.size(), 4u);

  EXPECT_TRUE(backend_->has_transaction_for_testing());

  // Removing non-validated URL removes from metrics table.
  backend_->RemoveUrls({kUrl3}, /*all_urls=*/false);
  DatabaseStats stats3 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats3.total_metrics, 9);
  EXPECT_EQ(stats3.metric_count_for_event_id.size(), 3u);
  EXPECT_THAT(stats3.metric_count_for_url_id,
              UnorderedElementsAre(std::make_pair(kUrlId1, 3),
                                   std::make_pair(kUrlId2, 3),
                                   std::make_pair(UrlId(), 3)));
  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1},
                                UrlMatcher{.url_id = kUrlId2, .url = kUrl2}});

  // Removing validated URL removes from url and metrics table.
  backend_->RemoveUrls({kUrl1, kUrl2}, /*all_urls=*/false);
  DatabaseStats stats4 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats4.total_metrics, 3);
  EXPECT_EQ(stats4.metric_count_for_event_id.size(), 1u);
  EXPECT_THAT(stats4.metric_count_for_url_id,
              UnorderedElementsAre(std::make_pair(UrlId(), 3)));
  test_util::AssertUrlsInTable(backend_->db(), {});

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, DestructorCommitsTransaction) {
  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry();
  backend_->StoreUkmEntry(std::move(entry1));
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry();
  backend_->StoreUkmEntry(std::move(entry2));
  EXPECT_TRUE(backend_->has_transaction_for_testing());

  // Delete the backend (database) and recreate it with same path.
  backend_.reset();
  CreateAndInitBackend();

  // The delete should have committed immediately.
  test_util::AssertUrlsInTable(backend_->db(), {});

  DatabaseStats stats = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats.metric_count_for_event_id.size(), 2u);
  EXPECT_EQ(stats.metric_count_for_event_id.begin()->second, 3);
  EXPECT_EQ(stats.metric_count_for_event_id.rbegin()->second, 3);
  EXPECT_THAT(stats.metric_count_for_url_id,
              ElementsAre(std::make_pair(UrlId(), 6)));
}

TEST_F(UkmDatabaseBackendTest, RemoveUrlsCommitsImmediately) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId2);

  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false, /*profile_id*/ "");
  backend_->OnUrlValidated(kUrl1, /*profile_id*/ "");
  backend_->OnUrlValidated(kUrl2, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));

  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1},
                                UrlMatcher{.url_id = kUrlId2, .url = kUrl2}});

  backend_->RemoveUrls({kUrl1, kUrl2}, /*all_urls=*/false);

  // Rollback transaction in progress and destroy the db, the URL should be
  // deleted from database.
  ASSERT_TRUE(backend_->has_transaction_for_testing());
  backend_->RollbackTransactionForTesting();

  // Delete the backend (database) and recreate it with same path.
  backend_.reset();
  CreateAndInitBackend();
  // The delete should have committed immediately.
  test_util::AssertUrlsInTable(backend_->db(), {});
}

TEST_F(UkmDatabaseBackendTest, DeleteAllUrls) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;
  const ukm::SourceId kSourceId3 = 30;
  const ukm::SourceId kSourceId4 = 40;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId2);
  ukm::mojom::UkmEntryPtr entry3 = GetSampleUkmEntry(kSourceId3);
  ukm::mojom::UkmEntryPtr entry4 = GetSampleUkmEntry(kSourceId4);

  // Delete on empty database does not crash.
  backend_->RemoveUrls({}, /*all_urls=*/true);

  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl3, false, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  backend_->StoreUkmEntry(std::move(entry3));
  backend_->StoreUkmEntry(std::move(entry4));

  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1},
                                UrlMatcher{.url_id = kUrlId2, .url = kUrl2}});
  DatabaseStats stats1 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats1.total_metrics, 12);
  EXPECT_EQ(stats1.metric_count_for_event_id.size(), 4u);
  EXPECT_EQ(stats1.metric_count_for_url_id.size(), 4u);

  // Only one event with 3 metrics and without URL is left.
  backend_->RemoveUrls({}, /*all_urls=*/true);
  test_util::AssertUrlsInTable(backend_->db(), {});
  DatabaseStats stats2 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats2.total_metrics, 3);
  EXPECT_EQ(stats2.metric_count_for_event_id.size(), 1u);
  const base::flat_map<UrlId, int> no_url_metrics({{UrlId(), 3}});
  EXPECT_EQ(stats2.metric_count_for_url_id, no_url_metrics);

  // Delete on table with all metrics without URL ID does nothing.
  backend_->RemoveUrls({}, /*all_urls=*/true);
  test_util::AssertUrlsInTable(backend_->db(), {});
  DatabaseStats stats3 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats3.total_metrics, 3);
  EXPECT_EQ(stats3.metric_count_for_event_id.size(), 1u);
  EXPECT_EQ(stats2.metric_count_for_url_id, no_url_metrics);

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, DeleteOldEntries) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const GURL kUrl4("https://www.url4.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const UrlId kUrlId3 = UkmUrlTable::GenerateUrlId(kUrl3);
  const UrlId kUrlId4 = UkmUrlTable::GenerateUrlId(kUrl4);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;
  const ukm::SourceId kSourceId3 = 30;
  const ukm::SourceId kSourceId4 = 40;
  const ukm::SourceId kSourceId5 = 50;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId2);
  ukm::mojom::UkmEntryPtr entry3 = GetSampleUkmEntry(kSourceId3);
  ukm::mojom::UkmEntryPtr entry4 = GetSampleUkmEntry(kSourceId4);

  UmaMetricEntry uma1 = GetSampleMetricsRow();

  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl3, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId4, kUrl1, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId5, kUrl4, true, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  backend_->StoreUkmEntry(std::move(entry3));
  backend_->StoreUkmEntry(std::move(entry4));
  backend_->AddUmaMetric("1", uma1);
  backend_->AddUmaMetric("2", uma1);
  backend_->AddUmaMetric("3", uma1);

  test_util::AssertUrlsInTable(backend_->db(),
                               {
                                   UrlMatcher{.url_id = kUrlId1, .url = kUrl1},
                                   UrlMatcher{.url_id = kUrlId2, .url = kUrl2},
                                   UrlMatcher{.url_id = kUrlId3, .url = kUrl3},
                                   UrlMatcher{.url_id = kUrlId4, .url = kUrl4},
                               });
  EXPECT_EQ(test_util::GetAllUmaMetrics(backend_->db()).size(), 3u);

  backend_->DeleteEntriesOlderThan(base::Time::Max());

  test_util::AssertUrlsInTable(backend_->db(), {});
  // UMA metrics are not deleted.
  EXPECT_EQ(test_util::GetAllUmaMetrics(backend_->db()).size(), 3u);

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

TEST_F(UkmDatabaseBackendTest, CleanupItems) {
  auto uma = GetSampleMetricsRow();
  uma.name_hash = 10;
  backend_->AddUmaMetric("1", uma);
  uma.name_hash = 20;
  backend_->AddUmaMetric("1", uma);
  uma.name_hash = 30;
  backend_->AddUmaMetric("1", uma);

  EXPECT_EQ(test_util::GetAllUmaMetrics(backend_->db()).size(), 3u);

  base::Time now = base::Time::Now();
  std::vector<CleanupItem> cleanup{
      CleanupItem(10, 0, uma.type, now),
      CleanupItem(15, 0, uma.type, now),
      CleanupItem(20, 0, uma.type, now),
  };
  // Wrong profile id.
  backend_->CleanupItems("2", cleanup);
  EXPECT_EQ(test_util::GetAllUmaMetrics(backend_->db()).size(), 3u);

  backend_->CleanupItems("1", cleanup);
  EXPECT_EQ(test_util::GetAllUmaMetrics(backend_->db()).size(), 1u);
}

TEST_F(UkmDatabaseBackendTest, ReadOnlyQueries) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;
  const ukm::SourceId kSourceId3 = 30;
  const ukm::SourceId kSourceId4 = 40;

  ukm::mojom::UkmEntryPtr entry1 = GetSampleUkmEntry(kSourceId1);
  ukm::mojom::UkmEntryPtr entry2 = GetSampleUkmEntry(kSourceId2);
  ukm::mojom::UkmEntryPtr entry3 = GetSampleUkmEntry(kSourceId3);
  ukm::mojom::UkmEntryPtr entry4 = GetSampleUkmEntry(kSourceId4);

  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, true, /*profile_id*/ "");
  base::Time after1 = base::Time::Now();
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl3, true, /*profile_id*/ "");
  backend_->UpdateUrlForUkmSource(kSourceId4, kUrl1, true, /*profile_id*/ "");
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  backend_->StoreUkmEntry(std::move(entry3));
  backend_->StoreUkmEntry(std::move(entry4));

  UkmDatabase::QueryList queries;
  queries.emplace(0, UkmDatabase::CustomSqlQuery(
                         "SELECT AVG(metric_value) FROM metrics",
                         std::vector<processing::ProcessedValue>()));
  ExpectQueryResult(std::move(queries), true,
                    {{0, {processing::ProcessedValue(101.00f)}}});

  constexpr char kBindValuesQuery[] =
      // clang-format off
      "SELECT CASE WHEN ? THEN SUM(metric_value) ELSE AVG(metric_value) END "
      "FROM metrics m "
      "LEFT JOIN urls u "
        "ON m.url_id = u.url_id "
      "WHERE "
        "ukm_source_id/2=? "
        "AND metric_hash=? "
        "AND url=? "
        "AND u.url_id=? "
        "AND event_timestamp>=?";
  // clang-format on
  std::vector<processing::ProcessedValue> bind_values{
      processing::ProcessedValue(true),
      processing::ProcessedValue(10.00),
      processing::ProcessedValue(std::string("1E")),
      processing::ProcessedValue(std::string("https://www.url2.com/")),
      processing::ProcessedValue(
          static_cast<int64_t>(kUrlId2.GetUnsafeValue())),
      processing::ProcessedValue(after1)};
  UkmDatabase::QueryList queries2;
  queries2.emplace(0, UkmDatabase::CustomSqlQuery(
                          "SELECT AVG(metric_value) FROM metrics "
                          "GROUP BY metric_hash ORDER BY metric_hash",
                          std::vector<processing::ProcessedValue>()));
  queries2.emplace(
      1, UkmDatabase::CustomSqlQuery(kBindValuesQuery, std::move(bind_values)));

  ExpectQueryResult(
      std::move(queries2), true,
      {{0,
        {ProcessedValue::FromFloat(100), ProcessedValue::FromFloat(101),
         ProcessedValue::FromFloat(102)}},
       {1, {ProcessedValue::FromFloat(100)}}});

  UkmDatabase::QueryList queries3;
  queries3.emplace(0, UkmDatabase::CustomSqlQuery("SELECT bad query", {}));
  ExpectQueryResult(std::move(queries3), false, {});

  UkmDatabase::QueryList queries4;
  queries4.emplace(
      0, UkmDatabase::CustomSqlQuery(
             "SELECT metric_value FROM metrics WHERE metric_hash=?", {}));
  ExpectQueryResult(std::move(queries4), false, {});

  UkmDatabase::QueryList queries5;
  queries5.emplace(0, UkmDatabase::CustomSqlQuery("DROP TABLE metrics", {}));
  ExpectQueryResult(std::move(queries5), false, {});

  // Database should not have changed.
  DatabaseStats stats = GetDatabaseStats(backend_->db());
  EXPECT_EQ(12, stats.total_metrics);
  EXPECT_EQ(4u, stats.metric_count_for_event_id.size());

  UkmDatabase::QueryList queries6;
  queries6.emplace(
      0, UkmDatabase::CustomSqlQuery(
             "INSERT INTO urls(url_id, url) VALUES(1,'not_added')", {}));
  ExpectQueryResult(std::move(queries6), false, {});

  // Database should not have changed.
  DatabaseStats stats1 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(12, stats1.total_metrics);
  EXPECT_EQ(4u, stats1.metric_count_for_event_id.size());

  EXPECT_TRUE(backend_->has_transaction_for_testing());
}

class FailedUkmDatabaseTest : public UkmDatabaseBackendTest {
 public:
  void SetUp() override {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    backend_ = std::make_unique<UkmDatabaseBackend>(
        base::FilePath(kBadFilePath), /*in_memory=*/false, task_runner_);
    base::RunLoop wait_for_init;
    backend_->InitDatabase(base::BindOnce(
        [](base::OnceClosure quit,
           scoped_refptr<base::SequencedTaskRunner> task_runner, bool success) {
          EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
          std::move(quit).Run();
          ASSERT_FALSE(success);
        },
        wait_for_init.QuitClosure(), task_runner_));
    wait_for_init.Run();
  }
  void TearDown() override {
    backend_.reset();
    task_runner_.reset();
  }
};

TEST_F(FailedUkmDatabaseTest, QueriesAreNoop) {
  // Check queries do not crash.
  const GURL kUrl1("https://www.url1.com");
  backend_->OnUrlValidated(kUrl1, /*profile_id*/ "");
  backend_->StoreUkmEntry(GetSampleUkmEntry());
  backend_->UpdateUrlForUkmSource(10, kUrl1, true, /*profile_id*/ "");
  backend_->RemoveUrls({kUrl1}, /*all_urls=*/false);
  backend_->RemoveUrls({kUrl1}, /*all_urls=*/true);
  backend_->DeleteEntriesOlderThan(base::Time() - base::Seconds(10));
  backend_->AddUmaMetric("1", GetSampleMetricsRow());
  backend_->CleanupItems("",
                         {CleanupItem(1, 2, proto::SignalType::HISTOGRAM_ENUM,
                                      base::Time() - base::Seconds(10))});

  UkmDatabase::QueryList queries;
  queries.emplace(0, UkmDatabase::CustomSqlQuery("SELECT bad query", {}));
  ExpectQueryResult(std::move(queries), false, {});
}

}  // namespace segmentation_platform
