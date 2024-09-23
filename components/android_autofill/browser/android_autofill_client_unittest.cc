// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_client.h"

#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "content/public/test/test_renderer_host.h"

namespace android_autofill {
namespace {

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

}  // namespace
}  // namespace android_autofill
