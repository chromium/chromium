// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

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
#include "content/public/test/browser_test_utils.h"
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

class CrosComponentsUIConfig
    : public content::DefaultWebUIConfig<CrosComponentsUI> {
 public:
  CrosComponentsUIConfig() : DefaultWebUIConfig("chrome", kTestHost) {}
};

struct ComponentTestData {
  // The URL of the component.
  std::string_view script_src;
  // The name of the custom element.
  std::string_view component_name;
  // Used to generate the test name. GTest names are only allowed to use
  // alphanumeric characters.
  std::string_view gtest_name;
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
        .script_src = "chrome://resources/cros_components/badge/badge.js",
        .component_name = "cros-badge",
        .gtest_name = "CrosBadge",
    },
    {
        .script_src = "chrome://resources/cros_components/button/button.js",
        .component_name = "cros-button",
        .gtest_name = "CrosButton",
    },
    {
        .script_src = "chrome://resources/cros_components/card/card.js",
        .component_name = "cros-card",
        .gtest_name = "CrosCard",
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
        .script_src = "chrome://resources/cros_components/sidenav/sidenav.js",
        .component_name = "cros-sidenav",
        .gtest_name = "CrosSidenav",
    },
    {
        .script_src = "chrome://resources/cros_components/slider/slider.js",
        .component_name = "cros-slider",
        .gtest_name = "CrosSlider",
    },
    {
        .script_src =
            "chrome://resources/cros_components/tab_slider/tab-slider.js",
        .component_name = "cros-tab-slider",
        .gtest_name = "CrosTabSlider",
    },
    {
        .script_src =
            "chrome://resources/cros_components/tab_slider/tab-slider-item.js",
        .component_name = "cros-tab-slider-item",
        .gtest_name = "CrosTabSliderItem",
    },
    {
        .script_src = "chrome://resources/cros_components/tag/tag.js",
        .component_name = "cros-tag",
        .gtest_name = "CrosTag",
    },
    {
        .script_src =
            "chrome://resources/cros_components/textfield/textfield.js",
        .component_name = "cros-textfield",
        .gtest_name = "CrosTextfield",
    },
    {
        .script_src =
            "chrome://resources/cros_components/icon_button/icon-button.js",
        .component_name = "cros-icon-button",
        .gtest_name = "CrosIconButton",
    },
    {
        .script_src = "chrome://resources/cros_components/dropdown/dropdown.js",
        .component_name = "cros-dropdown",
        .gtest_name = "CrosDropdown",
    },
    {
        .script_src =
            "chrome://resources/cros_components/dropdown/dropdown_option.js",
        .component_name = "cros-dropdown-option",
        .gtest_name = "CrosDropdownOption",
    },
    {
        .script_src = "chrome://resources/cros_components/tabs/tabs.js",
        .component_name = "cros-tabs",
        .gtest_name = "CrosTabs",
    },
    {
        .script_src = "chrome://resources/cros_components/tabs/tab.js",
        .component_name = "cros-tab",
        .gtest_name = "CrosTab",
    },
    {
        .script_src = "chrome://resources/cros_components/menu/menu.js",
        .component_name = "cros-menu",
        .gtest_name = "CrosMenu",
    },
    {
        .script_src = "chrome://resources/cros_components/menu/menu_item.js",
        .component_name = "cros-menu-item",
        .gtest_name = "CrosMenuItem",
    },
    {
        .script_src =
            "chrome://resources/cros_components/menu/menu_separator.js",
        .component_name = "cros-menu-separator",
        .gtest_name = "CrosMenuSeparator",
    },
    {
        .script_src =
            "chrome://resources/cros_components/menu/sub_menu_item.js",
        .component_name = "cros-sub-menu-item",
        .gtest_name = "CrosSubMenuItem",
    },
    {
        .script_src = "chrome://resources/cros_components/snackbar/snackbar.js",
        .component_name = "cros-snackbar",
        .gtest_name = "CrosSnackbar",
    },
    {
        .script_src =
            "chrome://resources/cros_components/snackbar/snackbar-item.js",
        .component_name = "cros-snackbar-item",
        .gtest_name = "CrosSnackbarItem",
    },
    {
        .script_src = "chrome://resources/cros_components/tooltip/tooltip.js",
        .component_name = "cros-tooltip",
        .gtest_name = "CrosTooltip",
    },
    {
        .script_src =
            "chrome://resources/cros_components/accordion/accordion.js",
        .component_name = "cros-accordion",
        .gtest_name = "CrosAccordion",
    },
    {
        .script_src =
            "chrome://resources/cros_components/accordion/accordion_item.js",
        .component_name = "cros-accordion-item",
        .gtest_name = "CrosAccordionItem",
    },
    {
        .script_src =
            "chrome://resources/cros_components/icon_dropdown/icon-dropdown.js",
        .component_name = "cros-icon-dropdown",
        .gtest_name = "CrosIconDropdown",
    },
    {
        .script_src = "chrome://resources/cros_components/icon_dropdown/"
                      "icon-dropdown-option.js",
        .component_name = "cros-icon-dropdown-option",
        .gtest_name = "CrosIconDropdownOption",
    },
    // TODO(b:332970280): Bring orca-feedback back once we can support safeHTML
    // properly.
    // {
    //     .script_src =
    //         "chrome://resources/cros_components/orca_feedback/orca-feedback.js",
    //     .component_name = "mako-orca-feedback",
    //     .gtest_name = "CrosOrcaFeedbackItem",
    // },
};

INSTANTIATE_TEST_SUITE_P(All,
                         CrosComponentsBrowserTest,
                         testing::ValuesIn(kComponentsTestData),
                         testing::PrintToStringParamName());

}  // namespace ash
