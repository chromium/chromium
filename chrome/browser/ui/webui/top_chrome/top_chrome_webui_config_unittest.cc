// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"

#include <optional>
#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_web_ui.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace {

using TopChromeWebUIConfigTest = ChromeRenderViewHostTestHarness;

const char kTestWebUIHost[] = "test";
const char kTestWebUIURL[] = "chrome://test";

class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {}
};

class TestWebUIConfig
    : public content::DefaultWebUIConfig<TestWebUIController> {
 public:
  TestWebUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kTestWebUIHost) {}
};

const char kTestTopChromeWebUIHost[] = "test.top-chrome";
const char kTestTopChromeWebUIURL[] = "chrome://test.top-chrome";

class TestTopChromeWebUIController : public TopChromeWebUIController {
 public:
  explicit TestTopChromeWebUIController(content::WebUI* web_ui)
      : TopChromeWebUIController(web_ui) {}

  static constexpr std::string_view GetWebUIName() { return "Test"; }
};

class TestTopChromeWebUIConfig
    : public DefaultTopChromeWebUIConfig<TestTopChromeWebUIController> {
 public:
  explicit TestTopChromeWebUIConfig(
      std::string_view scheme = content::kChromeUIScheme,
      ProcessModel process_model = ProcessModel::kTopChromeProcess)
      : DefaultTopChromeWebUIConfig(scheme,
                                    kTestTopChromeWebUIHost,
                                    process_model) {}
};

}  // namespace

// Tests that TopChromeWebUIConfig cannot be retrieved for a regular WebUI.
TEST_F(TopChromeWebUIConfigTest, RegularWebUI) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestWebUIConfig>());
  TopChromeWebUIConfig* config =
      TopChromeWebUIConfig::From(profile(), GURL(kTestWebUIURL));
  EXPECT_EQ(config, nullptr);
}

// Tests that TopChromeWebUIConfig can be retrieved for a top-chrome WebUI.
TEST_F(TopChromeWebUIConfigTest, TopChromeWebUI) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestTopChromeWebUIConfig>());
  TopChromeWebUIConfig* config =
      TopChromeWebUIConfig::From(profile(), GURL(kTestTopChromeWebUIURL));
  EXPECT_NE(config, nullptr);
  EXPECT_FALSE(config->GetWebUIName().empty());
}

// Tests that DefaultTopChromeWebUIConfig can create WebUIController.
TEST_F(TopChromeWebUIConfigTest, DefaultTopChromeWebUIConfig) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestTopChromeWebUIConfig>());
  TopChromeWebUIConfig* config =
      TopChromeWebUIConfig::From(profile(), GURL(kTestTopChromeWebUIURL));
  content::TestWebUI test_webui;
  std::unique_ptr<content::WebUIController> webui_controller =
      config->CreateWebUIController(&test_webui, GURL(kTestTopChromeWebUIURL));
  EXPECT_NE(webui_controller, nullptr);
}

// Tests that trusted chrome:// TopChromeWebUIConfigs share the top-chrome
// process lock host by default, while untrusted configs and configs opting into
// `ProcessModel::kDedicatedProcess` lock to their own site URL.
TEST_F(TopChromeWebUIConfigTest, ProcessLockHost) {
  TestTopChromeWebUIConfig default_config;
  EXPECT_EQ(default_config.process_lock_host(),
            chrome::kChromeUITopChromeDomain);

  TestTopChromeWebUIConfig untrusted_config(content::kChromeUIUntrustedScheme);
  EXPECT_EQ(untrusted_config.process_lock_host(), std::nullopt);

  TestTopChromeWebUIConfig dedicated_config(
      content::kChromeUIScheme,
      TopChromeWebUIConfig::ProcessModel::kDedicatedProcess);
  EXPECT_EQ(dedicated_config.process_lock_host(), std::nullopt);
}
