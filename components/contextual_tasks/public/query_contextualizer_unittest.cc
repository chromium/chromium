// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/query_contextualizer.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
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
  MOCK_METHOD(void,
              UploadTabContextWithData,
              (QueryContextualizer::TabId id,
               std::optional<int64_t> context_id,
               std::unique_ptr<lens::ContextualInputData> data,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void, OnPageContextIneligible, (), (override));
  MOCK_METHOD(void,
              OnTabProcessedForQueryContextualization,
              (QueryContextualizer::TabId id),
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
};

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
    EXPECT_CALL(
        *session_handle_,
        StartUrlContextUploadFlow(testing::_, GURL("https://example.com!")));
    EXPECT_CALL(*session_handle_, StartUrlContextUploadFlow(
                                      testing::_, GURL("http://test.org,")));
    EXPECT_CALL(
        *session_handle_,
        StartUrlContextUploadFlow(testing::_, GURL("http://www.google.com.")));
    EXPECT_CALL(*session_handle_, StartUrlContextUploadFlow(
                                      testing::_, GURL("https://example.com")));
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

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id,
                                 "Check out https://example.com! Also "
                                 "http://test.org, and www.google.com. "
                                 "Duplicate: https://example.com",
                                 {}, {}, session_handle_.get(),
                                 done_callback.Get());
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

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id,
                                 "Check out https://example.com! Also "
                                 "http://test.org, and www.google.com. "
                                 "Duplicate: https://example.com",
                                 {}, {}, session_handle_.get(),
                                 done_callback.Get());
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

  // Expect UploadTabContextWithData call.
  EXPECT_CALL(*delegate_,
              UploadTabContextWithData(tab_id, std::optional<int64_t>(12345),
                                       testing::_, testing::_))
      .WillOnce([](QueryContextualizer::TabId id,
                   std::optional<int64_t> context_id,
                   std::unique_ptr<lens::ContextualInputData> data,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call because content changed.
  EXPECT_CALL(*delegate_,
              UploadTabContextWithData(tab_id, std::optional<int64_t>(12345),
                                       testing::_, testing::_))
      .WillOnce([](QueryContextualizer::TabId id,
                   std::optional<int64_t> context_id,
                   std::unique_ptr<lens::ContextualInputData> data,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call to NOT be called.
  EXPECT_CALL(*delegate_, UploadTabContextWithData(testing::_, testing::_,
                                                   testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call to NOT be called.
  EXPECT_CALL(*delegate_, UploadTabContextWithData(testing::_, testing::_,
                                                   testing::_, testing::_))
      .Times(0);

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call to NOT be called.
  EXPECT_CALL(*delegate_, UploadTabContextWithData(testing::_, testing::_,
                                                   testing::_, testing::_))
      .Times(0);

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call because bitmap changed.
  EXPECT_CALL(*delegate_,
              UploadTabContextWithData(tab_id, std::optional<int64_t>(12345),
                                       testing::_, testing::_))
      .WillOnce([](QueryContextualizer::TabId id,
                   std::optional<int64_t> context_id,
                   std::unique_ptr<lens::ContextualInputData> data,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call to be called, but with empty
  // context_input.
  EXPECT_CALL(*delegate_, UploadTabContextWithData(
                              tab_id, testing::Optional(12345),
                              testing::Pointee(testing::Field(
                                  &lens::ContextualInputData::context_input,
                                  testing::Eq(std::nullopt))),
                              testing::_))
      .WillOnce([](QueryContextualizer::TabId id,
                   std::optional<int64_t> context_id,
                   std::unique_ptr<lens::ContextualInputData> data,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  // Expect UploadTabContextWithData call to be called, but with empty
  // context_input.
  EXPECT_CALL(*delegate_, UploadTabContextWithData(
                              tab_id, testing::Optional(12345),
                              testing::Pointee(testing::Field(
                                  &lens::ContextualInputData::context_input,
                                  testing::Eq(std::nullopt))),
                              testing::_))
      .WillOnce([](QueryContextualizer::TabId id,
                   std::optional<int64_t> context_id,
                   std::unique_ptr<lens::ContextualInputData> data,
                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
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

  EXPECT_CALL(*delegate_, OnTabProcessedForQueryContextualization(tab_id));

  base::MockCallback<base::OnceClosure> done_callback;
  EXPECT_CALL(done_callback, Run());

  contextualizer_->Contextualize(task_id, "test query", {tab_id}, {},
                                 session_handle_.get(), done_callback.Get());
}

}  // namespace contextual_tasks
