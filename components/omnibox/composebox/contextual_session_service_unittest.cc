// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/contextual_session_service.h"

#include "base/test/task_environment.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/variations_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::IsNull;
using testing::NotNull;

class FakeVariationsClient : public variations::VariationsClient {
 public:
  FakeVariationsClient() = default;
  ~FakeVariationsClient() override = default;

  // variations::VariationsClient:
  bool IsOffTheRecord() const override { return false; }
  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::mojom::VariationsHeaders::New();
  }
};

}  // namespace

class ContextualSessionServiceTest : public testing::Test {
 public:
  ContextualSessionServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    service_ = std::make_unique<ContextualSessionService>(
        identity_test_env_.identity_manager(), test_shared_loader_factory_,
        search_engines_test_environment_.template_url_service(),
        &fake_variations_client_, version_info::Channel::UNKNOWN, "en-US");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  FakeVariationsClient fake_variations_client_;
  std::unique_ptr<ContextualSessionService> service_;
};

TEST_F(ContextualSessionServiceTest, Session) {
  // Try to get a session that does not exist.
  auto bad_handle = service_->GetSession(base::UnguessableToken::Create());
  ASSERT_THAT(bad_handle, IsNull());

  // Create a new session.
  auto config_params1 = std::make_unique<
      ComposeboxQueryController::QueryControllerConfigParams>();
  config_params1->send_lns_surface = false;
  config_params1->suppress_lns_surface_param_if_no_image = true;
  config_params1->enable_multi_context_input_flow = false;
  config_params1->enable_viewport_images = false;
  auto session1_handle1 = service_->CreateSession(std::move(config_params1));
  ASSERT_THAT(session1_handle1, NotNull());
  ASSERT_THAT(session1_handle1->GetController(), NotNull());

  // Create another new session.
  auto config_params2 = std::make_unique<
      ComposeboxQueryController::QueryControllerConfigParams>();
  config_params2->send_lns_surface = false;
  config_params2->suppress_lns_surface_param_if_no_image = true;
  config_params2->enable_multi_context_input_flow = false;
  config_params2->enable_viewport_images = false;
  auto session2_handle1 = service_->CreateSession(std::move(config_params2));
  ASSERT_THAT(session2_handle1, NotNull());
  ASSERT_THAT(session2_handle1->GetController(), NotNull());

  // Get a new handle to session two.
  auto session2_handle2 = service_->GetSession(session2_handle1->session_id());
  ASSERT_THAT(session2_handle2, NotNull());
  EXPECT_EQ(session2_handle2->GetController(),
            session2_handle1->GetController());

  // Release the first handle to session two. The session should still be alive.
  session2_handle1.reset();
  auto session2_handle3 = service_->GetSession(session2_handle2->session_id());
  ASSERT_THAT(session2_handle3, NotNull());
  EXPECT_EQ(session2_handle3->GetController(),
            session2_handle2->GetController());

  // Release the remaining handles to session two. The session should be
  // released.
  auto session_id = session2_handle2->session_id();
  session2_handle2.reset();
  session2_handle3.reset();
  auto session2_handle4 = service_->GetSession(session_id);
  ASSERT_THAT(session2_handle4, IsNull());

  // Get a new handle to session one.
  auto session1_handle2 = service_->GetSession(session1_handle1->session_id());
  ASSERT_THAT(session1_handle2, NotNull());
  EXPECT_EQ(session1_handle2->GetController(),
            session1_handle1->GetController());
}
