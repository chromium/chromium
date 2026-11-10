// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_client.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/credential_management/content_credential_manager.h"
#include "components/security_state/content/android/security_state_client.h"
#include "components/security_state/content/android/security_state_model_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"

namespace android_autofill {
namespace {

using ::testing::Return;
using ::testing::StrictMock;

class MockSecurityStateModelDelegate : public SecurityStateModelDelegate {
 public:
  MOCK_METHOD(security_state::SecurityLevel,
              GetSecurityLevel,
              (content::WebContents*),
              (const, override));
  MOCK_METHOD(security_state::SecurityLevel,
              GetCommittedSecurityLevel,
              (content::WebContents*),
              (const, override));
  MOCK_METHOD(security_state::MaliciousContentStatus,
              GetMaliciousContentStatus,
              (content::WebContents*),
              (const, override));
  MOCK_METHOD(std::unique_ptr<security_state::VisibleSecurityState>,
              GetVisibleSecurityState,
              (content::WebContents*),
              (const, override));
};

class ScopedSecurityStateModelDelegate
    : public security_state::SecurityStateClient {
 public:
  ScopedSecurityStateModelDelegate() {
    auto delegate =
        std::make_unique<StrictMock<MockSecurityStateModelDelegate>>();
    delegate_ = delegate.get();
    owned_delegate_ = std::move(delegate);
    security_state::SetSecurityStateClient(this);
  }

  ~ScopedSecurityStateModelDelegate() override {
    delegate_ = nullptr;
    security_state::SetSecurityStateClient(nullptr);
  }

  MockSecurityStateModelDelegate& delegate() { return *delegate_; }

  std::unique_ptr<SecurityStateModelDelegate>
  MaybeCreateSecurityStateModelDelegate() override {
    return std::move(owned_delegate_);
  }

 private:
  std::unique_ptr<StrictMock<MockSecurityStateModelDelegate>> owned_delegate_;
  raw_ptr<StrictMock<MockSecurityStateModelDelegate>> delegate_ = nullptr;
};

// This test class is needed to make the constructor public.
class TestAndroidAutofillClient : public AndroidAutofillClient {
 public:
  TestAndroidAutofillClient(content::WebContents* web_contents)
      : AndroidAutofillClient(web_contents) {}
  ~TestAndroidAutofillClient() override = default;
};

class AndroidAutofillClientTest : public content::RenderViewHostTestHarness {
 public:
  AndroidAutofillClientTest() = default;
  ~AndroidAutofillClientTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    security_state::SetSecurityStateClient(nullptr);
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));
  }

  void TearDown() override {
    security_state::SetSecurityStateClient(nullptr);
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  TestAndroidAutofillClient* client() {
    return test_autofill_client_injector_[web_contents()];
  }

  autofill::ContentAutofillDriver* driver(content::RenderFrameHost* rfh) {
    return autofill::ContentAutofillDriver::GetForRenderFrameHost(rfh);
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  autofill::TestAutofillClientInjector<TestAndroidAutofillClient>
      test_autofill_client_injector_;
};

// Verify that the ablation study returns kDefault for WebView.
TEST_F(AndroidAutofillClientTest, TestAblationStudyReturnsDefault) {
  const autofill::AutofillAblationStudy& ablation_study =
      client()->GetAblationStudy();
  EXPECT_EQ(ablation_study.GetAblationGroup(
                web_contents()->GetLastCommittedURL(),
                autofill::FormTypeForAblationStudy::kAddress, nullptr),
            autofill::AblationGroup::kDefault);
}

// Verify that the credential manager binding is disconnected upon navigating
// to a different page so that the binding is not stale.
TEST_F(AndroidAutofillClientTest,
       DisconnectsCredentialManagerBindingOnNavigation) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.test"), main_rfh());

  credential_management::ContentCredentialManager* credential_manager =
      client()->GetContentCredentialManager();
  ASSERT_TRUE(credential_manager);

  mojo::Remote<blink::mojom::CredentialManager> remote;
  // This test is only simulating what really happens so the binding is
  // established manually. To really test this, we would need to create a
  // browser test.
  credential_manager->BindRequest(main_rfh(),
                                  remote.BindNewPipeAndPassReceiver());
  // Binding is established.
  EXPECT_TRUE(credential_manager->HasBinding());

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.test"), main_rfh());

  // Binding is disconnected after navigation. This is the main purpose of this
  // test.
  EXPECT_FALSE(credential_manager->HasBinding());

  // Binding is established again.
  remote.reset();
  credential_manager->BindRequest(main_rfh(),
                                  remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(credential_manager->HasBinding());
}

// Verifies GetSecurityLevelForUmaHistograms uses the committed security level.
TEST_F(AndroidAutofillClientTest, GetSecurityLevelForUmaHistograms) {
  EXPECT_EQ(client()->GetSecurityLevelForUmaHistograms(),
            security_state::SecurityLevel::SECURITY_LEVEL_COUNT);

  ScopedSecurityStateModelDelegate scoped_delegate;
  EXPECT_CALL(scoped_delegate.delegate(),
              GetCommittedSecurityLevel(web_contents()))
      .WillOnce(Return(security_state::SecurityLevel::SECURE));
  EXPECT_EQ(client()->GetSecurityLevelForUmaHistograms(),
            security_state::SecurityLevel::SECURE);
}

// Verifies IsContextSecure uses the committed state with a delegate.
TEST_F(AndroidAutofillClientTest,
       IsContextSecureWithSecurityStateModelDelegate) {
  ScopedSecurityStateModelDelegate scoped_delegate;

  // Committed on HTTP: not secure even if a browser-initiated HTTPS navigation
  // is pending.
  NavigateAndCommit(GURL("http://example.test"));
  std::unique_ptr<content::NavigationSimulator> pending_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://secure.test"), web_contents());
  pending_navigation->Start();
  EXPECT_FALSE(client()->IsContextSecure());

  // Once the HTTPS navigation commits, delegates to GetCommittedSecurityLevel.
  pending_navigation->Commit();
  EXPECT_CALL(scoped_delegate.delegate(),
              GetCommittedSecurityLevel(web_contents()))
      .WillOnce(Return(security_state::SecurityLevel::SECURE));
  EXPECT_TRUE(client()->IsContextSecure());

  EXPECT_CALL(scoped_delegate.delegate(),
              GetCommittedSecurityLevel(web_contents()))
      .WillOnce(Return(security_state::SecurityLevel::DANGEROUS));
  EXPECT_FALSE(client()->IsContextSecure());

  // Committed HTTPS error page (PAGE_TYPE_ERROR) is not secure.
  std::unique_ptr<content::NavigationSimulator> error_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://error.test"), web_contents());
  error_navigation->Fail(net::ERR_CERT_DATE_INVALID);
  error_navigation->CommitErrorPage();
  EXPECT_FALSE(client()->IsContextSecure());
}

// Verifies IsContextSecure uses the committed entry's SSLStatus fallback.
TEST_F(AndroidAutofillClientTest,
       IsContextSecureWithoutSecurityStateModelDelegate) {
  NavigateAndCommit(GURL("http://example.test"));
  std::unique_ptr<content::NavigationSimulator> pending_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://secure.test"), web_contents());
  pending_navigation->Start();
  EXPECT_FALSE(client()->IsContextSecure());

  pending_navigation->Commit();
  content::NavigationEntry* const entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);

  // Without a certificate on the SSLStatus, the context is not secure.
  EXPECT_FALSE(client()->IsContextSecure());
}

}  // namespace
}  // namespace android_autofill
