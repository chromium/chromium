// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_cookie_provider/site_cookie_provider.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/site_cookie_provider/site_cookie_provider_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_cookie_provider {
namespace {

using ::testing::NiceMock;
using ::testing::StrictMock;

// Mock version of the core SiteCookieProvider engine for testing.
class MockSiteCookieProvider : public SiteCookieProvider {
 public:
  MockSiteCookieProvider() = default;
  ~MockSiteCookieProvider() override = default;

  MOCK_METHOD(void, UpdateState, (), (override));
};

class SiteCookieProviderServiceTest : public ::testing::Test {
 protected:
  SiteCookieProviderServiceTest() {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  ~SiteCookieProviderServiceTest() override = default;

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(SiteCookieProviderServiceTest, UpdatesStateOnStartupIfSignedIn) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto mock_provider = std::make_unique<StrictMock<MockSiteCookieProvider>>();
  MockSiteCookieProvider* mock_ptr = mock_provider.get();
  EXPECT_CALL(*mock_ptr, UpdateState()).Times(1);
  SiteCookieProviderService service(identity_test_env_.identity_manager(),
                                    std::move(mock_provider));
  service.Shutdown();
}

TEST_F(SiteCookieProviderServiceTest, UpdatesStateOnSignInEvent) {
  auto mock_provider = std::make_unique<StrictMock<MockSiteCookieProvider>>();
  MockSiteCookieProvider* mock_ptr = mock_provider.get();
  SiteCookieProviderService service(identity_test_env_.identity_manager(),
                                    std::move(mock_provider));
  EXPECT_CALL(*mock_ptr, UpdateState()).Times(1);
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  service.Shutdown();
}

}  // namespace
}  // namespace site_cookie_provider
