// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::StrictMock;

constexpr char kTestAccount[] = "user@gmail.com";

class LeakDetectionCheckFactoryImplTest : public testing::Test {
 public:
  LeakDetectionCheckFactoryImplTest() = default;
  ~LeakDetectionCheckFactoryImplTest() override = default;

  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  MockLeakDetectionDelegateInterface& delegate() { return delegate_; }
  const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }
  LeakDetectionCheckFactoryImpl& request_factory() { return request_factory_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  StrictMock<MockLeakDetectionDelegateInterface> delegate_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  LeakDetectionCheckFactoryImpl request_factory_;
};

}  // namespace

TEST_F(LeakDetectionCheckFactoryImplTest, DisabledFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kLeakDetection);

  identity_env().SetPrimaryAccount(kTestAccount);
  EXPECT_FALSE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory()));
}

TEST_F(LeakDetectionCheckFactoryImplTest, SignedOut) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kLeakDetection);

  EXPECT_CALL(delegate(), OnError(LeakDetectionError::kNotSignIn));
  EXPECT_FALSE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory()));
}

TEST_F(LeakDetectionCheckFactoryImplTest, SignedIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kLeakDetection);

  AccountInfo info = identity_env().MakeAccountAvailable(kTestAccount);
  identity_env().SetCookieAccounts({{info.email, info.gaia}});
  identity_env().SetRefreshTokenForAccount(info.account_id);
  EXPECT_TRUE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory()));
}

TEST_F(LeakDetectionCheckFactoryImplTest, SignedInAndSyncing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kLeakDetection);

  identity_env().SetPrimaryAccount(kTestAccount);
  EXPECT_TRUE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory()));
}

}  // namespace password_manager
