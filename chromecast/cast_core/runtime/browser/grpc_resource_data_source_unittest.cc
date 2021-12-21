// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc_resource_data_source.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"

using ::testing::IsEmpty;

namespace chromecast {

class GrpcResourceDataSourceTest : public ::testing::Test {
 public:
  GrpcResourceDataSourceTest()
      : grpc_resource_data_source_(
            new GrpcResourceDataSource("chrome", true, *stub_.get())) {}

 protected:
  std::string GetMimeType(const std::string& path) {
    return grpc_resource_data_source_->GetMimeType(path);
  }

  std::string GetAccessControlAllowOriginForOrigin(const std::string& origin) {
    return grpc_resource_data_source_->GetAccessControlAllowOriginForOrigin(
        origin);
  }

  base::test::TaskEnvironment task_environment_;
  std::string core_application_service_address_ = "fake-address";
  std::unique_ptr<cast::v2::CoreApplicationService::Stub> stub_ =
      cast::v2::CoreApplicationService::NewStub(
          grpc::CreateChannel(core_application_service_address_,
                              grpc::InsecureChannelCredentials()));
  GrpcResourceDataSource* grpc_resource_data_source_;
};

TEST_F(GrpcResourceDataSourceTest, GetMimeTypeEmptyParam) {
  EXPECT_EQ(GetMimeType(""), "text/html");
}

TEST_F(GrpcResourceDataSourceTest, GetMimeTypeRemoteUrl) {
  EXPECT_EQ(GetMimeType("?resource=http://google.com"), "text/html");
}

TEST_F(GrpcResourceDataSourceTest, GetMimeTypeNoExtension) {
  EXPECT_EQ(GetMimeType("fontscss"), "text/html");
}

TEST_F(GrpcResourceDataSourceTest, GetMimeTypeSuccess) {
  EXPECT_EQ(GetMimeType("fonts.css"), "text/css");
}

TEST_F(GrpcResourceDataSourceTest,
       GetAccessControlAllowOriginForOriginAllowedOrigin) {
  EXPECT_EQ(GetAccessControlAllowOriginForOrigin("chrome://"), "chrome://");
}

TEST_F(GrpcResourceDataSourceTest,
       GetAccessControlAllowOriginForOriginDisallowedOrigin) {
  EXPECT_THAT(GetAccessControlAllowOriginForOrigin("chrome-resources://"),
              IsEmpty());
}

}  // namespace chromecast
