// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_client.h"

#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/credential_management/content_credential_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"

namespace android_autofill {
namespace {

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
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));
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
      GURL("https://example.test"), main_rfh());

  // Binding is disconnected after navigation. This is the main purpose of this
  // test.
  EXPECT_FALSE(credential_manager->HasBinding());

  // Binding is established again.
  remote.reset();
  credential_manager->BindRequest(main_rfh(),
                                  remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(credential_manager->HasBinding());
}

}  // namespace
}  // namespace android_autofill
