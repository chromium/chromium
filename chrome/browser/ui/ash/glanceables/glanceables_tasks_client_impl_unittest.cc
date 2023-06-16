// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/time_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_model.h"
#include "ui/base/models/list_model_observer.h"

namespace ash {
namespace {

using ::base::test::RepeatingTestFuture;
using ::base::test::TestFuture;
using ::google_apis::ApiErrorCode;
using ::google_apis::util::FormatTimeAsString;
using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpMethod;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;

constexpr char kDefaultTaskListsResponseContent[] = R"(
    {
      "kind": "tasks#taskLists",
      "items": [
        {
          "id": "qwerty",
          "title": "My Tasks 1",
          "updated": "2023-01-30T22:19:22.812Z"
        },
        {
          "id": "asdfgh",
          "title": "My Tasks 2",
          "updated": "2022-12-21T23:38:22.590Z"
        }
      ]
    }
  )";

constexpr char kDefaultTasksResponseContent[] = R"(
    {
      "kind": "tasks#tasks",
      "items": [
        {
          "id": "asd",
          "title": "Parent task, level 1",
          "status": "needsAction",
          "due": "2023-04-19T00:00:00.000Z"
        },
        {
          "id": "qwe",
          "title": "Child task, level 2",
          "parent": "asd",
          "status": "needsAction"
        },
        {
          "id": "zxc",
          "title": "Parent task 2, level 1",
          "status": "needsAction",
          "links": [{"type": "email"}]
        }
      ]
    }
  )";

// Helper class to simplify mocking `net::EmbeddedTestServer` responses,
// especially useful for subsequent responses when testing pagination logic.
class TestRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse(
      const std::string& content) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(content);
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

// Observer for `ui::ListModel` changes.
class TestListModelObserver : public ui::ListModelObserver {
 public:
  MOCK_METHOD(void, ListItemsAdded, (size_t start, size_t count), (override));
  MOCK_METHOD(void, ListItemsRemoved, (size_t start, size_t count), (override));
  MOCK_METHOD(void,
              ListItemMoved,
              (size_t index, size_t target_index),
              (override));
  MOCK_METHOD(void, ListItemsChanged, (size_t start, size_t count), (override));
};

}  // namespace

class GlanceablesTasksClientImplTest : public testing::Test {
 public:
  void SetUp() override {
    auto create_request_sender_callback = base::BindLambdaForTesting(
        [&](const std::vector<std::string>& scopes,
            const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
          return std::make_unique<google_apis::RequestSender>(
              std::make_unique<google_apis::DummyAuthService>(),
              url_loader_factory_, task_environment_.GetMainThreadTaskRunner(),
              "test-user-agent", TRAFFIC_ANNOTATION_FOR_TESTS);
        });
    client_ = std::make_unique<GlanceablesTasksClientImpl>(
        create_request_sender_callback);

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TestRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));
    ASSERT_TRUE(test_server_.Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "tasks_api_origin_url",
        test_server_.base_url().spec());
    ASSERT_EQ(GaiaUrls::GetInstance()->tasks_api_origin_url(),
              test_server_.base_url().spec());
  }

  GlanceablesTasksClientImpl* client() { return client_.get(); }
  TestRequestHandler& request_handler() { return request_handler_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  testing::StrictMock<TestRequestHandler> request_handler_;
  std::unique_ptr<GlanceablesTasksClientImpl> client_;
};

// ----------------------------------------------------------------------------
// Get task lists:

TEST_F(GlanceablesTasksClientImplTest, GetTaskLists) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTaskListsResponseContent))));

  TestFuture<ui::ListModel<GlanceablesTaskList>*> future;
  client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const task_lists = future.Get();
  EXPECT_EQ(task_lists->item_count(), 2u);

  EXPECT_EQ(task_lists->GetItemAt(0)->id, "qwerty");
  EXPECT_EQ(task_lists->GetItemAt(0)->title, "My Tasks 1");
  EXPECT_EQ(FormatTimeAsString(task_lists->GetItemAt(0)->updated),
            "2023-01-30T22:19:22.812Z");

  EXPECT_EQ(task_lists->GetItemAt(1)->id, "asdfgh");
  EXPECT_EQ(task_lists->GetItemAt(1)->title, "My Tasks 2");
  EXPECT_EQ(FormatTimeAsString(task_lists->GetItemAt(1)->updated),
            "2022-12-21T23:38:22.590Z");
}

TEST_F(GlanceablesTasksClientImplTest, GetTaskListsOnSubsequentCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTaskListsResponseContent))));

  RepeatingTestFuture<ui::ListModel<GlanceablesTaskList>*> future;
  client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const task_lists = future.Take();

  // Subsequent request doesn't trigger another network call and returns a
  // pointer to the same `ui::ListModel`.
  client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), task_lists);
}

TEST_F(GlanceablesTasksClientImplTest,
       GetTaskListsReturnsEmptyVectorOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TestFuture<ui::ListModel<GlanceablesTaskList>*> future;
  client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const task_lists = future.Get();
  EXPECT_EQ(task_lists->item_count(), 0u);
}

TEST_F(GlanceablesTasksClientImplTest, GetTaskListsFetchesAllPages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-1"}],
            "nextPageToken": "qwe"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=qwe"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-2"}],
            "nextPageToken": "asd"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=asd"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#taskLists",
            "items": [{"id": "task-list-from-page-3"}]
          }
        )"))));

  TestFuture<ui::ListModel<GlanceablesTaskList>*> future;
  client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const task_lists = future.Get();
  EXPECT_EQ(task_lists->item_count(), 3u);
  EXPECT_EQ(task_lists->GetItemAt(0)->id, "task-list-from-page-1");
  EXPECT_EQ(task_lists->GetItemAt(1)->id, "task-list-from-page-2");
  EXPECT_EQ(task_lists->GetItemAt(2)->id, "task-list-from-page-3");
}

// ----------------------------------------------------------------------------
// Get tasks:

TEST_F(GlanceablesTasksClientImplTest, GetTasks) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  TestFuture<ui::ListModel<GlanceablesTask>*> future;
  client()->GetTasks("test-task-list-id", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const root_tasks = future.Get();
  ASSERT_EQ(root_tasks->item_count(), 2u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "asd");
  EXPECT_EQ(root_tasks->GetItemAt(0)->title, "Parent task, level 1");
  EXPECT_EQ(root_tasks->GetItemAt(0)->completed, false);
  EXPECT_EQ(FormatTimeAsString(root_tasks->GetItemAt(0)->due.value()),
            "2023-04-19T00:00:00.000Z");
  EXPECT_TRUE(root_tasks->GetItemAt(0)->has_subtasks);
  EXPECT_FALSE(root_tasks->GetItemAt(0)->has_email_link);

  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "zxc");
  EXPECT_EQ(root_tasks->GetItemAt(1)->title, "Parent task 2, level 1");
  EXPECT_EQ(root_tasks->GetItemAt(1)->completed, false);
  EXPECT_FALSE(root_tasks->GetItemAt(1)->due);
  EXPECT_FALSE(root_tasks->GetItemAt(1)->has_subtasks);
  EXPECT_TRUE(root_tasks->GetItemAt(1)->has_email_link);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasksOnSubsequentCalls) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kDefaultTasksResponseContent))));

  RepeatingTestFuture<ui::ListModel<GlanceablesTask>*> future;
  client()->GetTasks("test-task-list-id", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const root_tasks = future.Take();

  // Subsequent request doesn't trigger another network call and returns a
  // pointer to the same `ui::ListModel`.
  client()->GetTasks("test-task-list-id", future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), root_tasks);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasksReturnsEmptyVectorOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TestFuture<ui::ListModel<GlanceablesTask>*> future;
  client()->GetTasks("test-task-list-id", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const root_tasks = future.Get();
  EXPECT_EQ(root_tasks->item_count(), 0u);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasksFetchesAllPages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  Not(HasSubstr("pageToken")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "child-task-from-page-1",
                "parent": "parent-task-from-page-2"
              }
            ],
            "nextPageToken": "qwe"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=qwe"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "parent-task-from-page-2"}],
            "nextPageToken": "asd"
          }
        )"))));
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  HasSubstr("pageToken=asd"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [{"id": "parent-task-from-page-3"}]
          }
        )"))));

  TestFuture<ui::ListModel<GlanceablesTask>*> future;
  client()->GetTasks("test-task-list-id", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const root_tasks = future.Get();
  ASSERT_EQ(root_tasks->item_count(), 2u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->id, "parent-task-from-page-2");
  EXPECT_TRUE(root_tasks->GetItemAt(0)->has_subtasks);

  EXPECT_EQ(root_tasks->GetItemAt(1)->id, "parent-task-from-page-3");
  EXPECT_FALSE(root_tasks->GetItemAt(1)->has_subtasks);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasksSortsByPosition) {
  EXPECT_CALL(request_handler(), HandleRequest(_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {"title": "2nd", "position": "00000000000000000001"},
              {"title": "3rd", "position": "00000000000000000002"},
              {"title": "1st", "position": "00000000000000000000"}
            ]
          }
        )"))));

  TestFuture<ui::ListModel<GlanceablesTask>*> future;
  client()->GetTasks("test-task-list-id", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  const auto* const root_tasks = future.Get();
  ASSERT_EQ(root_tasks->item_count(), 3u);

  EXPECT_EQ(root_tasks->GetItemAt(0)->title, "1st");
  EXPECT_EQ(root_tasks->GetItemAt(1)->title, "2nd");
  EXPECT_EQ(root_tasks->GetItemAt(2)->title, "3rd");
}

// ----------------------------------------------------------------------------
// Mark as completed:

TEST_F(GlanceablesTasksClientImplTest, MarkAsCompleted) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "task-1",
                "status": "needsAction"
              },
              {
                "id": "task-2",
                "status": "needsAction"
              }
            ]
          }
        )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_PATCH))))
      .WillOnce(
          Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(""))));

  TestFuture<ui::ListModel<GlanceablesTask>*> get_tasks_future;
  client()->GetTasks("test-task-list-id", get_tasks_future.GetCallback());
  ASSERT_TRUE(get_tasks_future.Wait());

  auto* const tasks = get_tasks_future.Get();
  EXPECT_EQ(tasks->item_count(), 2u);

  testing::StrictMock<TestListModelObserver> observer;
  tasks->AddObserver(&observer);

  EXPECT_CALL(observer, ListItemsRemoved(/*start=*/1, /*count=*/1));
  TestFuture<bool> mark_as_completed_future;
  client()->MarkAsCompleted("test-task-list-id", "task-2",
                            mark_as_completed_future.GetCallback());
  ASSERT_TRUE(mark_as_completed_future.Wait());

  EXPECT_TRUE(mark_as_completed_future.Get());
  EXPECT_EQ(tasks->item_count(), 1u);
  EXPECT_EQ(tasks->GetItemAt(0)->id, "task-1");
}

TEST_F(GlanceablesTasksClientImplTest, MarkAsCompletedOnHttpError) {
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_GET))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "task-1",
                "status": "needsAction"
              },
              {
                "id": "task-2",
                "status": "needsAction"
              }
            ]
          }
        )"))));
  EXPECT_CALL(
      request_handler(),
      HandleRequest(Field(&HttpRequest::method, Eq(HttpMethod::METHOD_PATCH))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  TestFuture<ui::ListModel<GlanceablesTask>*> get_tasks_future;
  client()->GetTasks("test-task-list-id", get_tasks_future.GetCallback());
  ASSERT_TRUE(get_tasks_future.Wait());

  const auto* const tasks = get_tasks_future.Get();
  EXPECT_EQ(tasks->item_count(), 2u);

  TestFuture<bool> mark_as_completed_future;
  client()->MarkAsCompleted("test-task-list-id", "task-2",
                            mark_as_completed_future.GetCallback());
  ASSERT_TRUE(mark_as_completed_future.Wait());

  EXPECT_FALSE(mark_as_completed_future.Get());
  EXPECT_EQ(tasks->item_count(), 2u);
}

}  // namespace ash
