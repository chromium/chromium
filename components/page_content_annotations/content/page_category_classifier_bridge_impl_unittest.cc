// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_category_classifier_bridge_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/page_content_annotations/content/producer.h"
#include "components/page_content_annotations/core/on_device_category_classifier.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

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
  void OnDemandChanged() override {}

  base::RepeatingCallbackList<void(content::Page&, const T&)> callbacks_;
};

std::vector<PassageEmbedding> CreateTestEmbeddings() {
  return {
      {{"title and url", EmbeddingPassageType::kTitleAndUrl},
       passage_embeddings::Embedding({1.0f, 0.0f})},
      {{"body passage", EmbeddingPassageType::kPageContent},
       passage_embeddings::Embedding({0.0f, 1.0f})},
  };
}

class ClassifierObserver : public OnDeviceCategoryClassifier::Observer {
 public:
  void OnCategoriesClassified(
      const GURL& url,
      ukm::SourceId source_id,
      const std::vector<Category>& categories) override {
    last_url_ = url;
    last_source_id_ = source_id;
    notification_count_++;
  }

  const GURL& last_url() const { return last_url_; }
  ukm::SourceId last_source_id() const { return last_source_id_; }
  int notification_count() const { return notification_count_; }

 private:
  GURL last_url_;
  ukm::SourceId last_source_id_ = ukm::kInvalidSourceId;
  int notification_count_ = 0;
};

class PageCategoryClassifierBridgeImplTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    classifier_ = std::make_unique<OnDeviceCategoryClassifier>(
        model_provider_.get(), /*embedder_metadata_provider=*/nullptr);
    bridge_ = std::make_unique<PageCategoryClassifierBridgeImpl>(
        embeddings_producer_, *classifier_);
  }

  void TearDown() override {
    bridge_.reset();
    classifier_.reset();
    model_provider_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  FakeProducer<std::vector<PassageEmbedding>> embeddings_producer_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
  std::unique_ptr<OnDeviceCategoryClassifier> classifier_;
  std::unique_ptr<PageCategoryClassifierBridgeImpl> bridge_;
};

TEST_F(PageCategoryClassifierBridgeImplTest, EndToEndSubscriptionAndDelivery) {
  ClassifierObserver observer;
  classifier_->AddObserver(&observer);

  EXPECT_FALSE(embeddings_producer_.HasSubscribers());

  bridge_->SetDemandActive(true);
  EXPECT_TRUE(embeddings_producer_.HasSubscribers());

  const GURL kUrl("https://example.com/shop");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kUrl);

  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());

  EXPECT_EQ(kUrl, observer.last_url());
  EXPECT_EQ(
      web_contents()->GetPrimaryPage().GetMainDocument().GetPageUkmSourceId(),
      observer.last_source_id());
  EXPECT_EQ(1, observer.notification_count());

  classifier_->RemoveObserver(&observer);
}

TEST_F(PageCategoryClassifierBridgeImplTest, SetDemandActiveTogglesPipeline) {
  ClassifierObserver observer;
  classifier_->AddObserver(&observer);

  bridge_->SetDemandActive(true);
  // Calling SetDemandActive(true) again should be idempotent.
  bridge_->SetDemandActive(true);
  EXPECT_TRUE(embeddings_producer_.HasSubscribers());

  bridge_->SetDemandActive(false);
  EXPECT_FALSE(embeddings_producer_.HasSubscribers());

  const GURL kUrl("https://example.com/inactive");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kUrl);

  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());
  EXPECT_EQ(0, observer.notification_count());

  // Re-enabling demand resumes pipeline delivery.
  bridge_->SetDemandActive(true);
  embeddings_producer_.Notify(web_contents()->GetPrimaryPage(),
                              CreateTestEmbeddings());
  EXPECT_EQ(1, observer.notification_count());

  classifier_->RemoveObserver(&observer);
}

}  // namespace

}  // namespace page_content_annotations
