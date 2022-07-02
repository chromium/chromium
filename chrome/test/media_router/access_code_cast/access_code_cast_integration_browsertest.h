// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_ACCESS_CODE_CAST_ACCESS_CODE_CAST_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_ACCESS_CODE_CAST_ACCESS_CODE_CAST_INTEGRATION_BROWSERTEST_H_

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"

namespace media_router {

// Base class that generates an access code cast dialog and all objects that are
// required for interacting with the dialog.
class AccessCodeCastIntegrationBrowserTest : public MojoWebUIBrowserTest {
 public:
  AccessCodeCastIntegrationBrowserTest();
  ~AccessCodeCastIntegrationBrowserTest() override;

  void SetUp() override;
  void SetUpInProcessBrowserTestFixture() override;

  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  // Makes user signed-in with the stub account's email and sets the
  // |consent_level| for that account.
  void SetUpPrimaryAccountWithHostedDomain(signin::ConsentLevel consent_level);

  void PreShow();

  // TODO(b/235882005): This function will hang on Wayland linux tests. If a
  // test case is added that uses this function, make sure to add that test case
  // to  //testing/buildbot/filters/ozone-linux.wayland_browser_tests.filter
  content::WebContents* ShowDialog();
  void CloseDialog(content::WebContents* dialog_contents);

  void SetAccessCode(std::string access_code,
                     content::WebContents* dialog_contents);
  void PressSubmit(content::WebContents* dialog_contents);

  // This function spins the run loop until an error code is surfaced.
  int WaitForAddSinkErrorCode(content::WebContents* dialog_contents);

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  std::unique_ptr<KeyedService> CreateAccessCodeCastSinkService(
      content::BrowserContext* context);

  void SpinRunLoop();

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  std::string GetElementScript() {
    return std::string(
        R"(document.getElementsByTagName('access-code-cast-app')[0])");
  }

  std::string GetErrorElementScript() {
    return std::string(GetElementScript() + ".$['errorMessage']");
  }

 protected:
  raw_ptr<media_router::MockMediaRouter> media_router_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  content::EvalJsResult EvalJs(const std::string& string_value,
                               content::WebContents* web_contents);

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_ACCESS_CODE_CAST_ACCESS_CODE_CAST_INTEGRATION_BROWSERTEST_H_
