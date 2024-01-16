// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

class DataSharingNetworkLoaderImplTest : public testing::Test {
 public:
  DataSharingNetworkLoaderImplTest() = default;
  ~DataSharingNetworkLoaderImplTest() override = default;

  void SetUp() override {
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    data_sharing_network_loader_ =
        std::make_unique<DataSharingNetworkLoaderImpl>(
            std::move(test_url_loader_factory),
            identity_test_env_.identity_manager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DataSharingNetworkLoaderImpl> data_sharing_network_loader_;
};

TEST_F(DataSharingNetworkLoaderImplTest, LoadUrl) {
  base::RunLoop run_loop;
  data_sharing_network_loader_->LoadUrl(
      GURL("http://foo.com"), std::vector<std::string>(), std::string(),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindOnce(
          [](base::RunLoop* run_loop, std::unique_ptr<std::string> response) {
            ASSERT_FALSE(response);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace data_sharing
