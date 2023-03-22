// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/time_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::base::test::TestFuture;
using ::google_apis::ApiErrorCode;
using ::google_apis::util::FormatTimeAsString;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

// Helper class to temporary override `GaiaUrls` singleton.
class GaiaUrlsOverrider {
 public:
  GaiaUrlsOverrider() { GaiaUrls::SetInstanceForTesting(&test_gaia_urls_); }
  ~GaiaUrlsOverrider() { GaiaUrls::SetInstanceForTesting(nullptr); }

 private:
  GaiaUrls test_gaia_urls_;
};

std::unique_ptr<net::test_server::HttpResponse> CreateSuccessfulResponse(
    const std::string& content) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(content);
  response->set_content_type("application/json");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> CreateFailedResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
  return response;
}

}  // namespace

class GlanceablesTasksClientImplTest : public testing::Test {
 public:
  using GenerateResponseCallback =
      base::RepeatingCallback<std::unique_ptr<HttpResponse>(
          const HttpRequest& request)>;

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

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &GlanceablesTasksClientImplTest::HandleDataFileRequest,
        base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kGoogleApisUrl, test_server_.base_url().spec());
    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverrider>();
    ASSERT_EQ(GaiaUrls::GetInstance()->google_apis_origin_url(),
              test_server_.base_url().spec());
  }

  void set_generate_response_callback(const GenerateResponseCallback& cb) {
    generate_response_callback_ = cb;
  }

  GlanceablesTasksClientImpl* client() { return client_.get(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleDataFileRequest(
      const net::test_server::HttpRequest& request) {
    return std::move(generate_response_callback_).Run(request);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::test::ScopedCommandLine command_line_;
  net::EmbeddedTestServer test_server_;
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  std::unique_ptr<GaiaUrlsOverrider> gaia_urls_overrider_;
  GenerateResponseCallback generate_response_callback_;
  std::unique_ptr<GlanceablesTasksClientImpl> client_;
};

TEST_F(GlanceablesTasksClientImplTest, GetTaskLists) {
  set_generate_response_callback(
      base::BindLambdaForTesting([](const HttpRequest& request) {
        return CreateSuccessfulResponse(R"(
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
        )");
      }));

  TestFuture<const std::vector<GlanceablesTaskList>&> future;
  auto cancel_closure = client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());

  const auto& task_lists = future.Get();
  EXPECT_EQ(task_lists.size(), 2u);

  EXPECT_EQ(task_lists.at(0).id, "qwerty");
  EXPECT_EQ(task_lists.at(0).title, "My Tasks 1");
  EXPECT_EQ(FormatTimeAsString(task_lists.at(0).updated),
            "2023-01-30T22:19:22.812Z");

  EXPECT_EQ(task_lists.at(1).id, "asdfgh");
  EXPECT_EQ(task_lists.at(1).title, "My Tasks 2");
  EXPECT_EQ(FormatTimeAsString(task_lists.at(1).updated),
            "2022-12-21T23:38:22.590Z");
}

TEST_F(GlanceablesTasksClientImplTest,
       GetTaskListsReturnsEmptyVectorOnHttpError) {
  set_generate_response_callback(base::BindLambdaForTesting(
      [](const HttpRequest& request) { return CreateFailedResponse(); }));

  TestFuture<const std::vector<GlanceablesTaskList>&> future;
  auto cancel_closure = client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());

  const auto& task_lists = future.Get();
  EXPECT_EQ(task_lists.size(), 0u);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasks) {
  set_generate_response_callback(
      base::BindLambdaForTesting([](const HttpRequest& request) {
        return CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "asd",
                "title": "Parent task, level 1",
                "status": "needsAction"
              },
              {
                "id": "qwe",
                "title": "Child task, level 2",
                "parent": "asd",
                "status": "needsAction"
              },
              {
                "id": "zxc",
                "title": "Child task, level 3",
                "parent": "qwe",
                "status": "completed"
              }
            ]
          }
        )");
      }));

  TestFuture<const std::vector<GlanceablesTask>&> future;
  auto cancel_closure =
      client()->GetTasks(future.GetCallback(), "test-task-list-id");
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());

  const auto& root_tasks = future.Get();
  EXPECT_EQ(root_tasks.size(), 1u);
  EXPECT_EQ(root_tasks.at(0).id, "asd");
  EXPECT_EQ(root_tasks.at(0).title, "Parent task, level 1");
  EXPECT_EQ(root_tasks.at(0).completed, false);

  const auto& subtasks_level_2 = root_tasks.at(0).subtasks;
  EXPECT_EQ(subtasks_level_2.size(), 1u);
  EXPECT_EQ(subtasks_level_2.at(0).id, "qwe");
  EXPECT_EQ(subtasks_level_2.at(0).title, "Child task, level 2");
  EXPECT_EQ(subtasks_level_2.at(0).completed, false);

  const auto& subtasks_level_3 = subtasks_level_2.at(0).subtasks;
  EXPECT_EQ(subtasks_level_3.size(), 1u);
  EXPECT_EQ(subtasks_level_3.at(0).id, "zxc");
  EXPECT_EQ(subtasks_level_3.at(0).title, "Child task, level 3");
  EXPECT_EQ(subtasks_level_3.at(0).completed, true);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasksReturnsEmptyVectorOnHttpError) {
  set_generate_response_callback(base::BindLambdaForTesting(
      [](const HttpRequest& request) { return CreateFailedResponse(); }));

  TestFuture<const std::vector<GlanceablesTask>&> future;
  auto cancel_closure =
      client()->GetTasks(future.GetCallback(), "test-task-list-id");
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());

  const auto& root_tasks = future.Get();
  EXPECT_EQ(root_tasks.size(), 0u);
}

TEST_F(GlanceablesTasksClientImplTest,
       GetTasksReturnsEmptyVectorOnConversionError) {
  set_generate_response_callback(
      base::BindLambdaForTesting([](const HttpRequest& request) {
        return CreateSuccessfulResponse(R"(
          {
            "kind": "tasks#tasks",
            "items": [
              {
                "id": "asd",
                "title": "Parent task",
                "status": "needsAction"
              },
              {
                "id": "qwe",
                "title": "Child task",
                "parent": "asd1",
                "status": "needsAction"
              }
            ]
          }
        )");
      }));

  TestFuture<const std::vector<GlanceablesTask>&> future;
  auto cancel_closure =
      client()->GetTasks(future.GetCallback(), "test-task-list-id");
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());

  const auto& root_tasks = future.Get();
  EXPECT_EQ(root_tasks.size(), 0u);
}

}  // namespace ash
