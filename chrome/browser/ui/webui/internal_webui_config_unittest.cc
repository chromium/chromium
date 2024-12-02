// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internal_webui_config.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace {

const char kTestWebUIHost[] = "test";
const char kTestWebUIURL[] = "chrome://test";
const char kTestUserFacingWebUIHost[] = "test-user-facing";
const char kTestUserFacingWebUIURL[] = "chrome://test-user-facing";

class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {
    web_ui->OverrideTitle(u"test title");
  }
};

class TestInternalWebUIConfig
    : public webui::DefaultInternalWebUIConfig<TestWebUIController> {
 public:
  TestInternalWebUIConfig() : DefaultInternalWebUIConfig(kTestWebUIHost) {}
};

class TestUserFacingWebUIController : public content::WebUIController {
 public:
  explicit TestUserFacingWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {}
};

class TestUserFacingWebUIConfig
    : public content::DefaultWebUIConfig<TestUserFacingWebUIController> {
 public:
  TestUserFacingWebUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kTestUserFacingWebUIHost) {
  }
};

}  // namespace

class InternalWebUIConfigFeatureDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InternalWebUIConfigFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(features::kInternalOnlyUisPref);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the UI is always enabled if the feature flag is false.
TEST_F(InternalWebUIConfigFeatureDisabledTest, AlwaysEnabled) {
  std::unique_ptr<TestInternalWebUIConfig> config =
      std::make_unique<TestInternalWebUIConfig>();

  // Check that DefaultInternalWebUIConfig creates TestWebUIController.
  content::TestWebUI test_webui;
  std::unique_ptr<content::WebUIController> webui_controller =
      config->CreateWebUIController(&test_webui, GURL(kTestWebUIURL));
  EXPECT_NE(webui_controller, nullptr);
  EXPECT_EQ(u"test title", webui_controller->web_ui()->GetOverriddenTitle());
}

class InternalWebUIConfigFeatureEnabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InternalWebUIConfigFeatureEnabledTest()
      : ChromeRenderViewHostTestHarness(),
        local_state_(TestingBrowserProcess::GetGlobal()) {}

 protected:
  ScopedTestingLocalState local_state_;

 private:
  base::test::ScopedFeatureList feature_list_{features::kInternalOnlyUisPref};
};

// Tests that the UI is disabled if the feature flag is on and the pref is not
// set and enabled if the pref is set.
TEST_F(InternalWebUIConfigFeatureEnabledTest, EnabledWhenPrefSet) {
  std::unique_ptr<TestInternalWebUIConfig> config =
      std::make_unique<TestInternalWebUIConfig>();
  local_state_.Get()->SetBoolean(prefs::kInternalOnlyUisEnabled, false);
  content::TestWebUI test_webui;
  std::unique_ptr<TestingProfile> profile = CreateTestingProfile();
  auto web_contents =
      content::WebContentsTester::CreateTestWebContents(profile.get(), nullptr);
  test_webui.set_web_contents(web_contents.get());
  std::unique_ptr<content::WebUIController> webui_controller =
      config->CreateWebUIController(&test_webui, GURL(kTestWebUIURL));
  EXPECT_NE(webui_controller, nullptr);
  // Does not create the test controller, because the
  // InternalDebugPagesDisabledUI controller is created instead.
  EXPECT_EQ(u"", webui_controller->web_ui()->GetOverriddenTitle());

  // Check that DefaultInternalWebUIConfig creates TestWebUIController when
  // the pref is enabled.
  local_state_.Get()->SetBoolean(prefs::kInternalOnlyUisEnabled, true);
  content::TestWebUI test_webui_pref_enabled;
  std::unique_ptr<content::WebUIController> webui_controller_pref_enabled =
      config->CreateWebUIController(&test_webui_pref_enabled,
                                    GURL(kTestWebUIURL));
  EXPECT_NE(webui_controller_pref_enabled, nullptr);
  EXPECT_EQ(u"test title",
            webui_controller_pref_enabled->web_ui()->GetOverriddenTitle());
}

// Tests the IsInternalWebUI method.
TEST_F(InternalWebUIConfigFeatureEnabledTest, IsInternalWebUIConfig) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestInternalWebUIConfig>());
  EXPECT_TRUE(webui::IsInternalWebUI(GURL(kTestWebUIURL)));
  content::ScopedWebUIConfigRegistration user_facing_registration(
      std::make_unique<TestUserFacingWebUIConfig>());
  EXPECT_FALSE(webui::IsInternalWebUI(GURL(kTestUserFacingWebUIURL)));
}
