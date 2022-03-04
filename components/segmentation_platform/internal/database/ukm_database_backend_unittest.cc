// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_backend.h"

#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/ukm_database_test_utils.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
namespace {

using ::segmentation_platform::test_util::UrlMatcher;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

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

}  // namespace

class UkmDatabaseBackendTest : public testing::Test {
 public:
  UkmDatabaseBackendTest() = default;
  ~UkmDatabaseBackendTest() override = default;

  void SetUp() override {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_ = std::make_unique<UkmDatabaseBackend>(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("ukm_database")),
        task_runner_);
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
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 3);

  // Adding more entries with source ID1 should use the existing URL ID.
  backend_->StoreUkmEntry(std::move(entry2));
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 6);

  // Updating URL for source ID2, then adding entries, should use the URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl2, false);
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
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false);
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false);
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  ExpectEntriesWithUrlId(backend_->db(), kUrlId1, 3);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 3);

  // Updating existing URL should replace existing entries with new URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl2, false);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 6);

  // Updating source URL again should those entries with the new URL ID.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl3, false);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId3, 3);
  ExpectEntriesWithUrlId(backend_->db(), kUrlId2, 3);

  // Empty because URL was not validated.
  test_util::AssertUrlsInTable(backend_->db(), {});
}

TEST_F(UkmDatabaseBackendTest, ValidatedUrl) {
  const GURL kUrl1("https://www.url1.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const ukm::SourceId kSourceId1 = 30;

  // Adding non-validated URL should not write to db.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false);
  EXPECT_FALSE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  // Adding validated URL should write to db.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, true);
  EXPECT_TRUE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  // Adding URL as non-validated again will not remove from db.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false);
  EXPECT_TRUE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));
}

TEST_F(UkmDatabaseBackendTest, UrlValidation) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const ukm::SourceId kSourceId1 = 10;
  const ukm::SourceId kSourceId2 = 20;

  // Adding not-validated URL then validating it, should add URL to table.
  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false);
  EXPECT_FALSE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));
  backend_->OnUrlValidated(kUrl1);
  EXPECT_TRUE(backend_->url_table_for_testing().IsUrlInTable(kUrlId1));

  // Validating URL then adding URL with not-validated flag should not add to
  // table.
  backend_->OnUrlValidated(kUrl2);
  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1}});
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false);
  EXPECT_FALSE(backend_->url_table_for_testing().IsUrlInTable(kUrlId2));
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

  backend_->UpdateUrlForUkmSource(kSourceId1, kUrl1, false);
  backend_->UpdateUrlForUkmSource(kSourceId2, kUrl2, false);
  backend_->UpdateUrlForUkmSource(kSourceId3, kUrl3, false);
  backend_->StoreUkmEntry(std::move(entry1));
  backend_->StoreUkmEntry(std::move(entry2));
  backend_->StoreUkmEntry(std::move(entry3));
  backend_->StoreUkmEntry(std::move(entry4));
  backend_->OnUrlValidated(kUrl1);
  backend_->OnUrlValidated(kUrl2);

  test_util::AssertUrlsInTable(backend_->db(),
                               {UrlMatcher{.url_id = kUrlId1, .url = kUrl1},
                                UrlMatcher{.url_id = kUrlId2, .url = kUrl2}});
  DatabaseStats stats1 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats1.total_metrics, 12);
  EXPECT_EQ(stats1.metric_count_for_event_id.size(), 4u);
  EXPECT_EQ(stats1.metric_count_for_url_id.size(), 4u);

  // Removing URLs that were not added does nothing.
  backend_->RemoveUrls({GURL()});
  backend_->RemoveUrls({GURL("https://www.other.com")});

  DatabaseStats stats2 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats2.total_metrics, 12);
  EXPECT_EQ(stats2.metric_count_for_event_id.size(), 4u);
  EXPECT_EQ(stats2.metric_count_for_url_id.size(), 4u);

  // Removing non-validated URL removes from metrics table.
  backend_->RemoveUrls({kUrl3});
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
  backend_->RemoveUrls({kUrl1, kUrl2});
  DatabaseStats stats4 = GetDatabaseStats(backend_->db());
  EXPECT_EQ(stats4.total_metrics, 3);
  EXPECT_EQ(stats4.metric_count_for_event_id.size(), 1u);
  EXPECT_THAT(stats4.metric_count_for_url_id,
              UnorderedElementsAre(std::make_pair(UrlId(), 3)));
  test_util::AssertUrlsInTable(backend_->db(), {});
}

}  // namespace segmentation_platform
