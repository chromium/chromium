// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/query_contextualizer.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_features.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class MockQueryContextualizerDelegate : public QueryContextualizer::Delegate {
 public:
  MOCK_METHOD(GURL, GetTabUrl, (QueryContextualizer::TabId id), (override));
  MOCK_METHOD(SessionID,
              GetTabSessionId,
              (QueryContextualizer::TabId id),
              (override));
  MOCK_METHOD(
      void,
      GetPageContext,
      (QueryContextualizer::TabId id,
       base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
           callback),
      (override));
  MOCK_METHOD(bool, IsTabValid, (QueryContextualizer::TabId id), (override));
  MOCK_METHOD(std::optional<lens::ImageEncodingOptions>,
              GetTabViewportEncodingOptionsForQueryContextualizer,
              (),
              (override));

  MOCK_METHOD(contextual_search::ContextualSearchSessionHandle*,
              GetOrCreateSessionHandleForQueryContextualizer,
              (),
              (override));

  MOCK_METHOD(void,
              GetRelevantTabsForQuery,
              (const std::string& query_text,
               const std::vector<GURL>& attached_context_urls,
               base::OnceCallback<void(std::vector<QueryContextualizer::TabId>)>
                   callback),
              (override));
};

class QueryContextualizerTest : public testing::Test {
 public:
  QueryContextualizerTest() {
    feature_list_.InitAndEnableFeature(
        lens::features::kLensSendUrlsInComposeboxes);
  }

  void SetUp() override {
    service_ =
        std::make_unique<testing::NiceMock<MockContextualTasksService>>();
    delegate_ =
        std::make_unique<testing::NiceMock<MockQueryContextualizerDelegate>>();
    contextualizer_ =
        std::make_unique<QueryContextualizer>(service_.get(), delegate_.get());
    session_handle_ = std::make_unique<testing::NiceMock<
        contextual_search::MockContextualSearchSessionHandle>>();
    context_controller_ = std::make_unique<testing::NiceMock<
        contextual_search::MockContextualSearchContextController>>();

    ON_CALL(*session_handle_, GetController())
        .WillByDefault(testing::Return(context_controller_.get()));
    ON_CALL(*context_controller_, AddObserver(testing::_))
        .WillByDefault(
            [this](contextual_search::ContextualSearchContextController::
                       ContextUploadStatusObserver* obs) {
              captured_observer_ = obs;
            });
    ON_CALL(*context_controller_, RemoveObserver(testing::_))
        .WillByDefault(
            [this](contextual_search::ContextualSearchContextController::
                       ContextUploadStatusObserver* obs) {
              if (captured_observer_ == obs) {
                captured_observer_ = nullptr;
              }
            });

    mock_context_controller_weak_factory_ =
        std::make_unique<base::WeakPtrFactory<
            contextual_search::ContextualSearchContextController>>(
            context_controller_.get());

    ON_CALL(*context_controller_, AsWeakPtr()).WillByDefault([this]() {
      return mock_context_controller_weak_factory_->GetWeakPtr();
    });

    ON_CALL(*session_handle_, CreateContextToken()).WillByDefault([this]() {
      auto token = base::UnguessableToken::Create();
      created_tokens_.push_back(token);
      return token;
    });

    ON_CALL(*delegate_, GetOrCreateSessionHandleForQueryContextualizer())
        .WillByDefault(testing::Return(session_handle_.get()));
  }

  void CompleteAllUploads() {
    if (captured_observer_) {
      for (const auto& token : created_tokens_) {
        captured_observer_->OnContextUploadStatusChanged(
            token, lens::MimeType::kUnknown,
            contextual_search::ContextUploadStatus::kUploadSuccessful,
            std::nullopt);
      }
      created_tokens_.clear();
    }
  }

  void TearDown() override {
    contextualizer_.reset();
    delegate_.reset();
    service_.reset();
    session_handle_.reset();
    context_controller_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<testing::NiceMock<MockContextualTasksService>> service_;
  std::unique_ptr<testing::NiceMock<MockQueryContextualizerDelegate>> delegate_;
  std::unique_ptr<QueryContextualizer> contextualizer_;
  std::unique_ptr<
      testing::NiceMock<contextual_search::MockContextualSearchSessionHandle>>
      session_handle_;
  std::unique_ptr<testing::NiceMock<
      contextual_search::MockContextualSearchContextController>>
      context_controller_;
  raw_ptr<contextual_search::ContextualSearchContextController::
              ContextUploadStatusObserver>
      captured_observer_ = nullptr;
  std::vector<base::UnguessableToken> created_tokens_;
  std::unique_ptr<base::WeakPtrFactory<
      contextual_search::ContextualSearchContextController>>
      mock_context_controller_weak_factory_;
};

TEST_F(QueryContextualizerTest, Contextualize_SmartTabSharingEnabled) {
  std::string query = "query";

  EXPECT_CALL(*delegate_,
              GetRelevantTabsForQuery(query, testing::_, testing::_))
      .WillOnce([](const auto& query, const auto& urls, auto callback) {
        std::move(callback).Run({1, 2});
      });

  EXPECT_CALL(*delegate_, IsTabValid(1)).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate_, IsTabValid(2)).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate_, GetTabUrl(1))
      .WillRepeatedly(testing::Return(GURL("http://tab1.com")));
  EXPECT_CALL(*delegate_, GetTabUrl(2))
      .WillRepeatedly(testing::Return(GURL("http://tab2.com")));

  EXPECT_CALL(*delegate_, GetPageContext(1, testing::_))
      .WillOnce([](auto id, auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });
  EXPECT_CALL(*delegate_, GetPageContext(2, testing::_))
      .WillOnce([](auto id, auto callback) {
        std::move(callback).Run(std::make_unique<lens::ContextualInputData>());
      });

  base::MockCallback<QueryContextualizer::ContextualizedCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(1);

  contextualizer_->Contextualize(std::nullopt, query, {}, {}, base::DoNothing(),
                                 base::DoNothing(), callback.Get(),
                                 /*enable_smart_tab_selection=*/true);

  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest, Contextualize_WaitsForUploadsToFinish) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with a query containing a URL.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  EXPECT_CALL(*context_controller_, AddObserver(testing::_));
  EXPECT_CALL(*context_controller_, RemoveObserver(testing::_));
  EXPECT_CALL(*session_handle_, CreateContextToken()).Times(2);

  EXPECT_CALL(*session_handle_,
              StartUrlContextUploadFlow(testing::_, "https://example.com"));

  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "new content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_));

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  // Mock GetFileInfo to return non-terminal status initially.
  contextual_search::FileInfo mock_file_info;
  mock_file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadStarted;
  EXPECT_CALL(*context_controller_, GetFileInfo(testing::_))
      .WillRepeatedly(testing::Return(&mock_file_info));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;

  // The done_callback should NOT be called synchronously, as we are waiting for
  // uploads.
  EXPECT_CALL(done_callback, Run(testing::_)).Times(0);

  contextualizer_->Contextualize(task_id, "Check out https://example.com",
                                 {tab_id}, {}, ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);

  ASSERT_NE(captured_observer_, nullptr);
  ASSERT_EQ(created_tokens_.size(), 2u);

  // Now simulate the URL upload finishing.
  // We still expect done_callback to not be called, because the tab is
  // uploading.
  captured_observer_->OnContextUploadStatusChanged(
      created_tokens_[0], lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  // Now simulate the tab upload finishing.
  // This time the callback should be invoked.
  EXPECT_CALL(done_callback, Run(testing::_)).Times(1);
  captured_observer_->OnContextUploadStatusChanged(
      created_tokens_[1], lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
}

TEST_F(QueryContextualizerTest, Contextualize_ExtractsUrls) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with a query containing URLs.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect StartUrlContextUploadFlow to be called for each unique URL in order.
  {
    testing::InSequence s;
    EXPECT_CALL(*session_handle_,
                StartUrlContextUploadFlow(testing::_, "https://example.com"));
    EXPECT_CALL(*session_handle_,
                StartUrlContextUploadFlow(testing::_, "http://test.org"));
    EXPECT_CALL(*session_handle_,
                StartUrlContextUploadFlow(testing::_, "http://www.google.com"));
  }

  // Expect GetPageContext call to NOT be called since the tab is not expired
  // and content hasn't changed. We mock GetFileInfoList to return the tab
  // so it's not updated.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info_list.push_back(&file_info);
  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call to NOT be called.
  EXPECT_CALL(*delegate_, GetPageContext(testing::_, testing::_)).Times(0);

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id,
                                 "Check out https://example.com! Also "
                                 "http://test.org, and www.google.com. "
                                 "Duplicate: https://example.com",
                                 {}, {}, base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest, Contextualize_WithNullService) {
  auto null_service_contextualizer = std::make_unique<QueryContextualizer>(
      /*service=*/nullptr, delegate_.get());

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_)).Times(1);

  null_service_contextualizer->Contextualize(
      /*task_id=*/std::nullopt, "test query", {}, {}, base::DoNothing(),
      base::DoNothing(), done_callback.Get(),
      /*enable_smart_tab_selection=*/false);
}

TEST(QueryContextualizerStaticTest, ExtractUrlsFromQuery) {
  // Test simple extraction.
  std::vector<std::string> urls = QueryContextualizer::ExtractUrlsFromQuery(
      "Check out https://example.com");
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "https://example.com");

  // Test extraction with ampersand.
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "Check out https://example.com?a=1&b=2");
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "https://example.com?a=1&b=2");

  // Test extraction with multiple URLs.
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "https://example.com and http://test.org");
  ASSERT_EQ(urls.size(), 2u);
  EXPECT_EQ(urls[0], "https://example.com");
  EXPECT_EQ(urls[1], "http://test.org");

  // Test extraction with www. prefix.
  urls = QueryContextualizer::ExtractUrlsFromQuery("www.google.com");
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "http://www.google.com");

  // Test deduplication.
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "https://example.com and https://example.com");
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "https://example.com");

  // Test trailing punctuation trimming.
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "Check out https://wikipedia.org, and www.example.com. Also see "
      "http://test.com/path; is http://google.com? ok? What about "
      "https://test.org!");
  ASSERT_EQ(urls.size(), 5u);
  EXPECT_EQ(urls[0], "https://wikipedia.org");
  EXPECT_EQ(urls[1], "http://www.example.com");
  EXPECT_EQ(urls[2], "http://test.com/path;");
  EXPECT_EQ(urls[3], "http://google.com?");
  EXPECT_EQ(urls[4], "https://test.org");

  // Test trailing punctuation trimming with multiple trailing characters.
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "Check out https://wikipedia.org.,;?!");
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "https://wikipedia.org");

  // Test that query parameters with non-trailing ? are preserved.
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "Check out https://wikipedia.org?query=1");
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], "https://wikipedia.org?query=1");

  // Test trailing punctuation preserving (valid URLs)
  urls = QueryContextualizer::ExtractUrlsFromQuery(
      "Check out https://example.com/path/ and "
      "https://example.com/search?q=test! and "
      "https://example.com/page#section.");
  ASSERT_EQ(urls.size(), 3u);
  EXPECT_EQ(urls[0], "https://example.com/path/");
  EXPECT_EQ(urls[1], "https://example.com/search?q=test!");
  EXPECT_EQ(urls[2], "https://example.com/page#section.");
}

TEST_F(QueryContextualizerTest,
       Contextualize_DoesNotExtractUrlsWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      lens::features::kLensSendUrlsInComposeboxes);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with a query containing URLs.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect StartUrlContextUploadFlow to NOT be called.
  EXPECT_CALL(*session_handle_,
              StartUrlContextUploadFlow(testing::_, testing::_))
      .Times(0);

  // Expect GetPageContext call to NOT be called since the tab is not expired
  // and content hasn't changed. We mock GetFileInfoList to return the tab
  // so it's not updated.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info_list.push_back(&file_info);
  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call to NOT be called.
  EXPECT_CALL(*delegate_, GetPageContext(testing::_, testing::_)).Times(0);

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id,
                                 "Check out https://example.com! Also "
                                 "http://test.org, and www.google.com. "
                                 "Duplicate: https://example.com",
                                 {}, {}, base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest, Contextualize_RecontextualizeExpiredTab) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with expired tab.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with expired status.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadExpired;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);
  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(data->context_id, 12345);
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest, Contextualize_RecontextualizeContentChanged) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab (not expired).
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup context with uploaded status and some previous content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with NEW content.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "new content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call because content changed.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(data->context_id, 12345);
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_NoRecontextualizationIfUnchanged) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and SAME content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  std::string old_content = "same content";
  auto old_content_span = base::as_bytes(base::span(old_content));
  std::vector<uint8_t> old_bytes(old_content_span.begin(),
                                 old_content_span.end());
  lens::ContextualInput old_input(std::move(old_bytes),
                                  lens::MimeType::kPlainText);
  input_data->context_input.emplace().push_back(std::move(old_input));
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with SAME content.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "same content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect StartTabContextUploadFlow call to NOT be called.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_NoRecontextualizationIfApcComparisonDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kContextualTasksWebpageApcComparison);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and some content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  std::string old_content = "old content";
  auto old_content_span = base::as_bytes(base::span(old_content));
  std::vector<uint8_t> old_bytes(old_content_span.begin(),
                                 old_content_span.end());
  lens::ContextualInput old_input(std::move(old_bytes),
                                  lens::MimeType::kPlainText);
  input_data->context_input.emplace().push_back(std::move(old_input));
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with NEW/CHANGED content.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "new content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect StartTabContextUploadFlow call to NOT be called because page content
  // bytes (APC) comparison is disabled.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_WebpageRecontextualizedWithApcIfViewportAndApcChanged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kContextualTasksWebpageApcComparison);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status, some content, and old screenshot.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kPlainText;
  std::string old_content = "old content";
  auto old_content_span = base::as_bytes(base::span(old_content));
  std::vector<uint8_t> old_bytes(old_content_span.begin(),
                                 old_content_span.end());
  lens::ContextualInput old_input(std::move(old_bytes),
                                  lens::MimeType::kPlainText);
  input_data->context_input.emplace().push_back(std::move(old_input));
  input_data->viewport_screenshot_bytes = std::vector<uint8_t>{1, 2, 3};
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with NEW content and NEW screenshot.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->primary_content_type = lens::MimeType::kPlainText;
        std::string new_content = "longer new content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->viewport_screenshot_bytes = std::vector<uint8_t>{4, 5, 6};
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call to return true so the flow can proceed.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow to be called because viewport changed.
  // And context_input (APC) should BE INCLUDED because page content also
  // changed.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        ASSERT_TRUE(data->context_input.has_value());
        EXPECT_EQ(data->context_input->size(), 1u);
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(
    QueryContextualizerTest,
    Contextualize_WebpageRecontextualizedWithoutApcIfViewportChangedButApcUnchanged) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kContextualTasksWebpageApcComparison);

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status, some content, and old screenshot.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->primary_content_type = lens::MimeType::kPlainText;
  std::string old_content = "same content";
  auto old_content_span = base::as_bytes(base::span(old_content));
  std::vector<uint8_t> old_bytes(old_content_span.begin(),
                                 old_content_span.end());
  lens::ContextualInput old_input(std::move(old_bytes),
                                  lens::MimeType::kPlainText);
  input_data->context_input.emplace().push_back(std::move(old_input));
  input_data->viewport_screenshot_bytes = std::vector<uint8_t>{1, 2, 3};
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with SAME content and NEW screenshot.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->primary_content_type = lens::MimeType::kPlainText;
        std::string new_content = "same content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->viewport_screenshot_bytes = std::vector<uint8_t>{4, 5, 6};
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call to return true so the flow can proceed.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow to be called because viewport changed.
  // But context_input (APC) should BE OMITTED (std::nullopt) because page
  // content did not change.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_FALSE(data->context_input.has_value());
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest, Contextualize_ActiveTabNotInContext) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");

  // Setup context WITHOUT the active tab.
  ContextualTask task(task_id);
  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call to NOT be called.
  EXPECT_CALL(*delegate_, GetPageContext(testing::_, testing::_)).Times(0);

  // Expect StartTabContextUploadFlow call to NOT be called.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest, Contextualize_ActiveTabUrlMismatch) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  GURL kDifferentUrl("https://example.com");

  // Setup context with the active tab but a DIFFERENT URL.
  ContextualTask task(task_id);
  UrlResource resource(kDifferentUrl, ResourceType::kWebpage);
  resource.title = "example";
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call to NOT be called.
  EXPECT_CALL(*delegate_, GetPageContext(testing::_, testing::_)).Times(0);

  // Expect StartTabContextUploadFlow call to NOT be called.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_RecontextualizeScreenshotChanged_SkBitmap) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and OLD bitmap.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  SkBitmap old_bitmap;
  old_bitmap.allocN32Pixels(10, 10);
  old_bitmap.eraseColor(SK_ColorRED);
  input_data->viewport_screenshot = old_bitmap;
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with NEW bitmap (different color).
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        SkBitmap new_bitmap;
        new_bitmap.allocN32Pixels(10, 10);
        new_bitmap.eraseColor(SK_ColorBLUE);
        data->viewport_screenshot = new_bitmap;
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call because bitmap changed.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(data->context_id, 12345);
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_RecontextualizePdfViewportChanged) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and old content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  std::string old_content = "old pdf content";
  auto old_content_span = base::as_bytes(base::span(old_content));
  std::vector<uint8_t> old_bytes(old_content_span.begin(),
                                 old_content_span.end());
  lens::ContextualInput old_input(std::move(old_bytes), lens::MimeType::kPdf);
  input_data->context_input.emplace().push_back(std::move(old_input));
  input_data->primary_content_type = lens::MimeType::kPdf;
  input_data->viewport_screenshot_bytes = std::vector<uint8_t>{1, 2, 3};
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with NEW content and NEW viewport.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "new pdf content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPdf);
        data->context_input.emplace().push_back(std::move(new_input));
        data->primary_content_type = lens::MimeType::kPdf;
        data->viewport_screenshot_bytes = std::vector<uint8_t>{4, 5, 6};
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call to be called, but with empty
  // context_input.
  EXPECT_CALL(
      *session_handle_,
      StartTabContextUploadFlow(testing::_,
                                testing::Pointee(testing::Field(
                                    &lens::ContextualInputData::context_input,
                                    testing::Eq(std::nullopt))),
                                testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(data->context_id, 12345);
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_RecontextualizeWebpageViewportChanged) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and old content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  std::string old_content = "same content";
  auto old_content_span = base::as_bytes(base::span(old_content));
  std::vector<uint8_t> old_bytes(old_content_span.begin(),
                                 old_content_span.end());
  lens::ContextualInput old_input(std::move(old_bytes),
                                  lens::MimeType::kPlainText);
  input_data->context_input.emplace().push_back(std::move(old_input));
  input_data->primary_content_type = lens::MimeType::kPlainText;
  input_data->viewport_screenshot_bytes = std::vector<uint8_t>{1, 2, 3};
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with SAME content and NEW viewport.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "same content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->primary_content_type = lens::MimeType::kPlainText;
        data->viewport_screenshot_bytes = std::vector<uint8_t>{4, 5, 6};
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call to be called, but with empty
  // context_input.
  EXPECT_CALL(
      *session_handle_,
      StartTabContextUploadFlow(testing::_,
                                testing::Pointee(testing::Field(
                                    &lens::ContextualInputData::context_input,
                                    testing::Eq(std::nullopt))),
                                testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_EQ(data->context_id, 12345);
      });

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}
TEST_F(QueryContextualizerTest,
       Contextualize_RecontextualizeIgnoresSupersededFileInfo) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with uploaded tab.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo list. First is superseded, second is active.
  std::vector<const contextual_search::FileInfo*> file_info_list;

  contextual_search::FileInfo file_info_superseded;
  file_info_superseded.tab_session_id = session_id;
  file_info_superseded.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info_superseded.is_superceded = true;
  file_info_superseded.request_id.emplace();
  file_info_superseded.request_id->set_context_id(12344);

  auto input_data_superseded = std::make_unique<lens::ContextualInputData>();
  std::string old_content_superseded = "old content";
  auto old_content_span_superseded =
      base::as_bytes(base::span(old_content_superseded));
  std::vector<uint8_t> old_bytes_superseded(old_content_span_superseded.begin(),
                                            old_content_span_superseded.end());
  lens::ContextualInput old_input_superseded(std::move(old_bytes_superseded),
                                             lens::MimeType::kPlainText);
  input_data_superseded->context_input.emplace().push_back(
      std::move(old_input_superseded));
  file_info_superseded.input_data = std::move(input_data_superseded);

  contextual_search::FileInfo file_info_active;
  file_info_active.tab_session_id = session_id;
  file_info_active.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  file_info_active.is_superceded = false;
  file_info_active.request_id.emplace();
  file_info_active.request_id->set_context_id(12345);

  auto input_data_active = std::make_unique<lens::ContextualInputData>();
  std::string old_content_active = "same content";
  auto old_content_span_active = base::as_bytes(base::span(old_content_active));
  std::vector<uint8_t> old_bytes_active(old_content_span_active.begin(),
                                        old_content_span_active.end());
  lens::ContextualInput old_input_active(std::move(old_bytes_active),
                                         lens::MimeType::kPlainText);
  input_data_active->context_input.emplace().push_back(
      std::move(old_input_active));
  file_info_active.input_data = std::move(input_data_active);

  file_info_list.push_back(&file_info_superseded);
  file_info_list.push_back(&file_info_active);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call with SAME content as active file info.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "same content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Since it ignores the superseded file (which had "old content") and checks
  // against the active one ("same content"), it should not upload.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::MockCallback<QueryContextualizer::PageContextIneligibleCallback>
      ineligible_callback;
  base::MockCallback<QueryContextualizer::TabProcessedCallback>
      processed_callback;

  EXPECT_CALL(processed_callback, Run(tab_id));

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 ineligible_callback.Get(),
                                 processed_callback.Get(), done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
}

TEST_F(QueryContextualizerTest,
       Contextualize_AutoSuggestedTabIsImplicitUpload) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");

  ContextualTask task(task_id);
  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call and check is_implicit_upload.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_TRUE(data->is_implicit_upload);
      });

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  // Pass tab_id in tabs_to_force_contextualize (the 4th parameter).
  contextualizer_->Contextualize(task_id, "test query", {}, {tab_id},
                                 base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_AutoSuggestedTabSetsAutoTabChipUploadType) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");

  ContextualTask task(task_id);
  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call and check upload_type.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        ASSERT_TRUE(data->upload_type.has_value());
        EXPECT_EQ(*data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_AUTO_TAB_CHIP);
      });

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  // Pass tab_id in tabs_to_force_contextualize.
  contextualizer_->Contextualize(task_id, "test query", {}, {tab_id},
                                 base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_RecontextualizedTabSetsRecontextualizationUploadType) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  int32_t tab_id = 100;
  SessionID session_id = SessionID::FromSerializedValue(1);
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  // Setup context with expired tab.
  ContextualTask task(task_id);
  UrlResource resource(kUrl, ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context = std::make_unique<ContextualTaskContext>(task);

  EXPECT_CALL(*service_,
              GetContextForTask(
                  task_id,
                  testing::Contains(
                      ContextualTaskContextSource::kSubmittedContextDecorator),
                  testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<ContextualTaskContextSource>& sources,
              std::unique_ptr<ContextDecorationParams> params,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with expired status so it recontextualizes.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadExpired;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);
  file_info_list.push_back(&file_info);

  EXPECT_CALL(*context_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(kUrl));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  // Expect GetPageContext call.
  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](
                    QueryContextualizer::TabId id,
                    base::OnceCallback<void(
                        std::unique_ptr<lens::ContextualInputData>)> callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect IsTabValid call.
  EXPECT_CALL(*delegate_, IsTabValid(tab_id)).WillOnce(testing::Return(true));

  // Expect StartTabContextUploadFlow call and check upload_type.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        ASSERT_TRUE(data->upload_type.has_value());
        EXPECT_EQ(*data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_RECONTEXTUALIZATION);
      });

  base::MockCallback<QueryContextualizer::ContextualizedCallback> done_callback;
  EXPECT_CALL(done_callback, Run(testing::_));

  // Pass tab_id in tabs_to_recontextualize.
  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 base::DoNothing(), base::DoNothing(),
                                 done_callback.Get(),
                                 /*enable_smart_tab_selection=*/false);
  CompleteAllUploads();
}

TEST_F(QueryContextualizerTest,
       Contextualize_SmartTabSetsSmartTabSelectionUploadType) {
  std::string query = "query";
  int32_t tab_id = 1;
  SessionID session_id = SessionID::FromSerializedValue(1);

  EXPECT_CALL(*delegate_,
              GetRelevantTabsForQuery(query, testing::_, testing::_))
      .WillOnce([tab_id](const auto& query, const auto& urls, auto callback) {
        std::move(callback).Run({tab_id});
      });

  EXPECT_CALL(*delegate_, IsTabValid(tab_id))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*delegate_, GetTabUrl(tab_id))
      .WillRepeatedly(testing::Return(GURL("http://tab1.com")));
  EXPECT_CALL(*delegate_, GetTabSessionId(tab_id))
      .WillRepeatedly(testing::Return(session_id));

  EXPECT_CALL(*delegate_, GetPageContext(tab_id, testing::_))
      .WillOnce([session_id](auto id, auto callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->tab_session_id = session_id;
        data->page_url = GURL("http://tab1.com");
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect StartTabContextUploadFlow call and check upload_type.
  EXPECT_CALL(*session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([](const base::UnguessableToken& file_token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> image_options) {
        ASSERT_TRUE(data->upload_type.has_value());
        EXPECT_EQ(*data->upload_type,
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_SMART_TAB_SELECTION);
      });

  base::MockCallback<QueryContextualizer::ContextualizedCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(1);

  contextualizer_->Contextualize(std::nullopt, query, {}, {}, base::DoNothing(),
                                 base::DoNothing(), callback.Get(),
                                 /*enable_smart_tab_selection=*/true);

  CompleteAllUploads();
}

}  // namespace contextual_tasks
