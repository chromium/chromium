// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/base/interaction/element_identifier.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSideSearchElementId);

constexpr char kSearchMatchPath[] = "/search-match";
constexpr char kNonMatchPath[] = "/non-match";

bool IsSearchURLMatch(const GURL& url) {
  // Test via path prefix match as the embedded test server ensures that all
  // URLs are using the same host and paths are made unique via appending a
  // monotonically increasing value to the end of their paths.
  return url.path().find(kSearchMatchPath) != std::string::npos;
}

}  // namespace

class SideSearchSideContentsHelperBrowsertest : public InteractiveBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kSideSearch}, {});
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(base::BindRepeating(
        &SideSearchSideContentsHelperBrowsertest::HandleRequest,
        base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();

    auto* config = SideSearchConfig::Get(browser()->profile());

    // TODO(crbug.com/40249898): this is a workaround for a side search bug that
    // can randomly close the side panel.
    config->set_skip_on_template_url_changed_for_testing(true);

    // Basic configuration for testing that allows navigations to URLs with
    // paths prefixed with `kSearchMatchPath` to proceed within the side panel,
    // and only allows showing the side panel on non-matching pages.
    config->SetShouldNavigateInSidePanelCallback(
        base::BindRepeating(IsSearchURLMatch));
    config->SetCanShowSidePanelForURLCallback(
        base::BindRepeating([](const GURL& url) { return true; }));
    config->SetGenerateSideSearchURLCallback(
        base::BindRepeating([](const GURL& url) { return url; }));

    // Create the side panel contents.
    GetTabContentsHelper()->GetSidePanelContents();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  // Navigates the active tab to `url`.
  void NavigateTab(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  // Navigates the active tab's side panel web contents.
  void NavigateInSideContents(const GURL& url) {
    auto* side_contents = GetSideContents();
    content::TestNavigationObserver nav_observer(side_contents);
    side_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));
    nav_observer.Wait();
    if (SideSearchConfig::Get(browser()->profile())
            ->ShouldNavigateInSidePanel(url)) {
      // If allowed to proceed in the side panel the side contents committed URL
      // should have been updated to reflect.
      EXPECT_EQ(url, side_contents->GetLastCommittedURL());
    } else {
      // If redirected to the tab contents ensure we observe the correct
      // committed URL in the tab.
      auto* tab_contents = browser()->tab_strip_model()->GetActiveWebContents();
      content::TestNavigationObserver tab_observer(tab_contents);
      tab_observer.Wait();
      EXPECT_EQ(url, tab_contents->GetLastCommittedURL());
    }
  }

  auto OpenAndInstrumentSideSearch(GURL url) {
    return Steps(
        Do(base::BindLambdaForTesting([this, url]() {
          GetTabContentsHelper()->OpenSidePanelFromContextMenuSearch(url);
        })),
        InstrumentNonTabWebView(kSideSearchElementId,
                                kSideSearchWebViewElementId));
  }

  // Navigates the active tab's side panel web contents.
  // Verb version for InteractiveBrowserTests.
  auto DoNavigateInSideContents(ui::ElementIdentifier current_tab_id,
                                const GURL& url) {
    return Steps(
        std::move(WithElement(kSideSearchElementId,
                              base::BindOnce(
                                  [](GURL url, ui::TrackedElement* el) {
                                    AsInstrumentedWebContents(el)->LoadPage(
                                        url);
                                  },
                                  url))
                      .SetMustRemainVisible(false)),
        WaitForWebContentsNavigation(SideSearchConfig::Get(browser()->profile())
                                             ->ShouldNavigateInSidePanel(url)
                                         ? kSideSearchElementId
                                         : current_tab_id,
                                     url));
  }

  GURL GetMatchingSearchUrl() {
    // Ensure that each returned matching URL is unique.
    static int id = 1;
    return embedded_test_server()->GetURL(
        base::StrCat({kSearchMatchPath, base::NumberToString(id++)}));
  }

  GURL GetNonMatchingUrl() {
    // Ensure that each returned non-matching URL is unique.
    static int id = 1;
    return embedded_test_server()->GetURL(
        base::StrCat({kNonMatchPath, base::NumberToString(id++)}));
  }

  //////////////////////////////////////////////////////////////////////////////
  // Getters

  // Gets the tab helper for the currently active tab.
  SideSearchTabContentsHelper* GetTabContentsHelper() {
    return SideSearchTabContentsHelper::FromWebContents(GetTabContents());
  }

  // Gets the WebContents for the currently active tab.
  content::WebContents* GetTabContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Gets the WebContents for the currently active tab's side panel.
  content::WebContents* GetSideContents() {
    return GetTabContentsHelper()->side_panel_contents_for_testing();
  }

 private:
  // Handles embedded test server requests to ensure we return successful OK
  // responses.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    return std::move(http_response);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Base class for Extensions API tests for the side panel WebContents.
class SideSearchExtensionsTest
    : public SideSearchSideContentsHelperBrowsertest {
 public:
  void SetUpOnMainThread() override {
    SideSearchSideContentsHelperBrowsertest::SetUpOnMainThread();
    // We want all navigations to be routed through the side panel for the
    // purposes of testing extension support.
    auto* config = SideSearchConfig::Get(browser()->profile());
    config->SetShouldNavigateInSidePanelCallback(
        base::BindRepeating([](const GURL& url) { return true; }));

    // Navigate to the first URL and open the side panel. This should create and
    // initiate a navigation in the side panel WebContents.
    NavigateTab(embedded_test_server()->GetURL("initial.example", "/"));

    // Wait for the side panel to finish loading the test URL.
    EXPECT_TRUE(content::WaitForLoadStop(GetSideContents()));
  }

  void NavigateInSideContents(const GURL& navigation_url,
                              const GURL& expected_url) {
    content::WebContents* side_contents = GetSideContents();

    content::TestNavigationObserver nav_observer(side_contents);
    side_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(navigation_url));
    nav_observer.Wait();

    EXPECT_EQ(expected_url, side_contents->GetLastCommittedURL());
  }
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// TODO(crbug.com/40230340): Test is flaky on Mac and Linux.
#define MAYBE_ContentScriptsExecuteInSidePanel \
  DISABLED_ContentScriptsExecuteInSidePanel
#else
#define MAYBE_ContentScriptsExecuteInSidePanel ContentScriptsExecuteInSidePanel
#endif
IN_PROC_BROWSER_TEST_F(SideSearchExtensionsTest,
                       MAYBE_ContentScriptsExecuteInSidePanel) {
  const GURL first_url = embedded_test_server()->GetURL("first.example", "/");
  const GURL second_url = embedded_test_server()->GetURL("second.example", "/");
  const GURL third_url = embedded_test_server()->GetURL("third.example", "/");
  constexpr char kManifest[] = R"(
      {
        "name": "Side Search Content Script Test",
        "manifest_version": 2,
        "version": "0.1",
        "content_scripts": [{
          "matches": ["*://*.second.example/*"],
          "js": ["script.js"],
          "run_at": "document_end"
        }]
      }
  )";
  constexpr char kContentScript[] =
      "document.body.innerText = 'content script has run';";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kContentScript);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::WebContents* side_contents = GetSideContents();

  // The extension should not run for the first URL.
  NavigateInSideContents(first_url, first_url);
  EXPECT_EQ("", content::EvalJs(side_contents, "document.body.innerText;"));

  // The extension should run for the second URL.
  NavigateInSideContents(second_url, second_url);
  EXPECT_EQ("content script has run",
            content::EvalJs(side_contents, "document.body.innerText;"));

  // The extension should not run for the third URL.
  NavigateInSideContents(third_url, third_url);
  EXPECT_EQ("", content::EvalJs(side_contents, "document.body.innerText;"));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// TODO(crbug.com/40230491): Test is flaky on Mac and Linux.
#define MAYBE_WebRequestInterceptsSidePanelNavigations \
  DISABLED_WebRequestInterceptsSidePanelNavigations
#else
#define MAYBE_WebRequestInterceptsSidePanelNavigations \
  WebRequestInterceptsSidePanelNavigations
#endif
IN_PROC_BROWSER_TEST_F(SideSearchExtensionsTest,
                       MAYBE_WebRequestInterceptsSidePanelNavigations) {
  const GURL first_url = embedded_test_server()->GetURL("first.example", "/");
  const GURL second_url = embedded_test_server()->GetURL("second.example", "/");
  const GURL third_url = embedded_test_server()->GetURL("third.example", "/");
  const GURL redirect_url =
      embedded_test_server()->GetURL("example.redirect", "/");
  constexpr char kManifest[] = R"(
      {
        "name": "WebRequest Test Extension",
        "version": "0.1",
        "manifest_version": 2,
        "background": {
          "scripts": ["background.js"]
        },
        "permissions": [
          "webRequest",
          "webRequestBlocking",
          "*://first.example/*",
          "*://second.example/*"
        ]
      }
  )";
  constexpr char kRulesScriptTemplate[] = R"(
      chrome.webRequest.onBeforeRequest.addListener(function(d) {
          return {redirectUrl: $1};
        }, {urls: ["*://*.second.example/*"]}, ["blocking"]);
  )";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      content::JsReplace(kRulesScriptTemplate, redirect_url));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigation to the first URL should be initiated in the side panel as
  // expected.
  NavigateInSideContents(first_url, first_url);

  // Navigation to the second URL should be redirected by the webRequest API.
  NavigateInSideContents(second_url, redirect_url);

  // Navigation to the third URL should proceed as expected.
  NavigateInSideContents(third_url, third_url);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// TODO(crbug.com/40827415): Test is flaky on Mac, Windows and Linux bots.
#define MAYBE_DeclarativeNetRequestInterceptsSidePanelNavigations \
  DISABLED_DeclarativeNetRequestInterceptsSidePanelNavigations
#else
#define MAYBE_DeclarativeNetRequestInterceptsSidePanelNavigations \
  DeclarativeNetRequestInterceptsSidePanelNavigations
#endif
IN_PROC_BROWSER_TEST_F(
    SideSearchExtensionsTest,
    MAYBE_DeclarativeNetRequestInterceptsSidePanelNavigations) {
  const GURL first_url = embedded_test_server()->GetURL("first.example", "/");
  const GURL second_url = embedded_test_server()->GetURL("second.example", "/");
  const GURL third_url = embedded_test_server()->GetURL("third.example", "/");
  const GURL redirect_url =
      embedded_test_server()->GetURL("example.redirect", "/");
  constexpr char kManifest[] = R"(
      {
        "name": "WebRequest Test Extension",
        "version": "0.1",
        "manifest_version": 2,
        "declarative_net_request": {
          "rule_resources": [{
            "id": "ruleset_1",
            "enabled": true,
            "path": "rules.json"
          }]
        },
        "permissions": [
          "declarativeNetRequest",
          "*://first.example/*",
          "*://second.example/*"
        ]
      }
  )";
  constexpr char kRulesJsonTemplate[] = R"(
    [{
      "id": 1,
      "priority": 1,
      "action": {
        "type": "redirect",
        "redirect": { "url": $1 } },
      "condition": {
        "urlFilter": "*second.example*",
        "resourceTypes": ["main_frame"]
      }
    }]
  )";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("rules.json"),
                          content::JsReplace(kRulesJsonTemplate, redirect_url));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigation to the first URL should proceed as expected.
  NavigateInSideContents(first_url, first_url);

  // Navigation to the secind URL should be redirected by the netRequest API.
  NavigateInSideContents(second_url, redirect_url);

  // Navigation to the third URL should proceed as expected.
  NavigateInSideContents(third_url, third_url);
}
