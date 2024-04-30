// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/history_embeddings/history_embeddings_handler.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"

std::unique_ptr<KeyedService> BuildTestHistoryEmbeddingsService(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  CHECK(history_service);
  auto* page_content_annotations_service =
      PageContentAnnotationsServiceFactory::GetForProfile(profile);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return std::make_unique<history_embeddings::HistoryEmbeddingsService>(
      history_service, page_content_annotations_service,
      optimization_guide_keyed_service, nullptr);
}

std::unique_ptr<KeyedService> BuildTestPageContentAnnotationsService(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return page_content_annotations::TestPageContentAnnotationsService::Create(
      optimization_guide_keyed_service, history_service);
}

std::unique_ptr<KeyedService> BuildTestOptimizationGuideKeyedService(
    content::BrowserContext* browser_context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

class HistoryEmbeddingsHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        history_embeddings::kHistoryEmbeddings, {{"UseMlEmbedder", "false"}});

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    MockOptimizationGuideKeyedService::InitializeWithExistingTestLocalState();

    profile_ = profile_manager_->CreateTestingProfile(
        "History Embeddings Test User",
        {
            {HistoryServiceFactory::GetInstance(),
             HistoryServiceFactory::GetDefaultFactory()},
            {HistoryEmbeddingsServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestHistoryEmbeddingsService)},
            {PageContentAnnotationsServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestPageContentAnnotationsService)},
            {OptimizationGuideKeyedServiceFactory::GetInstance(),
             base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
        });

    handler_ = std::make_unique<HistoryEmbeddingsHandler>(
        mojo::PendingReceiver<history_embeddings::mojom::PageHandler>(),
        profile_->GetWeakPtr());
  }

  void TearDown() override { handler_.reset(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<HistoryEmbeddingsHandler> handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(HistoryEmbeddingsHandlerTest, Searches) {
  auto query = history_embeddings::mojom::SearchQuery::New();
  query->query = "search query for empty result";
  base::test::TestFuture<history_embeddings::mojom::SearchResultPtr> future;
  handler_->Search(std::move(query), future.GetCallback());
  auto result = future.Take();
  ASSERT_EQ(result->items.size(), 0u);
}
