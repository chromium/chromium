// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_category_classifier_bridge_impl.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/category_classifier_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/page_content_annotations/content/producer.h"
#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

using ::testing::IsEmpty;

constexpr size_t kHalfModelInputDims = 384;

template <typename T>
class FakeProducer : public Producer<T> {
 public:
  base::CallbackListSubscription Subscribe(
      ConsumerCallback<T> callback) override {
    return callbacks_.Add(std::move(callback));
  }

  bool HasSubscribers() const { return !callbacks_.empty(); }

  void Notify(content::Page& page, const T& payload) {
    callbacks_.Notify(page, payload);
  }

 private:
  void OnSubscribersChanged() override {}

  base::RepeatingCallbackList<void(content::Page&, const T&)> callbacks_;
};

passage_embeddings::Embedding MakeUnitEmbedding(size_t one_hot_index) {
  std::vector<float> data(kHalfModelInputDims, 0.0f);
  data[one_hot_index] = 1.0f;
  return passage_embeddings::Embedding(std::move(data));
}

std::vector<PassageEmbedding> CreateTestEmbeddings() {
  return {
      {{"title and url", EmbeddingPassageType::kTitleAndUrl},
       MakeUnitEmbedding(0)},
      {{"body passage", EmbeddingPassageType::kPageContent},
       MakeUnitEmbedding(1)},
  };
}

class PageCategoryClassifierBridgeImplTest
    : public content::RenderViewHostTestHarness {
 public:
  PageCategoryClassifierBridgeImplTest() {
    content::InitBackForwardCacheFeature(&feature_list_,
                                         /*enable_back_forward_cache=*/true);
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    classifier_ = std::make_unique<OnDeviceCategoryClassifier>(
        &model_provider_, &embedder_metadata_provider_);
    bridge_ = std::make_unique<PageCategoryClassifierBridgeImpl>(
        embeddings_producer_, *classifier_);
  }

  void TearDown() override {
    bridge_.reset();
    classifier_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void PushEduModelWithConcatenation() {
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    optimization_guide::proto::CategoryClassifierMetadata metadata;
    metadata.set_required_embedder_version(
        passage_embeddings::kEmbeddingsModelVersion);
    auto* concat_strategy =
        metadata.mutable_passage_embedding_concatenation_strategy();
    concat_strategy->set_max_passages(1);
    concat_strategy->set_pooling_strategy(
        optimization_guide::proto::
            CategoryClassifierPassageEmbeddingConcatenationStrategy::
                POOLING_STRATEGY_MAX);
    model_provider_.UpdateModelImmediatelyForTesting(
        optimization_guide::proto::OPTIMIZATION_TARGET_EDU_CLASSIFIER,
        optimization_guide::ModelInfo{
            .model_file_path =
                test_data_dir
                    .AppendASCII(
                        "components/test/data/page_content_annotations")
                    .AppendASCII("edu_classifier.tflite"),
            .model_metadata = optimization_guide::AnyWrapProto(metadata),
        });
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  FakeProducer<std::vector<PassageEmbedding>> embeddings_producer_;
  passage_embeddings::TestEmbedderMetadataProvider embedder_metadata_provider_;
  OptimizationGuideLogger optimization_guide_logger_;
  optimization_guide::ModelProviderRegistry model_provider_{
      &optimization_guide_logger_};
  std::unique_ptr<OnDeviceCategoryClassifier> classifier_;
  std::unique_ptr<PageCategoryClassifierBridgeImpl> bridge_;
};

TEST_F(PageCategoryClassifierBridgeImplTest, EndToEndSubscriptionAndDelivery) {
  PushEduModelWithConcatenation();

  EXPECT_FALSE(embeddings_producer_.HasSubscribers());

  base::test::TestFuture<const GURL&, ukm::SourceId,
                         const std::vector<Category>&>
      future;
  base::CallbackListSubscription subscription =
      bridge_->Subscribe(future.GetRepeatingCallback());
  EXPECT_TRUE(embeddings_producer_.HasSubscribers());

  const GURL kUrl("https://example.com/shop");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kUrl);

  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());

  auto [url, source_id, categories] = future.Take();
  EXPECT_EQ(kUrl, url);
  EXPECT_EQ(
      web_contents()->GetPrimaryPage().GetMainDocument().GetPageUkmSourceId(),
      source_id);
  ASSERT_EQ(1u, categories.size());
  EXPECT_EQ(CategoryType::kEducation, categories[0].category_type);
}

TEST_F(PageCategoryClassifierBridgeImplTest,
       PartitionsTitleUrlAndPassageEmbeddings) {
  PushEduModelWithConcatenation();

  base::test::TestFuture<const GURL&, ukm::SourceId,
                         const std::vector<Category>&>
      future;
  base::CallbackListSubscription subscription =
      bridge_->Subscribe(future.GetRepeatingCallback());

  const GURL kUrl("https://example.com/edu");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kUrl);

  // Without a kTitleAndUrl embedding, non-kTitleAndUrl passages do not satisfy
  // the title-URL requirement and classification produces no categories.
  embeddings_producer_.Notify(
      web_contents()->GetPrimaryPage(),
      {
          {{"title only", EmbeddingPassageType::kTitle}, MakeUnitEmbedding(0)},
          {{"body passage", EmbeddingPassageType::kPageContent},
           MakeUnitEmbedding(1)},
      });
  auto [missing_title_url, missing_title_source_id, missing_title_categories] =
      future.Take();
  EXPECT_THAT(missing_title_categories, IsEmpty());

  // With kTitleAndUrl (and an ignored kTitle passage), classification executes
  // with zero-padded passage embeddings.
  embeddings_producer_.Notify(
      web_contents()->GetPrimaryPage(),
      {
          {{"title and url", EmbeddingPassageType::kTitleAndUrl},
           MakeUnitEmbedding(0)},
          {{"ignored title", EmbeddingPassageType::kTitle},
           MakeUnitEmbedding(2)},
      });
  auto [title_only_url, title_only_source_id, title_only_categories] =
      future.Take();
  ASSERT_EQ(1u, title_only_categories.size());

  // Adding a kPageContent passage forwards it into `passage_embeddings`,
  // changing the concatenated classifier input and resulting score.
  embeddings_producer_.Notify(
      web_contents()->GetPrimaryPage(),
      {
          {{"title and url", EmbeddingPassageType::kTitleAndUrl},
           MakeUnitEmbedding(0)},
          {{"body passage", EmbeddingPassageType::kPageContent},
           MakeUnitEmbedding(2)},
      });
  auto [full_url, full_source_id, full_categories] = future.Take();
  ASSERT_EQ(1u, full_categories.size());
  EXPECT_NE(title_only_categories[0].score, full_categories[0].score);
}

TEST_F(PageCategoryClassifierBridgeImplTest, SubscriptionTogglesPipeline) {
  int notification_count = 0;
  auto make_callback = [&notification_count]() {
    return base::BindRepeating(
        [](int* out_count, const GURL&, ukm::SourceId,
           const std::vector<Category>&) { (*out_count)++; },
        &notification_count);
  };

  std::optional<base::CallbackListSubscription> sub1 =
      bridge_->Subscribe(make_callback());
  std::optional<base::CallbackListSubscription> sub2 =
      bridge_->Subscribe(make_callback());
  EXPECT_TRUE(embeddings_producer_.HasSubscribers());

  sub1.reset();
  EXPECT_TRUE(embeddings_producer_.HasSubscribers());

  sub2.reset();
  EXPECT_FALSE(embeddings_producer_.HasSubscribers());

  const GURL kUrl("https://example.com/inactive");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kUrl);

  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());
  EXPECT_EQ(0, notification_count);

  // Re-subscribing resumes pipeline delivery.
  sub1 = bridge_->Subscribe(make_callback());
  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());
  EXPECT_EQ(1, notification_count);
}

TEST_F(PageCategoryClassifierBridgeImplTest,
       UnsubscribeCancelsInFlightClassificationForNewSubscriber) {
  PushEduModelWithConcatenation();

  base::test::TestFuture<const GURL&, ukm::SourceId,
                         const std::vector<Category>&>
      first_future;
  std::optional<base::CallbackListSubscription> first_sub =
      bridge_->Subscribe(first_future.GetRepeatingCallback());

  const GURL kFirstUrl("https://example.com/first");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kFirstUrl);
  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());

  // Dropping the last subscriber while inference is in flight invalidates
  // pending weak pointers so a new subscriber does not receive stale results.
  first_sub.reset();

  base::test::TestFuture<const GURL&, ukm::SourceId,
                         const std::vector<Category>&>
      second_future;
  base::CallbackListSubscription second_sub =
      bridge_->Subscribe(second_future.GetRepeatingCallback());

  const GURL kSecondUrl("https://example.com/second");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kSecondUrl);
  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());

  auto [url, source_id, categories] = second_future.Take();
  EXPECT_EQ(kSecondUrl, url);
  EXPECT_FALSE(first_future.IsReady());
}

TEST_F(PageCategoryClassifierBridgeImplTest,
       CapturesUrlAndSourceIdBeforeSameDocumentNavigation) {
  PushEduModelWithConcatenation();

  base::test::TestFuture<const GURL&, ukm::SourceId,
                         const std::vector<Category>&>
      future;
  base::CallbackListSubscription subscription =
      bridge_->Subscribe(future.GetRepeatingCallback());

  const GURL kInitialUrl("https://example.com/page1");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kInitialUrl);
  const ukm::SourceId initial_source_id =
      web_contents()->GetPrimaryPage().GetMainDocument().GetPageUkmSourceId();

  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());

  // Commit a same-document navigation while background TFLite inference is in
  // flight.
  const GURL kSameDocUrl("https://example.com/page2");
  content::NavigationSimulator::CreateRendererInitiated(kSameDocUrl, main_rfh())
      ->CommitSameDocument();
  ASSERT_EQ(
      kSameDocUrl,
      web_contents()->GetPrimaryPage().GetMainDocument().GetLastCommittedURL());

  auto [url, source_id, categories] = future.Take();
  EXPECT_EQ(kInitialUrl, url);
  EXPECT_EQ(initial_source_id, source_id);
  ASSERT_EQ(1u, categories.size());
  EXPECT_EQ(CategoryType::kEducation, categories[0].category_type);
}

TEST_F(PageCategoryClassifierBridgeImplTest,
       SuppressesNotificationWhenPageBecomesNonPrimary) {
  PushEduModelWithConcatenation();

  base::test::TestFuture<const GURL&, ukm::SourceId,
                         const std::vector<Category>&>
      future;
  base::CallbackListSubscription subscription =
      bridge_->Subscribe(future.GetRepeatingCallback());

  const GURL kFirstUrl("https://example.com/first");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kFirstUrl);
  base::WeakPtr<content::Page> first_page =
      web_contents()->GetPrimaryPage().GetWeakPtr();
  embeddings_producer_.Notify(*first_page, CreateTestEmbeddings());

  // Cross-site navigation moves `first_page` into BackForwardCache so its
  // WeakPtr remains valid while `IsPrimary()` becomes false.
  const GURL kSecondUrl("https://other.com/second");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kSecondUrl);
  ASSERT_TRUE(first_page);
  ASSERT_FALSE(first_page->IsPrimary());

  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());

  auto [url, source_id, categories] = future.Take();
  EXPECT_EQ(kSecondUrl, url);
}

}  // namespace

}  // namespace page_content_annotations
