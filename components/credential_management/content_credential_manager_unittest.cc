// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/content_credential_manager.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/credential_management/credential_manager_interface.h"
#include "components/credential_management/features.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace credential_management {

class MockCredentialManagerImpl
    : public credential_management::CredentialManagerInterface {
 public:
  MOCK_METHOD(void,
              Store,
              (const password_manager::CredentialInfo& credential,
               credential_management::StoreCallback callback),
              (override));
  MOCK_METHOD(void,
              PreventSilentAccess,
              (PreventSilentAccessCallback callback),
              (override));
  MOCK_METHOD(void,
              Get,
              (password_manager::CredentialMediationRequirement mediation,
               bool include_passwords,
               const std::vector<GURL>& federations,
               GetCallback callback),
              (override));
  MOCK_METHOD(void, ResetAfterDisconnecting, (), (override));
};

// Tests ContentCredentialManager functionality.
class ContentCredentialManagerTest : public content::RenderViewHostTestHarness {
 public:
  ContentCredentialManagerTest() = default;

  ~ContentCredentialManagerTest() override = default;
};

TEST_F(ContentCredentialManagerTest,
       StoreCallIsForwardedToCredentialManagerImpl) {
  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto mock_credential_manager = mock_credential_manager_unique_ptr.get();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  EXPECT_CALL(*mock_credential_manager, Store);
  content_credential_manager.Store(password_manager::CredentialInfo(),
                                   StoreCallback());
}

TEST_F(ContentCredentialManagerTest, BindRequest_Success) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());

  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  mojo::Remote<blink::mojom::CredentialManager> remote;
  content_credential_manager.BindRequest(main_rfh(),
                                         remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(content_credential_manager.HasBinding());
}

TEST_F(ContentCredentialManagerTest, BindRequest_RejectedForPdfProcess) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());

  process()->SetIsPdf(true);

  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  mojo::Remote<blink::mojom::CredentialManager> remote;
  content_credential_manager.BindRequest(main_rfh(),
                                         remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ("CredentialManager is not allowed for this process or origin.",
            bad_message_observer.WaitForBadMessage());
  EXPECT_FALSE(content_credential_manager.HasBinding());
}

TEST_F(ContentCredentialManagerTest,
       BindRequest_AllowedForPdfProcessWhenKillSwitchDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kCredentialManagerRejectUnauthorizedProcessOrOrigin);

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());

  process()->SetIsPdf(true);

  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  mojo::Remote<blink::mojom::CredentialManager> remote;
  content_credential_manager.BindRequest(main_rfh(),
                                         remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(content_credential_manager.HasBinding());
}

TEST_F(ContentCredentialManagerTest, BindRequest_RejectedForSubframe) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  mojo::Remote<blink::mojom::CredentialManager> remote;
  content_credential_manager.BindRequest(subframe,
                                         remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(content_credential_manager.HasBinding());
}

TEST_F(ContentCredentialManagerTest,
       BindRequest_RejectedForUnauthorizedOrigin) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());
  content::OverrideLastCommittedOrigin(
      main_rfh(), url::Origin::Create(GURL("https://unauthorized.com")));

  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  mojo::Remote<blink::mojom::CredentialManager> remote;
  content_credential_manager.BindRequest(main_rfh(),
                                         remote.BindNewPipeAndPassReceiver());
  EXPECT_EQ("CredentialManager is not allowed for this process or origin.",
            bad_message_observer.WaitForBadMessage());
  EXPECT_FALSE(content_credential_manager.HasBinding());
}

TEST_F(ContentCredentialManagerTest, BindRequest_RejectedForOpaqueOrigin) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("data:text/html,Hello"), main_rfh());
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  auto mock_credential_manager_unique_ptr =
      std::make_unique<testing::StrictMock<MockCredentialManagerImpl>>();
  auto content_credential_manager =
      ContentCredentialManager(std::move(mock_credential_manager_unique_ptr));

  mojo::Remote<blink::mojom::CredentialManager> remote;
  content_credential_manager.BindRequest(main_rfh(),
                                         remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(content_credential_manager.HasBinding());
}
}  // namespace credential_management
