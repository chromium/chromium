// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::StrictMock;

constexpr char kTestAccount[] = "user@gmail.com";
constexpr version_info::Channel kChannel = version_info::Channel::UNKNOWN;

class LeakDetectionCheckFactoryImplTest : public testing::Test {
 public:
  LeakDetectionCheckFactoryImplTest() = default;
  ~LeakDetectionCheckFactoryImplTest() override = default;

  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  MockLeakDetectionDelegateInterface& delegate() { return delegate_; }
  MockBulkLeakCheckDelegateInterface& bulk_delegate() { return bulk_delegate_; }
  const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }
  LeakDetectionCheckFactoryImpl& request_factory() { return request_factory_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  StrictMock<MockLeakDetectionDelegateInterface> delegate_;
  StrictMock<MockBulkLeakCheckDelegateInterface> bulk_delegate_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_ =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  LeakDetectionCheckFactoryImpl request_factory_;
};

}  // namespace

TEST_F(LeakDetectionCheckFactoryImplTest, SignedOut) {
  EXPECT_TRUE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory(),
      kChannel));
}

TEST_F(LeakDetectionCheckFactoryImplTest, BulkCheck_SignedOut) {
  EXPECT_CALL(bulk_delegate(), OnError(LeakDetectionError::kNotSignIn));
  EXPECT_FALSE(request_factory().TryCreateBulkLeakCheck(
      &bulk_delegate(), identity_env().identity_manager(),
      url_loader_factory()));
}

TEST_F(LeakDetectionCheckFactoryImplTest, SignedIn) {
  identity_env().MakePrimaryAccountAvailable(kTestAccount,
                                             signin::ConsentLevel::kSignin);
  EXPECT_TRUE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory(),
      kChannel));
}

TEST_F(LeakDetectionCheckFactoryImplTest, BulkCheck_SignedIn) {
  identity_env().MakePrimaryAccountAvailable(kTestAccount,
                                             signin::ConsentLevel::kSignin);
  EXPECT_TRUE(request_factory().TryCreateBulkLeakCheck(
      &bulk_delegate(), identity_env().identity_manager(),
      url_loader_factory()));
}

TEST_F(LeakDetectionCheckFactoryImplTest, SignedInAndSyncing) {
  identity_env().SetPrimaryAccount(kTestAccount, signin::ConsentLevel::kSync);
  EXPECT_TRUE(request_factory().TryCreateLeakCheck(
      &delegate(), identity_env().identity_manager(), url_loader_factory(),
      kChannel));
}

TEST_F(LeakDetectionCheckFactoryImplTest, BulkCheck_SignedInAndSyncing) {
  identity_env().MakePrimaryAccountAvailable(kTestAccount,
                                             signin::ConsentLevel::kSync);
  EXPECT_TRUE(request_factory().TryCreateBulkLeakCheck(
      &bulk_delegate(), identity_env().identity_manager(),
      url_loader_factory()));
}

}  // namespace password_manager
