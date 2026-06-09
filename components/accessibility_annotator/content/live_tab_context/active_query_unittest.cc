// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/live_tab_context/active_query.h"

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/live_tab_context/search.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/mock_page_content_services.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::page_content_annotations::CreateExtractionResult;
using ::page_content_annotations::CreatePassageEmbeddings;
using ::page_content_annotations::MockPageContentExtractionService;
using ::page_content_annotations::MockPageEmbeddingsService;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

namespace {

class ControllableTestEmbedder : public passage_embeddings::TestEmbedder {
 public:
  passage_embeddings::Embedder::Job ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override {
    uint64_t id = next_job_id_++;
    callbacks_.emplace(id, std::move(callback));
    passages_.emplace(std::move(passages));
    return Job(GetWeakPtr(), id);
  }

  void FireNextCallback(passage_embeddings::ComputeEmbeddingsStatus status) {
    ASSERT_FALSE(callbacks_.empty());
    std::pair<uint64_t, ComputePassagesEmbeddingsCallback> callback_info =
        std::move(callbacks_.front());
    std::vector<std::string> passages = std::move(passages_.front());
    callbacks_.pop();
    passages_.pop();

    std::vector<passage_embeddings::Embedding> embeddings;
    if (status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
      std::vector<float> data = {1.0f, 0.0f, 0.0f};
      embeddings.emplace_back(std::move(data));
    }
    std::move(callback_info.second)
        .Run(passages, std::move(embeddings), callback_info.first, status);
  }

 private:
  uint64_t next_job_id_ = 1;
  std::queue<std::pair<uint64_t, ComputePassagesEmbeddingsCallback>> callbacks_;
  std::queue<std::vector<std::string>> passages_;
};

}  // namespace

class ActiveQueryTest : public content::RenderViewHostTestHarness {
 public:
  ActiveQueryTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kAccessibilityAnnotatorLiveTabContext);
  }

  void TearDown() override {
    test_docs_.clear();
    content::RenderViewHostTestHarness::TearDown();
  }

  ActiveQuery::SearchablePage CreateSearchablePage(
      std::string text_content = "keyword passage with enough words",
      bool eligible = true) {
    test_docs_.push_back(CreateTestWebContents());
    return ActiveQuery::SearchablePage(test_docs_.back()->GetPrimaryPage(),
                          *CreateExtractionResult(text_content, eligible));
  }

  base::test::ScopedFeatureList feature_list_;
  NiceMock<MockPageContentExtractionService> test_extraction_service_;
  NiceMock<MockPageEmbeddingsService> test_embeddings_service_{
      &test_extraction_service_};
  passage_embeddings::TestEmbedder test_embedder_;
  std::vector<std::unique_ptr<content::WebContents>> test_docs_;
};

// Test that ActiveQuery successfully retrieves passages when both query and
// page embeddings are available.
TEST_F(ActiveQueryTest, Succeed) {
  base::test::TestFuture<std::vector<ScoredPassage>> future;
  ActiveQuery::SearchablePage page = CreateSearchablePage();
  content::Page* raw_page = page.GetPage();

  ON_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));

  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page));
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, test_embedder_);

  query.OnPageEmbeddingsAvailable(*raw_page);

  std::vector<ScoredPassage> results = future.Take();
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_EQ(u"semantic passage", results[0].passage);
}

// Test that ActiveQuery falls back to keyword matching if page embeddings are
// not available (e.g. they fail to compute or take too long).
TEST_F(ActiveQueryTest, PageEmbeddingFails) {
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillRepeatedly(
          Return(std::vector<page_content_annotations::PassageEmbedding>()));

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  ActiveQuery::SearchablePage page = CreateSearchablePage();

  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page));
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, test_embedder_);

  task_environment()->FastForwardBy(
      features::kAccessibilityAnnotatorLiveTabContextRequestTimeout.Get());

  std::vector<ScoredPassage> results = future.Take();
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_EQ(u"keyword passage with enough words", results[0].passage);
}

// Test that ActiveQuery falls back to keyword matching if the query embedding
// fails to compute.
TEST_F(ActiveQueryTest, QueryEmbeddingFails) {
  ActiveQuery::SearchablePage page = CreateSearchablePage();

  ControllableTestEmbedder controllable_embedder;
  base::test::TestFuture<std::vector<ScoredPassage>> future;

  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page));
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, controllable_embedder);

  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);

  std::vector<ScoredPassage> results = future.Take();
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_EQ(u"keyword passage with enough words", results[0].passage);
}

// Test that ActiveQuery can aggregate results from multiple documents
// concurrently.
TEST_F(ActiveQueryTest, MultipleDocuments) {
  ActiveQuery::SearchablePage page1 = CreateSearchablePage();
  content::Page* raw_page1 = page1.GetPage();

  ActiveQuery::SearchablePage page2 = CreateSearchablePage();
  content::Page* raw_page2 = page2.GetPage();

  ON_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page1));
  pages.push_back(std::move(page2));
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, test_embedder_);

  query.OnPageEmbeddingsAvailable(*raw_page1);
  query.OnPageEmbeddingsAvailable(*raw_page2);

  std::vector<ScoredPassage> results = future.Take();
  ASSERT_THAT(results, SizeIs(2));
}

// Test that ActiveQuery can handle a mix of success (semantic) and failure
// (fallback to keyword) across multiple documents in the same query.
TEST_F(ActiveQueryTest, MultipleDocuments_PartialFailure) {
  // --- 1. Setup Documents ---
  ActiveQuery::SearchablePage page1 = CreateSearchablePage();
  content::Page* raw_page1 = page1.GetPage();

  ActiveQuery::SearchablePage page2 = CreateSearchablePage();

  // --- 2. Configure Mock Expectations ---
  // We simulate a scenario where Page 1 has embeddings ready, but Page 2
  // doesn't. Page 2 will return an empty list of embeddings, which should
  // eventually trigger the keyword matching fallback.
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*raw_page1)))
      .WillRepeatedly(Return(CreatePassageEmbeddings("semantic passage")));
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*page2.GetPage())))
      .WillRepeatedly(
          Return(std::vector<page_content_annotations::PassageEmbedding>()));

  // --- 3. Start Query ---
  base::test::TestFuture<std::vector<ScoredPassage>> future;
  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page1));
  pages.push_back(std::move(page2));
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, test_embedder_);

  // --- 4. Dispatch Asynchronous Signals ---
  // Signal that Page 1 is ready for semantic search. Page 2 will stay pending
  // until the timeout triggers the fallback to keyword matching.
  query.OnPageEmbeddingsAvailable(*raw_page1);
  task_environment()->FastForwardBy(
      features::kAccessibilityAnnotatorLiveTabContextRequestTimeout.Get());

  // --- 5. Verification ---
  // We expect one result from semantic search (Page 1) and one from keyword
  // matching fallback (Page 2).
  EXPECT_THAT(future.Get(),
              ElementsAre(Field(&ScoredPassage::passage, u"semantic passage"),
                          Field(&ScoredPassage::passage,
                                u"keyword passage with enough words")));
}

// Test that ActiveQuery correctly truncates and sorts the final results based
// on the maximum configured limit across all processed documents.
TEST_F(ActiveQueryTest, MultipleDocuments_TruncatesResults) {
  base::test::TestFuture<std::vector<ScoredPassage>> future;

  const size_t max_results = static_cast<size_t>(
      features::kAccessibilityAnnotatorLiveTabContextMaxSearchResults.Get());

  std::vector<ActiveQuery::SearchablePage> pages;
  std::vector<content::Page*> raw_pages;
  for (size_t i = 0; i < max_results + 1; ++i) {
    pages.push_back(CreateSearchablePage("", true));
    raw_pages.push_back(pages.back().GetPage());
  }

  ON_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));

  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, test_embedder_);

  for (content::Page* raw_page : raw_pages) {
    query.OnPageEmbeddingsAvailable(*raw_page);
  }

  std::vector<ScoredPassage> results = future.Take();
  ASSERT_EQ(max_results, results.size());
}

// Test that ActiveQuery gracefully handles documents being closed (becoming
// invalid) while a query is still in flight.
TEST_F(ActiveQueryTest, DocumentClosedBeforeTimeout) {
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillRepeatedly(
          Return(std::vector<page_content_annotations::PassageEmbedding>()));

  base::test::TestFuture<std::vector<ScoredPassage>> future;

  ActiveQuery::SearchablePage page1 = CreateSearchablePage();
  ActiveQuery::SearchablePage page2 = CreateSearchablePage();

  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page1));
  pages.push_back(std::move(page2));
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, test_embedder_);

  // Destroy doc2 before the timeout triggers.
  test_docs_.pop_back();

  task_environment()->FastForwardBy(
      features::kAccessibilityAnnotatorLiveTabContextRequestTimeout.Get());

  std::vector<ScoredPassage> results = future.Take();
  EXPECT_LE(results.size(), 2u);
}

// Test that ActiveQuery correctly handles duplicate queries (same query+docs)
// and ensures that a failure in one does not "leak" to the other.
TEST_F(ActiveQueryTest, DuplicateQuery) {
  // --- 1. Setup Documents ---
  ActiveQuery::SearchablePage page1 = CreateSearchablePage();
  content::Page* raw_page1 = page1.GetPage();

  ActiveQuery::SearchablePage page2 = CreateSearchablePage();
  content::Page* raw_page2 = page2.GetPage();

  ControllableTestEmbedder controllable_embedder;
  base::test::TestFuture<std::vector<ScoredPassage>> future1;
  base::test::TestFuture<std::vector<ScoredPassage>> future2;

  // --- 2. Start First Query ---
  // This query only searches Page 1.
  std::vector<ActiveQuery::SearchablePage> pages1;
  pages1.push_back(std::move(page1));
  ActiveQuery query1(u"A", std::move(pages1), future1.GetCallback(),
                     test_embeddings_service_, controllable_embedder);

  // --- 3. Start Second Query (Overlap) ---
  // This query searches both Page 1 and Page 2. Note that we create "dupe"
  // handles to the same underlying raw pages.
  std::vector<ActiveQuery::SearchablePage> pages2;
  ActiveQuery::SearchablePage page1dupe(
      *raw_page1,
      *CreateExtractionResult("keyword passage with enough words", true));
  ActiveQuery::SearchablePage page2dupe(
      *raw_page2,
      *CreateExtractionResult("keyword passage with enough words", true));
  pages2.push_back(std::move(page1dupe));
  pages2.push_back(std::move(page2dupe));
  ActiveQuery query2(u"A", std::move(pages2), future2.GetCallback(),
                     test_embeddings_service_, controllable_embedder);

  // --- 4. Dispatch Asynchronous Signals ---
  // We simulate a race condition where query1 fails its embedding lookup,
  // but query2 succeeds.
  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);
  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  // Provide embeddings for the pages and signal availability.
  ON_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));
  query2.OnPageEmbeddingsAvailable(*raw_page1);
  query2.OnPageEmbeddingsAvailable(*raw_page2);

  // --- 5. Verification ---
  // EXPECTATION: Page 1 should NOT have fallen back to keyword matching for
  // query2 just because query1 failed. Both results in query2 should be
  // semantic search results.
  EXPECT_THAT(future2.Get(),
              ElementsAre(Field(&ScoredPassage::passage, u"semantic passage"),
                          Field(&ScoredPassage::passage, u"semantic passage")));
}

// Test the case where page embeddings are already available for every document
// before the query embedding even finishes.
TEST_F(ActiveQueryTest, EmbeddingsAlreadyAvailableAllDocs) {
  ActiveQuery::SearchablePage page1 = CreateSearchablePage("keyword passage 1");
  ActiveQuery::SearchablePage page2 = CreateSearchablePage("keyword passage 2");
  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page1));
  pages.push_back(std::move(page2));

  ON_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  ControllableTestEmbedder controllable_embedder;
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, controllable_embedder);
  // Omit query.OnPageEmbeddingsAvailable().

  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  EXPECT_THAT(future.Get(),
              ElementsAre(Field(&ScoredPassage::passage, u"semantic passage"),
                          Field(&ScoredPassage::passage, u"semantic passage")));
}

// Test the case where page embeddings are already available for all but one
// document when the query embedding finishes.
TEST_F(ActiveQueryTest, EmbeddingsAlreadyAvailableAllButOneDoc) {
  // --- 1. Setup Documents & Expectations ---
  // Page 1 has embeddings available immediately.
  ActiveQuery::SearchablePage page1 = CreateSearchablePage("keyword passage 1");
  content::Page* raw_page1 = page1.GetPage();
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*raw_page1)))
      .WillRepeatedly(Return(CreatePassageEmbeddings("semantic passage 1")));

  // Page 2 has embeddings available immediately.
  ActiveQuery::SearchablePage page2 = CreateSearchablePage("keyword passage 2");
  content::Page* raw_page2 = page2.GetPage();
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*raw_page2)))
      .WillRepeatedly(Return(CreatePassageEmbeddings("semantic passage 2")));

  // Page 3 does not have embeddings available initially, but will later.
  ActiveQuery::SearchablePage page3 = CreateSearchablePage("keyword passage 3");
  content::Page* raw_page3 = page3.GetPage();
  std::vector<page_content_annotations::PassageEmbedding> page3_embeddings;
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*raw_page3)))
      .WillRepeatedly(
          [&page3_embeddings](content::Page&) { return page3_embeddings; });

  // --- 2. Start Query ---
  std::vector<ActiveQuery::SearchablePage> pages;
  pages.push_back(std::move(page1));
  pages.push_back(std::move(page2));
  pages.push_back(std::move(page3));

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  ControllableTestEmbedder controllable_embedder;
  ActiveQuery query(u"passage", std::move(pages), future.GetCallback(),
                    test_embeddings_service_, controllable_embedder);

  // --- 3. Dispatch Asynchronous Signals ---
  // Simulate the query embedding finishing. Page 1 and Page 2 already have
  // embeddings available (as configured in step 1), so they should complete
  // semantic search immediately.
  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  // Page 3 still has no embeddings, so it must wait for this signal.
  // We populate its embeddings now.
  page3_embeddings = CreatePassageEmbeddings("semantic passage 3");

  // Note: We omit OnPageEmbeddingsAvailable() for Page 1 and Page 2 as they
  // should have finished during the FireNextCallback above.
  query.OnPageEmbeddingsAvailable(*raw_page3);

  // --- 4. Verification ---
  // All three pages should have successfully performed semantic search.
  EXPECT_THAT(
      future.Take(),
      UnorderedElementsAre(Field(&ScoredPassage::passage, u"semantic passage 1"),
                           Field(&ScoredPassage::passage, u"semantic passage 2"),
                           Field(&ScoredPassage::passage, u"semantic passage 3")));
}

// Test that ActiveQuery can be safely destroyed from within its own callback
// during a successful completion.
TEST_F(ActiveQueryTest, DestroyFromCallback_Success) {
  // Page embeddings are available.
  ON_CALL(test_embeddings_service_, GetEmbeddings(_))
      .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));

  std::vector<ActiveQuery::SearchablePage> searchable_pages;
  searchable_pages.push_back(CreateSearchablePage());
  searchable_pages.push_back(CreateSearchablePage());
  searchable_pages.push_back(CreateSearchablePage());

  std::vector<content::Page*> raw_pages;
  for (const auto& page : searchable_pages) {
    raw_pages.push_back(page.GetPage());
  }

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  std::unique_ptr<ActiveQuery> query;
  query = std::make_unique<ActiveQuery>(
      /*query=*/u"passage", /*pages=*/std::move(searchable_pages),
      /*callback=*/
      base::BindOnce(
          [](std::unique_ptr<ActiveQuery>* query_ptr,
             std::vector<ScoredPassage> results) {
            query_ptr->reset();
            return results;
          },
          &query).Then(future.GetCallback()),
      /*page_embeddings_service=*/test_embeddings_service_,
      /*embedder=*/test_embedder_);

  // Signal that the page embeddings are available.
  for (content::Page* raw_page : raw_pages) {
    query->OnPageEmbeddingsAvailable(*raw_page);
  }
  ASSERT_TRUE(future.Wait());  // Query runs and calls the callback.
  EXPECT_EQ(nullptr, query);  // Query is destroyed.
}

// Test that ActiveQuery can be safely destroyed from within its own callback
// during a timeout (fallback to keyword matching).
TEST_F(ActiveQueryTest, DestroyFromCallback_Timeout) {
  std::vector<ActiveQuery::SearchablePage> searchable_pages;
  for (int i = 0; i < 3; ++i) {
    searchable_pages.push_back(CreateSearchablePage());
  }

  // Page 1 embeddings remain pending (returns empty).
  // Pages 2 and 3 return embeddings immediately.
  content::Page* page1 = searchable_pages[0].GetPage();
  content::Page* page2 = searchable_pages[1].GetPage();
  content::Page* page3 = searchable_pages[2].GetPage();
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*page1)))
      .WillRepeatedly(
          Return(std::vector<page_content_annotations::PassageEmbedding>()));
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*page2)))
      .WillRepeatedly(Return(CreatePassageEmbeddings("semantic passage")));
  EXPECT_CALL(test_embeddings_service_, GetEmbeddings(Ref(*page3)))
      .WillRepeatedly(Return(CreatePassageEmbeddings("semantic passage")));

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  ControllableTestEmbedder controllable_embedder;
  std::unique_ptr<ActiveQuery> query;
  query = std::make_unique<ActiveQuery>(
      u"passage", std::move(searchable_pages),
      base::BindOnce(
          [](std::unique_ptr<ActiveQuery>* q, std::vector<ScoredPassage> r) {
            q->reset();
            return r;
          },
          &query)
          .Then(future.GetCallback()),
      test_embeddings_service_, controllable_embedder);

  // Complete query embedding. Pages 2 and 3 finish immediately.
  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  // Trigger timeout.
  // This should trigger Page 1's keyword search and destroy the ActiveQuery.
  task_environment()->FastForwardBy(
      features::kAccessibilityAnnotatorLiveTabContextRequestTimeout.Get());

  // ASAN testing should not crash; no UAFs.
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(nullptr, query);
}

// Test that ActiveQuery can be safely destroyed from within its own callback
// when a query embedding failure occurs.
TEST_F(ActiveQueryTest, DestroyFromCallback_QueryEmbeddingFails) {
  std::vector<ActiveQuery::SearchablePage> searchable_pages;
  searchable_pages.push_back(CreateSearchablePage());
  searchable_pages.push_back(CreateSearchablePage());
  searchable_pages.push_back(CreateSearchablePage());

  ControllableTestEmbedder controllable_embedder;
  base::test::TestFuture<std::vector<ScoredPassage>> future;
  std::unique_ptr<ActiveQuery> query;
  query = std::make_unique<ActiveQuery>(
      /*query=*/u"passage", /*pages=*/std::move(searchable_pages),
      /*callback=*/
      base::BindOnce(
          [](std::unique_ptr<ActiveQuery>* query_ptr,
             std::vector<ScoredPassage> results) {
            query_ptr->reset();
            return results;
          },
          &query)
          .Then(future.GetCallback()),
      /*page_embeddings_service=*/test_embeddings_service_,
      /*embedder=*/controllable_embedder);

  // The query embedding fails.
  controllable_embedder.FireNextCallback(
      passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);

  ASSERT_TRUE(future.Wait());  // Query runs and calls the callback.
  EXPECT_EQ(nullptr, query);   // Query is destroyed.
}

}  // namespace accessibility_annotator
