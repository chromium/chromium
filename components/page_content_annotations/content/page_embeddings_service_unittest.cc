// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_embeddings_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

using testing::AnyNumber;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;

namespace page_content_annotations {

using Candidates = std::vector<std::pair<std::string, EmbeddingPassageType>>;

Candidates GenerateCandidates(const PageContent& page_content,
                              size_t page_content_passages_to_generate,
                              const std::string& title,
                              const std::string& url) {
  if (!IsPageContentValid(page_content)) {
    return Candidates{};
  }

  return std::visit(
      absl::Overload{
          [](RefCountedAnnotatedPageContentPtr annotated_page_content_ptr) {
            if (annotated_page_content_ptr->data.main_frame_data().title() ==
                "EMPTY") {
              return Candidates{};
            }

            return Candidates{std::make_pair(
                annotated_page_content_ptr->data.main_frame_data().title(),
                EmbeddingPassageType::kTitle)};
          },
          [](RefCountedPDFTextPtr pdf_text_ptr) {
            return Candidates{std::make_pair(
                pdf_text_ptr->data, EmbeddingPassageType::kPageContent)};
          },
      },
      page_content);
}

class EmbedderMock : public passage_embeddings::TestEmbedder {
 public:
  MOCK_METHOD(passage_embeddings::Embedder::Job,
              ComputePassagesEmbeddings,
              (passage_embeddings::PassagePriority priority,
               std::vector<std::string> passages,
               passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                   callback),
              (override));

  MOCK_METHOD(void,
              ReprioritizeJobs,
              (passage_embeddings::PassagePriority priority,
               const std::set<uint64_t>& job_ids),
              (override));

  MOCK_METHOD(bool, TryCancel, (uint64_t job_id), (override));
};

class ObserverMock : public PageEmbeddingsService::Observer {
 public:
  MOCK_METHOD(PageEmbeddingsService::Priority, GetDefaultPriority, (), (const));
  MOCK_METHOD(PageEmbeddingsService::UsageMode, GetUsageMode, (), (const));

  MOCK_METHOD(void,
              OnPageEmbeddingsAvailable,
              (content::Page & page),
              (override));
};

class PageEmbeddingsServiceTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
    page_content_extraction_service_.emplace(os_crypt_async_.get(),
                                             GetBrowserContext()->GetPath(),
                                             /*tracker=*/nullptr);

    page_embeddings_service_.emplace(base::BindRepeating(&GenerateCandidates),
                                     &page_content_extraction_service_.value(),
                                     &embedder_mock_,
                                     /*embedder_metadata_provider=*/nullptr);

    ON_CALL(embedder_mock_, ComputePassagesEmbeddings)
        .WillByDefault(
            [this](
                passage_embeddings::PassagePriority priority,
                std::vector<std::string> passages,
                passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                    callback) {
              return embedder_mock_
                  .passage_embeddings::TestEmbedder::ComputePassagesEmbeddings(
                      priority, std::move(passages), std::move(callback));
            });
    EXPECT_CALL(embedder_mock_, TryCancel(testing::_))
        .Times(testing::AnyNumber());
  }

  void TearDown() override {
    page_embeddings_service_.reset();
    page_content_extraction_service_.reset();
    os_crypt_async_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<content::TestBrowserContext>();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContentsWithVisibility(
      content::Visibility visibility) {
    std::unique_ptr<content::WebContents> web_contents =
        CreateTestWebContents();
    // WebContents won't actually set visibility to a non-visible state until
    // it's first set to visible.
    if (visibility != content::Visibility::VISIBLE) {
      web_contents->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
    }
    web_contents->UpdateWebContentsVisibility(visibility);
    return web_contents;
  }

  PageEmbeddingsService& page_embeddings_service() {
    CHECK(page_embeddings_service_.has_value());
    return *page_embeddings_service_;
  }

  EmbedderMock& embedder_mock() { return embedder_mock_; }

 protected:
  testing::NiceMock<EmbedderMock> embedder_mock_;

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::optional<PageContentExtractionService> page_content_extraction_service_;
  std::optional<PageEmbeddingsService> page_embeddings_service_;
};

// Validates that candidate passages are generated from AnnotatedPageContent.
TEST_F(PageEmbeddingsServiceTest, GeneratesCandidatePassages) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);
  scoped_refptr<RefCountedAnnotatedPageContent> page_content =
      base::MakeRefCounted<RefCountedAnnotatedPageContent>();
  page_content->data.mutable_main_frame_data()->set_title("passage text");

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [this](passage_embeddings::PassagePriority priority,
                 std::vector<std::string> passages,
                 passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                     callback) {
            EXPECT_THAT(passages, ElementsAre("passage text"));
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(), page_content);
}

// Validates that the observer is notified on the generation of new embeddings
// for a WebContents.
TEST_F(PageEmbeddingsServiceTest, NotifiesObserver) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  page_embeddings_service().AddObserver(&observer);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(
                            testing::Ref(web_contents->GetPrimaryPage())));

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the observer is not notified if the WebContents associated
// with the passages is destroyed before the embeddings could be computed.
TEST_F(PageEmbeddingsServiceTest,
       DoesntNotifyObserverIfWebContentsIsDestroyed) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  page_embeddings_service().AddObserver(&observer);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(testing::_)).Times(0);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  web_contents.reset();

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that embeddings can be retrieved after they are computed.
TEST_F(PageEmbeddingsServiceTest, GetEmbeddings) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  EXPECT_THAT(
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage()),
      IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage());
  ASSERT_EQ(1u, embeddings.size());
  EXPECT_EQ("passage text", embeddings[0].passage.first);
  EXPECT_EQ(EmbeddingPassageType::kTitle, embeddings[0].passage.second);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

// Validates that embeddings can be retrieved after they are computed.
TEST_F(PageEmbeddingsServiceTest, EmbeddingsNotPresentOnError) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  EXPECT_THAT(
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage()),
      IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage());
  EXPECT_TRUE(embeddings.empty());
}

// Validates that seeing new page contents while embeddings are still pending
// results in canceling the previous embedding job.
TEST_F(PageEmbeddingsServiceTest, NewPageContentCancelsExistingEmbeddingJob) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  // Return the job id and don't compute the embeddings.
  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([this](auto, auto, auto) {
        return passage_embeddings::Embedder::Job(embedder_mock_.GetWeakPtr(),
                                                 1);
      });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(2);

  EXPECT_THAT(
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage()),
      IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([this](auto, auto, auto) {
        return passage_embeddings::Embedder::Job(embedder_mock_.GetWeakPtr(),
                                                 2);
      });
  EXPECT_CALL(embedder_mock(), TryCancel(1));

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());
}

// Validates that providing embeddings after destroying the WebContents does not
// crash.
TEST_F(PageEmbeddingsServiceTest, DoesNotCrashOnWebContentsDestroyed) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  EXPECT_THAT(
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage()),
      IsEmpty());

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  web_contents.reset();

  std::move(compute_passages_embeddings_callback)
      .Run({""}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
}

// Validates that the cancelled embeddings are ignored, even if received due to
// already being returned asynchronously.
TEST_F(PageEmbeddingsServiceTest, CancelledEmbeddingsAreIgnored) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback1;
  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback2;

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(2);

  EXPECT_THAT(
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage()),
      IsEmpty());

  EXPECT_CALL(embedder_mock(), TryCancel(1));

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback1 = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback2 = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 2);
          });

  // Providing page content a second time should try to cancel the first
  // embedding computation.
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  std::move(compute_passages_embeddings_callback1)
      .Run({"passage text 1"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  EXPECT_TRUE(page_embeddings_service()
                  .GetEmbeddings(web_contents->GetPrimaryPage())
                  .empty());

  std::move(compute_passages_embeddings_callback2)
      .Run({"passage text 2"}, {passage_embeddings::Embedding({1.0f})}, 2,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage());
  ASSERT_EQ(1u, embeddings.size());
  EXPECT_EQ("passage text 2", embeddings[0].passage.first);
  EXPECT_EQ(EmbeddingPassageType::kTitle, embeddings[0].passage.second);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

TEST_F(PageEmbeddingsServiceTest, DoesNotCrashOnCancel) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback1;
  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback2;

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(2);

  EXPECT_THAT(
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage()),
      IsEmpty());

  EXPECT_CALL(embedder_mock(), TryCancel(1));

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback1 = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback2 = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 2);
          });

  // Providing page content a second time should try to cancel the first
  // embedding computation.
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Mimic real cancelling.
  std::move(compute_passages_embeddings_callback1)
      .Run({"passage text 1"}, {}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kCanceled);

  EXPECT_TRUE(page_embeddings_service()
                  .GetEmbeddings(web_contents->GetPrimaryPage())
                  .empty());

  std::move(compute_passages_embeddings_callback2)
      .Run({"passage text 2"}, {passage_embeddings::Embedding({1.0f})}, 2,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  std::vector<PassageEmbedding> embeddings =
      page_embeddings_service().GetEmbeddings(web_contents->GetPrimaryPage());
  ASSERT_EQ(1u, embeddings.size());
  EXPECT_EQ("passage text 2", embeddings[0].passage.first);
  EXPECT_EQ(EmbeddingPassageType::kTitle, embeddings[0].passage.second);
  EXPECT_THAT(embeddings[0].embedding.GetData(), ElementsAre(1.0f));
}

// Validates that the embeddings are computed with the priority of the highest
// priority observer.
TEST_F(PageEmbeddingsServiceTest, PrioritySetBasedOnHighestPriorityObserver) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer_urgent;
  EXPECT_CALL(observer_urgent, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUrgent));
  EXPECT_CALL(observer_urgent, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  ObserverMock observer_user_blocking;
  EXPECT_CALL(observer_user_blocking, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUserBlocking));
  EXPECT_CALL(observer_user_blocking, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), TryCancel).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), ReprioritizeJobs).Times(AnyNumber());

  const auto set_priority_expectation =
      [this](passage_embeddings::PassagePriority expected_priority) {
        ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
            .WillByDefault([this, expected_priority](
                               passage_embeddings::PassagePriority priority,
                               std::vector<std::string> passages,
                               passage_embeddings::Embedder::
                                   ComputePassagesEmbeddingsCallback callback) {
              EXPECT_EQ(expected_priority, priority);
              return passage_embeddings::Embedder::Job(
                  embedder_mock_.GetWeakPtr(), 1);
            });
      };

  // With no observers the priority should be the default.
  set_priority_expectation(passage_embeddings::kPassive);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Adding an urgent observer should raise the priority.
  page_embeddings_service().AddObserver(&observer_urgent);

  set_priority_expectation(passage_embeddings::kUrgent);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Adding a user blocking observer should raise the priority again.
  page_embeddings_service().AddObserver(&observer_user_blocking);

  set_priority_expectation(passage_embeddings::kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Removing the urgent observer should not affect the priority since a higher
  // priority observer is present.
  page_embeddings_service().RemoveObserver(&observer_urgent);

  set_priority_expectation(passage_embeddings::kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Removing the last observer should restore the priority to the default.
  page_embeddings_service().RemoveObserver(&observer_user_blocking);

  set_priority_expectation(passage_embeddings::kPassive);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());
}

// Validates that the embedder's jobs are reprioritized as expected.
TEST_F(PageEmbeddingsServiceTest, JobsReprioritized) {
  std::unique_ptr<content::WebContents> web_contents1 =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);
  std::unique_ptr<content::WebContents> web_contents2 =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer_urgent;
  EXPECT_CALL(observer_urgent, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUrgent));
  EXPECT_CALL(observer_urgent, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());

  page_embeddings_service().AddObserver(&observer_urgent);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  page_embeddings_service().OnPageContentExtracted(
      web_contents1->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault([this](auto, auto, auto) {
        return passage_embeddings::Embedder::Job(embedder_mock_.GetWeakPtr(),
                                                 2);
      });
  page_embeddings_service().OnPageContentExtracted(
      web_contents2->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ObserverMock observer_user_blocking;
  EXPECT_CALL(observer_user_blocking, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUserBlocking));
  EXPECT_CALL(observer_user_blocking, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  EXPECT_CALL(
      embedder_mock(),
      ReprioritizeJobs(passage_embeddings::kUserInitiated, ElementsAre(1, 2)));

  page_embeddings_service().AddObserver(&observer_user_blocking);

  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kExecutionFailure);

  EXPECT_CALL(embedder_mock(),
              ReprioritizeJobs(passage_embeddings::kUrgent, ElementsAre(2)));

  page_embeddings_service().RemoveObserver(&observer_user_blocking);

  EXPECT_CALL(embedder_mock(),
              ReprioritizeJobs(passage_embeddings::kPassive, ElementsAre(2)));

  page_embeddings_service().RemoveObserver(&observer_urgent);
}

// Validates that ScopedPriority raises and lowers the priority as expected.
TEST_F(PageEmbeddingsServiceTest, ScopedPriority) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUrgent));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), TryCancel).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), ReprioritizeJobs).Times(AnyNumber());

  const auto set_priority_expectation =
      [this](passage_embeddings::PassagePriority expected_priority) {
        ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
            .WillByDefault([this, expected_priority](
                               passage_embeddings::PassagePriority priority,
                               std::vector<std::string> passages,
                               passage_embeddings::Embedder::
                                   ComputePassagesEmbeddingsCallback callback) {
              EXPECT_EQ(expected_priority, priority);
              return passage_embeddings::Embedder::Job(
                  embedder_mock_.GetWeakPtr(), 1);
            });
      };

  // Adding the observer raises the priority to kUrgent.
  page_embeddings_service().AddObserver(&observer);

  // Establishing the ScopedPriority should further raise the priority.
  std::optional<PageEmbeddingsService::ScopedPriority> scoped_priority =
      page_embeddings_service().RaisePriority(
          &observer, PageEmbeddingsService::kUserBlocking);

  set_priority_expectation(passage_embeddings::kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Destroying the ScopedPriority should revert to the lower priority.
  scoped_priority.reset();
  set_priority_expectation(passage_embeddings::kUrgent);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that ScopedPriority doesn't affect the priority if a higher
// priority observer is present.
TEST_F(PageEmbeddingsServiceTest, ScopedPriorityWithHigherPriorityObserver) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer_default;
  EXPECT_CALL(observer_default, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer_default, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  ObserverMock observer_user_blocking;
  EXPECT_CALL(observer_user_blocking, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kUserBlocking));
  EXPECT_CALL(observer_user_blocking, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), TryCancel).Times(AnyNumber());
  EXPECT_CALL(embedder_mock(), ReprioritizeJobs).Times(AnyNumber());

  const auto set_priority_expectation =
      [this](passage_embeddings::PassagePriority expected_priority) {
        ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
            .WillByDefault([this, expected_priority](
                               passage_embeddings::PassagePriority priority,
                               std::vector<std::string> passages,
                               passage_embeddings::Embedder::
                                   ComputePassagesEmbeddingsCallback callback) {
              EXPECT_EQ(expected_priority, priority);
              return passage_embeddings::Embedder::Job(
                  embedder_mock_.GetWeakPtr(), 1);
            });
      };

  page_embeddings_service().AddObserver(&observer_default);
  page_embeddings_service().AddObserver(&observer_user_blocking);

  // Establishing the ScopedPriority should not affect the priority.
  std::optional<PageEmbeddingsService::ScopedPriority> scoped_priority =
      page_embeddings_service().RaisePriority(&observer_default,
                                              PageEmbeddingsService::kUrgent);

  set_priority_expectation(passage_embeddings::kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Destroying the ScopedPriority should not affect the priority.
  scoped_priority.reset();
  set_priority_expectation(passage_embeddings::kUserInitiated);
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  page_embeddings_service().RemoveObserver(&observer_user_blocking);
  page_embeddings_service().RemoveObserver(&observer_default);
}

// Validates that the active tab's embeddings are not computed while visible in
// on demand mode.
TEST_F(PageEmbeddingsServiceTest,
       EmbeddingsForActiveTabDeferredWhileVisibleInOnDemandMode) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(0);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(
                            testing::Ref(web_contents->GetPrimaryPage())))
      .Times(0);

  page_embeddings_service().AddObserver(&observer);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the active tab's embeddings are computed while visible in
// continuous mode.
TEST_F(PageEmbeddingsServiceTest,
       EmbeddingsForActiveTabComputedWhileVisibleInContinuousMode) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kContinuous));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(
                            testing::Ref(web_contents->GetPrimaryPage())))
      .Times(1);

  page_embeddings_service().AddObserver(&observer);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());
  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the active tab's embeddings are computed when switching from
// on demand mode to continuous.
TEST_F(PageEmbeddingsServiceTest,
       EmbeddingsForActiveTabComputedWhenSwitchingToContinuousMode) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock on_demand_observer;
  EXPECT_CALL(on_demand_observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(on_demand_observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  EXPECT_CALL(
      on_demand_observer,
      OnPageEmbeddingsAvailable(testing::Ref(web_contents->GetPrimaryPage())))
      .Times(1);

  page_embeddings_service().AddObserver(&on_demand_observer);

  ObserverMock continuous_observer;
  EXPECT_CALL(continuous_observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(continuous_observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kContinuous));
  EXPECT_CALL(
      continuous_observer,
      OnPageEmbeddingsAvailable(testing::Ref(web_contents->GetPrimaryPage())))
      .Times(1);

  page_embeddings_service().AddObserver(&continuous_observer);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());
  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&on_demand_observer);
  page_embeddings_service().RemoveObserver(&continuous_observer);
}

// Validates that the active tab's embeddings are computed on the transition
// from visible to hidden in on demand mode.
TEST_F(PageEmbeddingsServiceTest,
       EmbeddingsForActiveTabComputedOnHiddenInOnDemandMode) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(
                            testing::Ref(web_contents->GetPrimaryPage())))
      .Times(1);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  page_embeddings_service().AddObserver(&observer);

  web_contents->UpdateWebContentsVisibility(content::Visibility::HIDDEN);

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());
  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that the active tab's embeddings are computed on invoking
// ProcessEmbeddingsOnDemand() in on demand mode.
TEST_F(PageEmbeddingsServiceTest,
       EmbeddingsForActiveTabComputedOnProcessEmbeddingsOnDemand) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(
                            testing::Ref(web_contents->GetPrimaryPage())))
      .Times(1);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  page_embeddings_service().AddObserver(&observer);

  page_embeddings_service().ProcessEmbeddingsOnDemand();

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());
  std::move(compute_passages_embeddings_callback)
      .Run({"passage text"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

// Validates that adding a lower priority observer doesn't downgrade the usage
// mode.
TEST_F(PageEmbeddingsServiceTest, UsageModeDoesNotDowngrade) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  ObserverMock continuous_observer;
  EXPECT_CALL(continuous_observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(continuous_observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kContinuous));
  page_embeddings_service().AddObserver(&continuous_observer);

  ObserverMock on_demand_observer;
  EXPECT_CALL(on_demand_observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(on_demand_observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  page_embeddings_service().AddObserver(&on_demand_observer);

  // Since we are in continuous mode, embeddings should be computed immediately
  // for visible tabs.
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  page_embeddings_service().RemoveObserver(&continuous_observer);
  page_embeddings_service().RemoveObserver(&on_demand_observer);
}

// Validates that transitioning to continuous mode only triggers eager
// computation once.
TEST_F(PageEmbeddingsServiceTest, ContinuousModeEagerComputationOnlyRunsOnce) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::VISIBLE);

  // Extract content so there are pending passages.
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Adding the first continuous observer should trigger eager computation.
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(1);

  ObserverMock continuous_observer1;
  EXPECT_CALL(continuous_observer1, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(continuous_observer1, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kContinuous));
  page_embeddings_service().AddObserver(&continuous_observer1);

  // Adding a second continuous observer should NOT trigger eager computation
  // again.
  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings).Times(0);

  ObserverMock continuous_observer2;
  EXPECT_CALL(continuous_observer2, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(continuous_observer2, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kContinuous));
  page_embeddings_service().AddObserver(&continuous_observer2);

  page_embeddings_service().RemoveObserver(&continuous_observer1);
  page_embeddings_service().RemoveObserver(&continuous_observer2);
}

// Validates that data is reset for the WebContents upon navigation.
TEST_F(PageEmbeddingsServiceTest, NavigationResetsData) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  // Navigate to a new page.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("http://example.com"));

  EXPECT_TRUE(page_embeddings_service()
                  .GetEmbeddings(web_contents->GetPrimaryPage())
                  .empty());
}

// Validates that seeing a new page with no passages clears the embeddings
// from the previous page in the same WebContents.
TEST_F(PageEmbeddingsServiceTest, NewPageWithNoPassagesClearsOldEmbeddings) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  // Load Page 1 and compute embeddings.
  scoped_refptr<RefCountedAnnotatedPageContent> page_content1 =
      base::MakeRefCounted<RefCountedAnnotatedPageContent>();
  page_content1->data.mutable_main_frame_data()->set_title("page 1");
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(), page_content1);

  std::move(compute_passages_embeddings_callback)
      .Run({"page 1"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  ASSERT_FALSE(page_embeddings_service()
                   .GetEmbeddings(web_contents->GetPrimaryPage())
                   .empty());

  // Navigate to Page 2.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("http://example.com/2"));

  // Extract content for Page 2, but it has no passages.
  scoped_refptr<RefCountedAnnotatedPageContent> page_content2 =
      base::MakeRefCounted<RefCountedAnnotatedPageContent>();
  page_content2->data.mutable_main_frame_data()->set_title("EMPTY");
  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(), page_content2);

  // The embeddings from Page 1 should no longer be available for Page 2.
  EXPECT_TRUE(page_embeddings_service()
                  .GetEmbeddings(web_contents->GetPrimaryPage())
                  .empty());
}

// Validates that candidate passages are generated from PDF text.
TEST_F(PageEmbeddingsServiceTest, GeneratesCandidatePassagesFromPDFText) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ON_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillByDefault(
          [this](passage_embeddings::PassagePriority priority,
                 std::vector<std::string> passages,
                 passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                     callback) {
            EXPECT_THAT(passages, ElementsAre("pdf text content"));
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings);

  page_embeddings_service().OnPageContentExtracted(
      web_contents->GetPrimaryPage(),
      base::MakeRefCounted<RefCountedPDFText>("pdf text content"));
}

// Validates that embeddings computed for a page that is no longer the primary
// page (e.g. it was navigated away from but is still alive in BFCache) are
// ignored and do not notify observers.
TEST_F(PageEmbeddingsServiceTest, BFCacheRaceReproduction) {
  std::unique_ptr<content::WebContents> web_contents =
      CreateTestWebContentsWithVisibility(content::Visibility::HIDDEN);

  ObserverMock observer;
  EXPECT_CALL(observer, GetDefaultPriority)
      .WillRepeatedly(Return(PageEmbeddingsService::kDefault));
  EXPECT_CALL(observer, GetUsageMode)
      .WillRepeatedly(Return(PageEmbeddingsService::kOnDemand));
  page_embeddings_service().AddObserver(&observer);

  passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
      compute_passages_embeddings_callback;

  EXPECT_CALL(embedder_mock(), ComputePassagesEmbeddings)
      .WillOnce(
          [&](passage_embeddings::PassagePriority priority,
              std::vector<std::string> passages,
              passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                  callback) {
            compute_passages_embeddings_callback = std::move(callback);
            return passage_embeddings::Embedder::Job(
                embedder_mock_.GetWeakPtr(), 1);
          });

  // 1. Initial page load (attacker.com).
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://attacker.com"));
  content::Page& page1 = web_contents->GetPrimaryPage();
  base::WeakPtr<content::Page> page1_weak = page1.GetWeakPtr();

  // 2. Content extracted for page 1.
  page_embeddings_service().OnPageContentExtracted(
      page1, base::MakeRefCounted<RefCountedAnnotatedPageContent>());

  ASSERT_FALSE(compute_passages_embeddings_callback.is_null());

  // 3. Navigate to page 2 (victim.com).
  EXPECT_CALL(embedder_mock(), TryCancel(1));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL("https://victim.com"));
  ASSERT_NE(nullptr, page1_weak)
      << "Page 1 was destroyed upon navigation. BFCache simulation failed.";

  // 4. Complete embedding for page 1.
  // OnPageEmbeddingsAvailable should NOT be called because
  // PrimaryPageChanged cleared the state.
  EXPECT_CALL(observer, OnPageEmbeddingsAvailable(testing::_)).Times(0);

  std::move(compute_passages_embeddings_callback)
      .Run({"passage"}, {passage_embeddings::Embedding({1.0f})}, 1,
           passage_embeddings::ComputeEmbeddingsStatus::kSuccess);

  page_embeddings_service().RemoveObserver(&observer);
}

}  // namespace page_content_annotations
