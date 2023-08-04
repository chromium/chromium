// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/web_ui_browsertest_util.h"

namespace ash {
namespace {

static constexpr const char kTestHost[] = "test-host";
static constexpr const char kTestUrl[] = "chrome://test-host";

// WebUIController that registers a URLDataSource which serves an html page with
// our import map and a Trusted Types CSP that allows us to inject script tags.
class CrosComponentsUI : public content::WebUIController {
 public:
  explicit CrosComponentsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    content::WebUIDataSource* source =
        content::WebUIDataSource::CreateAndAdd(browser_context, kTestHost);
    source->SetRequestFilter(
        base::BindRepeating([](const std::string& path) { return true; }),
        base::BindRepeating(
            [](const std::string& path,
               content::WebUIDataSource::GotDataCallback callback) {
              std::move(callback).Run(new base::RefCountedString(""));
            }));

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src chrome://resources 'self' 'unsafe-inline';");

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::TrustedTypes,
        "trusted-types lit-html script-js-static;");
  }

  ~CrosComponentsUI() override = default;
};

class CrosComponentsUIConfig : public content::WebUIConfig {
 public:
  CrosComponentsUIConfig() : WebUIConfig("chrome", kTestHost) {}

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<CrosComponentsUI>(web_ui);
  }
};

struct ComponentTestData {
  // The URL of the component.
  base::StringPiece script_src;
  // The name of the custom element.
  base::StringPiece component_name;
  // Used to generate the test name. GTest names are only allowed to use
  // alphanumeric characters.
  base::StringPiece gtest_name;
};

// Used by GTest to generate the test name.
std::ostream& operator<<(std::ostream& os,
                         const ComponentTestData& component_test_data) {
  os << component_test_data.gtest_name;
  return os;
}

class CrosComponentsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<ComponentTestData> {
 public:
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

}  // namespace

// The test imports a `script_src` for the component and waits for the custom
// element with `component_name` to be defined. If the test fails the URL that
// couldn't be loaded is printed.
IN_PROC_BROWSER_TEST_P(CrosComponentsBrowserTest, NoRuntimeErrors) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<CrosComponentsUIConfig>());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestUrl)));

  static constexpr const char kWaitElementToBeDefined[] = R"(
(async () => {
  const errors = []
  globalThis.onerror = (e) => {
    errors.push(e);
  };

  let injectScript = new Promise((resolve, reject) => {
    const staticUrlPolicy = trustedTypes.createPolicy(
      'script-js-static',
      {createScriptURL: () => '%s'});

    const script = document.createElement('script');
    script.src = staticUrlPolicy.createScriptURL('')
    script.type = "module"
    script.onerror = () => { reject('Failed to load script.'); };
    script.onload = resolve;
    document.body.appendChild(script);
  });

  await injectScript;

  if (errors.length != 0) {
    return errors;
  }

  await customElements.whenDefined('%s');

  return true;
})();
)";

  content::DevToolsInspectorLogWatcher log_watcher(GetActiveWebContents());
  auto result = content::EvalJs(
      GetActiveWebContents(),
      base::StringPrintf(kWaitElementToBeDefined, GetParam().script_src.data(),
                         GetParam().component_name.data()));
  log_watcher.FlushAndStopWatching();

  EXPECT_EQ(true, result) << log_watcher.last_message() << " - "
                          << log_watcher.last_url();
}

static constexpr const ComponentTestData kComponentsTestData[] = {
    {
        .script_src = "chrome://resources/cros_components/button/button.js",
        .component_name = "cros-button",
        .gtest_name = "CrosButton",
    },
    {
        .script_src = "chrome://resources/cros_components/checkbox/checkbox.js",
        .component_name = "cros-checkbox",
        .gtest_name = "CrosCheckbox",
    },
    {
        .script_src = "chrome://resources/cros_components/radio/radio.js",
        .component_name = "cros-radio",
        .gtest_name = "CrosRadio",
    },
    {
        .script_src = "chrome://resources/cros_components/switch/switch.js",
        .component_name = "cros-switch",
        .gtest_name = "CrosSwitch",
    },
    {
        .script_src = "chrome://resources/cros_components/slider/slider.js",
        .component_name = "cros-slider",
        .gtest_name = "CrosSlider",
    },
    {
        .script_src = "chrome://resources/cros_components/tag/tag.js",
        .component_name = "cros-tag",
        .gtest_name = "CrosTag",
    },
};

INSTANTIATE_TEST_SUITE_P(All,
                         CrosComponentsBrowserTest,
                         testing::ValuesIn(kComponentsTestData),
                         testing::PrintToStringParamName());

}  // namespace ash
