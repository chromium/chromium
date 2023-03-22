// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/tasks/tasks_api_requests.h"
#include "google_apis/tasks/tasks_api_response_types.h"
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
using ::google_apis::tasks::TaskLists;
using ::google_apis::tasks::Tasks;

// Helper class to temporary override `GaiaUrls` singleton.
class GaiaUrlsOverrider {
 public:
  GaiaUrlsOverrider() { GaiaUrls::SetInstanceForTesting(&test_gaia_urls_); }
  ~GaiaUrlsOverrider() { GaiaUrls::SetInstanceForTesting(nullptr); }

 private:
  GaiaUrls test_gaia_urls_;
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

  // Relative to "google_apis/test/data/".
  void set_file_path_for_response(const std::string& path) {
    file_path_for_response_ = path;
  }

  GlanceablesTasksClientImpl* client() { return client_.get(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleDataFileRequest(
      const net::test_server::HttpRequest& request) {
    return google_apis::test_util::CreateHttpResponseFromFile(
        google_apis::test_util::GetTestFilePath(file_path_for_response_));
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
  std::string file_path_for_response_;
  std::unique_ptr<GlanceablesTasksClientImpl> client_;
};

TEST_F(GlanceablesTasksClientImplTest, GetTaskLists) {
  set_file_path_for_response("tasks/task_lists.json");

  TestFuture<base::expected<std::unique_ptr<TaskLists>, ApiErrorCode>> future;
  auto cancel_closure = client()->GetTaskLists(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value()->items().size(), 2u);
}

TEST_F(GlanceablesTasksClientImplTest, GetTasks) {
  set_file_path_for_response("tasks/tasks.json");

  TestFuture<base::expected<std::unique_ptr<Tasks>, ApiErrorCode>> future;
  auto cancel_closure =
      client()->GetTasks(future.GetCallback(), "test-task-list-id");
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(cancel_closure.is_null());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get().value()->items().size(), 2u);
}

}  // namespace ash
