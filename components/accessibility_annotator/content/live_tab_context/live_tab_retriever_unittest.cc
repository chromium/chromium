// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/live_tab_context/live_tab_retriever.h"

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

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
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SizeIs;

class LiveTabRetrieverTest : public content::RenderViewHostTestHarness {
 public:
  LiveTabRetrieverTest() = default;

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kAccessibilityAnnotatorLiveTabContext);

    test_extraction_service_ =
        std::make_unique<NiceMock<MockPageContentExtractionService>>();
    test_embeddings_service_ =
        std::make_unique<NiceMock<MockPageEmbeddingsService>>(
            test_extraction_service_.get());
    test_embedder_ = std::make_unique<passage_embeddings::TestEmbedder>();

    ON_CALL(*test_extraction_service_,
            GetExtractedPageContentAndEligibilityForPage(_))
        .WillByDefault(Return(CreateExtractionResult(
            /*text_content=*/"keyword passage", /*eligible=*/true)));

    ON_CALL(*test_embeddings_service_, GetEmbeddings(_))
        .WillByDefault(Return(CreatePassageEmbeddings("semantic passage")));

    retriever_ = std::make_unique<LiveTabRetriever>(
        *test_extraction_service_, *test_embeddings_service_, *test_embedder_);
  }

  void TearDown() override {
    retriever_.reset();
    test_docs_.clear();
    test_embedder_.reset();
    test_embeddings_service_.reset();
    test_extraction_service_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  content::WebContents* CreateDoc() {
    test_docs_.push_back(CreateTestWebContents());
    return test_docs_.back().get();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NiceMock<MockPageContentExtractionService>>
      test_extraction_service_;
  std::unique_ptr<NiceMock<MockPageEmbeddingsService>> test_embeddings_service_;
  std::unique_ptr<passage_embeddings::TestEmbedder> test_embedder_;
  std::unique_ptr<LiveTabRetriever> retriever_;
  std::vector<std::unique_ptr<content::WebContents>> test_docs_;
};

TEST_F(LiveTabRetrieverTest, Retrieve_PageExtractionFails) {
  EXPECT_CALL(*test_extraction_service_,
              GetExtractedPageContentAndEligibilityForPage(_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*test_embeddings_service_, GetEmbeddings(_)).Times(0);

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  content::WebContents* doc = CreateDoc();
  retriever_->Retrieve(/*query=*/u"passage", {doc}, future.GetCallback());

  std::vector<ScoredPassage> results = future.Take();
  EXPECT_THAT(results, IsEmpty());
}

TEST_F(LiveTabRetrieverTest, Retrieve_PageNotEligible) {
  // If page is not eligible for server upload, semantic similarity is not
  // executed and keyword matching also fails.
  EXPECT_CALL(*test_extraction_service_,
              GetExtractedPageContentAndEligibilityForPage(_))
      .WillOnce(
          Return(CreateExtractionResult(/*text_content=*/"keyword passage",
                                        /*eligible=*/false)));
  EXPECT_CALL(*test_embeddings_service_, GetEmbeddings(_)).Times(0);

  base::test::TestFuture<std::vector<ScoredPassage>> future;
  content::WebContents* doc = CreateDoc();
  retriever_->Retrieve(/*query=*/u"passage", {doc}, future.GetCallback());

  std::vector<ScoredPassage> results = future.Take();
  EXPECT_THAT(results, IsEmpty());
}

TEST_F(LiveTabRetrieverTest, Retrieve_EmptyQuery) {
  content::WebContents* doc = CreateDoc();
  EXPECT_CALL(*test_extraction_service_,
              GetExtractedPageContentAndEligibilityForPage(_))
      .Times(0);
  EXPECT_CALL(*test_embeddings_service_, GetEmbeddings(_)).Times(0);

  // Retrieve should fail when query is empty or just whitespace.
  base::test::TestFuture<std::vector<ScoredPassage>> future_empty;
  retriever_->Retrieve(/*query=*/u"   ", {doc}, future_empty.GetCallback());
  std::vector<ScoredPassage> results_empty = future_empty.Take();
  EXPECT_THAT(results_empty, IsEmpty());
}

/* MULTIPLE DOCUMENTS */

TEST_F(LiveTabRetrieverTest, Retrieve_MultipleDocuments_MixedEligibility) {
  base::test::TestFuture<std::vector<ScoredPassage>> future;

  content::WebContents* doc_eligible = CreateDoc();
  EXPECT_CALL(*test_extraction_service_,
              GetExtractedPageContentAndEligibilityForPage(
                  Ref(doc_eligible->GetPrimaryPage())))
      .WillOnce(
          Return(CreateExtractionResult(/*text_content=*/"keyword passage",
                                        /*eligible=*/true)));

  content::WebContents* doc_failure = CreateDoc();
  EXPECT_CALL(*test_extraction_service_,
              GetExtractedPageContentAndEligibilityForPage(
                  Ref(doc_failure->GetPrimaryPage())))
      .WillOnce(Return(std::nullopt));

  content::WebContents* doc_ineligible = CreateDoc();
  EXPECT_CALL(*test_extraction_service_,
              GetExtractedPageContentAndEligibilityForPage(
                  Ref(doc_ineligible->GetPrimaryPage())))
      .WillOnce(
          Return(CreateExtractionResult(/*text_content=*/"keyword passage",
                                        /*eligible=*/false)));

  std::vector<content::WebContents*> docs = {doc_eligible, doc_failure,
                                             doc_ineligible};
  retriever_->Retrieve(/*query=*/u"passage", docs, future.GetCallback());

  retriever_->NotifyPageEmbeddingsAvailableForTesting(
      doc_eligible->GetPrimaryPage());
  std::vector<ScoredPassage> results = future.Take();
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_EQ(u"semantic passage", results[0].passage);
}

TEST_F(LiveTabRetrieverTest, Retrieve_CalledTwice_CancelsFirst) {
  base::test::TestFuture<std::vector<ScoredPassage>> future1;
  base::test::TestFuture<std::vector<ScoredPassage>> future2;

  content::WebContents* doc = CreateDoc();

  // Fire first request.
  retriever_->Retrieve(/*query=*/u"first", {doc}, future1.GetCallback());

  // Fire second request immediately, before the first completes.
  retriever_->Retrieve(/*query=*/u"second", {doc}, future2.GetCallback());

  // The first request should be immediately cancelled and return empty.
  EXPECT_THAT(future1.Take(), IsEmpty());

  // Simulate completion for the second request.
  retriever_->NotifyPageEmbeddingsAvailableForTesting(doc->GetPrimaryPage());

  // The second request should succeed.
  std::vector<ScoredPassage> results2 = future2.Take();
  ASSERT_THAT(results2, SizeIs(1));
}

// Test calling Retrieve twice with the same query and documents (duplicate
// request
TEST_F(LiveTabRetrieverTest, Retrieve_CalledTwice_DuplicateRequest) {
  base::test::TestFuture<std::vector<ScoredPassage>> future1;
  base::test::TestFuture<std::vector<ScoredPassage>> future2;

  content::WebContents* doc = CreateDoc();

  // Fire first request.
  retriever_->Retrieve(/*query=*/u"duplicate", {doc}, future1.GetCallback());

  // Fire second request with the SAME query AND SAME documents.
  retriever_->Retrieve(/*query=*/u"duplicate", {doc}, future2.GetCallback());

  // The first request should be immediately resolved with empty results.
  EXPECT_THAT(future1.Take(), IsEmpty());

  // Simulate completion for the second request.
  retriever_->NotifyPageEmbeddingsAvailableForTesting(doc->GetPrimaryPage());

  // The second request should succeed with results.
  std::vector<ScoredPassage> results2 = future2.Take();
  ASSERT_THAT(results2, SizeIs(1));
  EXPECT_EQ(u"semantic passage", results2[0].passage);
}

// Test that calling Retrieve twice with the SAME query but DIFFERENT documents
// results in the first request being cancelled and the second request starting
// fresh.
TEST_F(LiveTabRetrieverTest, Retrieve_CalledTwice_SameQueryDifferentDocuments) {
  base::test::TestFuture<std::vector<ScoredPassage>> future1;
  base::test::TestFuture<std::vector<ScoredPassage>> future2;

  content::WebContents* doc1 = CreateDoc();
  content::WebContents* doc2 = CreateDoc();

  // Fire first request for doc1.
  retriever_->Retrieve(/*query=*/u"query", {doc1}, future1.GetCallback());

  // Fire second request with same query but DIFFERENT document (doc2).
  retriever_->Retrieve(/*query=*/u"query", {doc2}, future2.GetCallback());

  // The first request should be immediately cancelled.
  EXPECT_THAT(future1.Take(), IsEmpty());

  // Simulate completion for the second request (doc2).
  retriever_->NotifyPageEmbeddingsAvailableForTesting(doc2->GetPrimaryPage());

  // The second request should succeed.
  std::vector<ScoredPassage> results2 = future2.Take();
  ASSERT_THAT(results2, SizeIs(1));
}

// Test that duplicate Retrieve calls succeed when nonoverlapping (i.e. the
// first has completed before the second is called).
TEST_F(LiveTabRetrieverTest, Retrieve_DuplicateQuery_Nonoverlapping) {
  base::test::TestFuture<std::vector<ScoredPassage>> future1;
  base::test::TestFuture<std::vector<ScoredPassage>> future2;

  content::WebContents* doc = CreateDoc();

  // Fire first request.
  retriever_->Retrieve(/*query=*/u"query", {doc}, future1.GetCallback());

  // Complete first request successfully.
  retriever_->NotifyPageEmbeddingsAvailableForTesting(doc->GetPrimaryPage());
  ASSERT_THAT(future1.Take(), SizeIs(1));

  // Fire second request with the same query. It should NOT hit duplicate
  // detection because the first one has already finished.
  retriever_->Retrieve(/*query=*/u"query", {doc}, future2.GetCallback());

  // Complete second request successfully.
  retriever_->NotifyPageEmbeddingsAvailableForTesting(doc->GetPrimaryPage());
  std::vector<ScoredPassage> results2 = future2.Take();
  ASSERT_THAT(results2, SizeIs(1));
}

}  // namespace accessibility_annotator
