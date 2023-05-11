// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class GlanceablesClassroomClientImplTest : public testing::Test {
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
    client_ = std::make_unique<GlanceablesClassroomClientImpl>(
        create_request_sender_callback);
  }

  GlanceablesClassroomClientImpl* client() { return client_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
          /*network_service=*/nullptr,
          /*is_trusted=*/true);
  std::unique_ptr<GlanceablesClassroomClientImpl> client_;
};

TEST_F(GlanceablesClassroomClientImplTest, CreatesClientInstance) {
  ASSERT_TRUE(client());
}

}  // namespace ash
