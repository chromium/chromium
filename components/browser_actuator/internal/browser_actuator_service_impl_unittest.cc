// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

class BrowserActuatorServiceImplTest : public testing::Test {
 protected:
  BrowserActuatorServiceImplTest()
      : identity_test_env_(&test_url_loader_factory_) {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }
  ~BrowserActuatorServiceImplTest() override = default;

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

TEST_F(BrowserActuatorServiceImplTest, IsInitialized) {
  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());
  EXPECT_TRUE(service.IsInitialized());
}
TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsValidChannel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl service(test_shared_url_loader_factory_,
                                     identity_test_env_.identity_manager());
  EXPECT_NE(service.GetChannel(), nullptr);
}
TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsNullIfChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl disabled_service(
      test_shared_url_loader_factory_, identity_test_env_.identity_manager());
  EXPECT_EQ(disabled_service.GetChannel(), nullptr);
}

}  // namespace browser_actuator
