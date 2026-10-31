// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/tab_list/constants.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/search/ntp_features.h"
#endif

class ThirdPartyNTPBrowserTest : public PlatformBrowserTest,
                                 public InstantTestBase,
                                 public ::testing::WithParamInterface<bool> {
 public:
  ThirdPartyNTPBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (InstantUsesSpareRenderer()) {
      enabled_features.push_back(features::kInstantUsesSpareRenderer);
    } else {
      disabled_features.push_back(features::kInstantUsesSpareRenderer);
    }
    // The following flags are needed to enable the 3P WebUI NTP on Android.
#if BUILDFLAG(IS_ANDROID)
    enabled_features.push_back(ntp_features::kNtpEnableInstantApiAndroid);
    enabled_features.push_back(chrome::android::kUseWebUiNtpAndroid);
    enabled_features.push_back(chrome::android::kUseWebUiNtp3PDSE);
#endif
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ThirdPartyNTPBrowserTest(const ThirdPartyNTPBrowserTest&) = delete;
  ThirdPartyNTPBrowserTest& operator=(const ThirdPartyNTPBrowserTest&) = delete;

  bool InstantUsesSpareRenderer() const { return GetParam(); }

  void SetUpOnMainThread() override {
    https_test_server().RegisterRequestHandler(base::BindRepeating(
        &ThirdPartyNTPBrowserTest::HandleRedirectToChromeSearch,
        base::Unretained(this)));
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().SetCertHostnames({"example.com", "ntp.example.com"});
    ASSERT_TRUE(https_test_server().Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRedirectToChromeSearch(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/redirect-to-chrome-search") {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location",
                              "chrome-search://most-visited/title.html");
    response->set_content_type("text/html");
    response->set_content("<p>Redirecting to chrome-search</p>");
    return response;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(InstantUsesSpareRenderer,
                         ThirdPartyNTPBrowserTest,
                         testing::Bool());

// Verifies that a third party NTP can successfully embed the most visited
// iframe.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest, EmbeddedMostVisitedIframe) {
  GURL base_url =
      https_test_server().GetURL("ntp.example.com", "/instant_extended.html");
  GURL ntp_url = https_test_server().GetURL("ntp.example.com",
                                            "/instant_extended_ntp.html");
  SetupInstant(GetProfile(), base_url, ntp_url);

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, ntp_url));
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));

  // Add a chrome-search://most-visited/title.html?rid=1&fs=0 subframe and
  // verify that navigation completes successfully, with no kills.
  content::TestNavigationObserver nav_observer(contents);
  const char* kScript = R"(
      const frame = document.createElement('iframe');
      frame.src = 'chrome-search://most-visited/title.html?rid=1&fs=0';
      document.body.appendChild(frame);
  )";
  ASSERT_TRUE(content::ExecJs(contents, kScript));
  nav_observer.WaitForNavigationFinished();

  // Verify that the subframe exists and has the expected origin.
  content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
  ASSERT_TRUE(subframe);
  EXPECT_EQ("chrome-search://most-visited",
            content::EvalJs(subframe, "window.origin"));
}

// Verifies that a non-instant page cannot embed an iframe with the
// chrome-search scheme.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       NonInstantProcessCannotEmbedChromeSearch) {
  GURL non_instant_url =
      https_test_server().GetURL("example.com", "/title1.html");
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, non_instant_url));

  // Add a chrome-search://most-visited/title.html?rid=1&fs=0 subframe and
  // verify that the resource is not allowed to load.
  const char kChromeSearchIframeUrl[] =
      "chrome-search://most-visited/title.html?rid=1&fs=0";

  // The test script string is the same for both branches, so define it once.
  std::string script =
      base::StrCat({"const frame = document.createElement('iframe');"
                    "frame.src = '",
                    kChromeSearchIframeUrl,
                    "';"
                    "document.body.appendChild(frame);"});

  if (InstantUsesSpareRenderer()) {
    // When feature is enabled: browser-side blocking via NavigationThrottle.
    // Navigation is blocked and should not load the chrome-search URL.
    content::TestNavigationObserver nav_observer(contents);
    ASSERT_TRUE(content::ExecJs(contents, script));
    nav_observer.WaitForNavigationFinished();

    content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
    ASSERT_TRUE(subframe);
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_TRUE(subframe->IsErrorDocument());
  } else {
    // When feature is disabled: renderer-side blocking, console error fires
    content::WebContentsConsoleObserver console_observer(contents);
    console_observer.SetPattern(
        std::string("Not allowed to load local resource: ") +
        kChromeSearchIframeUrl);
    ASSERT_TRUE(content::ExecJs(contents, script));
    ASSERT_TRUE(console_observer.Wait());

    // Verify that the subframe does not load the chrome-search url.
    content::RenderFrameHost* subframe = ChildFrameAt(contents, 0);
    ASSERT_TRUE(subframe);
    EXPECT_EQ(GURL(), subframe->GetLastCommittedURL());
  }
}

// Verifies that a non-instant page cannot navigate to a chrome-search scheme
// via renderer-initiated navigation.
IN_PROC_BROWSER_TEST_P(
    ThirdPartyNTPBrowserTest,
    NonInstantProcessBlocksRendererInitiatedChromeSearchNavigation) {
  GURL non_instant_url =
      https_test_server().GetURL("example.com", "/title1.html");
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, non_instant_url));

  // Start a renderer initiated navigation to the chrome-search://
  // url and verify that the resource is not allowed to load.
  const char kChromeSearchUrl[] =
      "chrome-search://most-visited/title.html?rid=1&fs=0";

  if (InstantUsesSpareRenderer()) {
    // When feature is enabled: browser-side blocking via NavigationThrottle.
    // Navigation is blocked and should not load the chrome-search URL. For
    // this main-frame navigation we expect an error page to be shown.
    content::TestNavigationObserver nav_observer(contents);
    std::string script =
        base::StrCat({"window.location= '", kChromeSearchUrl, "';"});
    ASSERT_TRUE(content::ExecJs(contents, script));
    nav_observer.WaitForNavigationFinished();

    // Navigation should fail (be blocked).
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_TRUE(contents->GetPrimaryMainFrame()->IsErrorDocument());
  } else {
    // When feature is disabled: renderer-side blocking, console error fires
    content::WebContentsConsoleObserver console_observer(contents);
    console_observer.SetPattern(
        std::string("Not allowed to load local resource: ") + kChromeSearchUrl);
    std::string script =
        base::StrCat({"window.location= '", kChromeSearchUrl, "';"});
    ASSERT_TRUE(content::ExecJs(contents, script));
    ASSERT_TRUE(console_observer.Wait());
    // Verify that the main frame does not navigate to the chrome-search url.
    EXPECT_EQ(non_instant_url,
              contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  }
}

// Verifies that renderer-initiated navigation to a page whose response
// redirects to a chrome-search scheme cannot commit.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       ResponseRedirectToChromeSearchBlockedAtCommit) {
  GURL non_instant_url =
      https_test_server().GetURL("example.com", "/title1.html");
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, non_instant_url));

  GURL redirect_url =
      https_test_server().GetURL("example.com", "/redirect-to-chrome-search");

  content::TestNavigationObserver nav_observer(contents);
  ASSERT_TRUE(content::ExecJs(
      contents,
      base::StrCat({"window.location = '", redirect_url.spec(), "';"})));
  content::WaitForLoadStopWithoutSuccessCheck(contents);

  // On Android, `HandleExternalProtocol` returns false for main-frame
  // navigations, so the navigation fails with `ERR_UNKNOWN_URL_SCHEME` and
  // commits an error page. On Desktop, `HandleExternalProtocol` returns true to
  // handle external protocols asynchronously, which silently aborts the
  // navigation (`ERR_ABORTED`) and leaves `non_instant_url` committed.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME, nav_observer.last_net_error_code());
  EXPECT_TRUE(contents->GetPrimaryMainFrame()->IsErrorDocument());
#else
  EXPECT_EQ(non_instant_url,
            contents->GetPrimaryMainFrame()->GetLastCommittedURL());
#endif
}

// Verifies that an instant process can fetch chrome-search resources.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       InstantProcessCanFetchChromeSearchResource) {
  GURL base_url =
      https_test_server().GetURL("ntp.example.com", "/instant_extended.html");
  GURL ntp_url = https_test_server().GetURL("ntp.example.com",
                                            "/instant_extended_ntp.html");
  SetupInstant(GetProfile(), base_url, ntp_url);

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, ntp_url));
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));

  // Try to fetch a chrome-search resource using XMLHttpRequest and verify it
  // succeeds.
  const char* kScript = R"(
    (async () => {
      return new Promise((resolve) => {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', 'chrome-search://most-visited/title.html?rid=1&fs=0');
        xhr.onload = () => resolve('success');
        xhr.onerror = () => resolve('error: Failed to fetch');
        xhr.send();
      });
    })()
  )";
  std::string result = content::EvalJs(contents, kScript).ExtractString();
  EXPECT_EQ("success", result) << "Instant process should be able to fetch "
                                  "chrome-search resources, but got: "
                               << result;
}

// Verifies that a non-instant process cannot fetch chrome-search resources.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       NonInstantProcessCannotFetchChromeSearchResource) {
  GURL non_instant_url =
      https_test_server().GetURL("example.com", "/title1.html");
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, non_instant_url));

  // Verify that the process is not marked as an Instant process.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));

  // Try to fetch a chrome-search resource using XMLHttpRequest and verify it
  // fails.
  const char* kScript = R"(
    (async () => {
      return new Promise((resolve) => {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', 'chrome-search://most-visited/title.html?rid=1&fs=0');
        xhr.onload = () => resolve('success');
        xhr.onerror = () => resolve('error: Failed to fetch');
        xhr.send();
      });
    })()
  )";
  std::string result = content::EvalJs(contents, kScript).ExtractString();
  EXPECT_EQ("error: Failed to fetch", result)
      << "Non-instant process should not be able to fetch chrome-search "
         "resources, but got: "
      << result;
}

// Verifies that Chrome won't spawn a separate renderer process for
// every single NTP tab.  This behavior goes all the way back to
// the initial commit [1] which achieved that behavior by forcing
// process-per-site mode for NTP tabs.  It seems desirable to preserve this
// behavior going forward.
//
// [1]
// https://chromium.googlesource.com/chromium/src/+/09911bf300f1a419907a9412154760efd0b7abc3/chrome/browser/browsing_instance.cc#55
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest, ProcessPerSite) {
  GURL base_url =
      https_test_server().GetURL("ntp.example.com", "/instant_extended.html");
  GURL ntp_url = https_test_server().GetURL("ntp.example.com",
                                            "/instant_extended_ntp.html");
  SetupInstant(GetProfile(), base_url, ntp_url);

  // Open NTP in |tab1|.
  content::WebContents* tab1;
  {
    tabs::TabInterface* tab = GetTabListInterface()->OpenTab(
        chrome::ChromeUINewTabURLAsGURL(), tab_list::kNoTabIndex);
    ASSERT_TRUE(tab);
    tab1 = tab->GetContents();
    ASSERT_TRUE(WaitForLoadStop(tab1));

    // Sanity check: the NTP should be provided by |ntp_url| and not by
    // chrome://new-tab-page [1P WebUI NTP] or chrome://newtab [incognito].
    EXPECT_EQ(ntp_url, content::EvalJs(tab1, "window.location.href"));
  }

  // Open another NTP in |tab2|.
  content::WebContents* tab2;
  {
    tabs::TabInterface* tab = GetTabListInterface()->OpenTab(
        chrome::ChromeUINewTabURLAsGURL(), tab_list::kNoTabIndex);
    ASSERT_TRUE(tab);
    tab2 = tab->GetContents();
    ASSERT_TRUE(WaitForLoadStop(tab2));
    EXPECT_EQ(ntp_url, content::EvalJs(tab2, "window.location.href"));
  }

  // Verify that |tab1| and |tab2| share a process.
  EXPECT_EQ(tab1->GetPrimaryMainFrame()->GetProcess(),
            tab2->GetPrimaryMainFrame()->GetProcess());
}

// Verify that a third-party NTP commits in a remote NTP SiteInstance.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest, VerifySiteInstance) {
  // Setup and navigate to third-party NTP.
  GURL base_url =
      https_test_server().GetURL("ntp.example.com", "/instant_extended.html");
  GURL ntp_url = https_test_server().GetURL("ntp.example.com",
                                            "/instant_extended_ntp.html");
  SetupInstant(GetProfile(), base_url, ntp_url);
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, ntp_url));

  // Sanity check: the NTP should be provided by |ntp_url| and not by
  // chrome://new-tab-page [1P WebUI NTP] or chrome://newtab [incognito].
  EXPECT_EQ(ntp_url, content::EvalJs(web_contents, "window.location.href"));

  // Verify that NTP committed in remote NTP SiteInstance.
  EXPECT_EQ(GURL("chrome-search://remote-ntp/"),
            web_contents->GetPrimaryMainFrame()
                ->GetSiteInstance()
                ->GetSecurityPrincipal()
                .GetDeprecatedSiteURL());
}

// Verify that a third-party NTP can use the spare renderer when we enable
// the feature kInstantUsesSpareRenderer.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest, VerifyCanUseSpareProcess) {
  // Navigate to a non 3rd party ntp url.
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, https_test_server().GetURL("/title1.html")));
  content::RenderProcessHost* old_process =
      web_contents->GetPrimaryMainFrame()->GetProcess();

  // Navigate to a third-party NTP while a spare process is present.
  content::SpareRenderProcessHostManager::Get().WarmupSpare(GetProfile());
  content::RenderProcessHost* spare_process =
      content::SpareRenderProcessHostManager::Get().GetSpares().front();
  ASSERT_TRUE(spare_process);

  // Setup and navigate to a third-party NTP.
  GURL base_url =
      https_test_server().GetURL("ntp.example.com", "/instant_extended.html");
  GURL ntp_url = https_test_server().GetURL("ntp.example.com",
                                            "/instant_extended_ntp.html");
  SetupInstant(GetProfile(), base_url, ntp_url);
  ASSERT_TRUE(content::NavigateToURL(web_contents, ntp_url));

  content::RenderProcessHost* new_process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  // Verify that the resulting process is marked as an Instant process.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(
      instant_service->IsInstantProcess(new_process->GetDeprecatedID()));

  ASSERT_NE(new_process, old_process);
  // Verify that if kInstantUsesSpareRenderer flag is enabled,
  // third-party NTP used the spare process.
  EXPECT_EQ(InstantUsesSpareRenderer(), new_process == spare_process);
}

// Verifies that accessors planted on a same-origin subframe's Object.prototype
// or window.chrome before commit cannot run script synchronously inside
// SearchBoxExtension::Install() during DidClearWindowObject() and detach the
// frame while observers are still on the stack.
IN_PROC_BROWSER_TEST_P(ThirdPartyNTPBrowserTest,
                       SearchBoxExtensionInstallDoesNotRunScript) {
  GURL base_url =
      https_test_server().GetURL("ntp.example.com", "/instant_extended.html");
  GURL ntp_url = https_test_server().GetURL("ntp.example.com",
                                            "/instant_extended_ntp.html");
  SetupInstant(GetProfile(), base_url, ntp_url);
  content::WebContents* contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(content::NavigateToURL(contents, ntp_url));

  const char* kScript = R"(
    (async () => {
      // Case 1: Setter on child Object.prototype.searchBox.
      const iframe1 = document.createElement('iframe');
      iframe1.src = '/title1.html';
      const load1 = new Promise(resolve => {
        iframe1.addEventListener('load', () => resolve('loaded1'));
      });
      document.body.appendChild(iframe1);

      let protoSetterFired = false;
      Object.defineProperty(
          iframe1.contentWindow.Object.prototype, 'searchBox', {
            configurable: true,
            set() {
              protoSetterFired = true;
              iframe1.remove();
            }
          });

      const status1 = await load1;
      const hasSearchBox1 = Boolean(
          iframe1.contentWindow.chrome &&
          iframe1.contentWindow.chrome.embeddedSearch &&
          iframe1.contentWindow.chrome.embeddedSearch.searchBox);

      // Case 2: Accessor on child window.chrome.
      const iframe2 = document.createElement('iframe');
      iframe2.src = '/title1.html';
      const load2 = new Promise(resolve => {
        iframe2.addEventListener('load', () => resolve('loaded2'));
      });
      document.body.appendChild(iframe2);

      let chromeGetterFired = false;
      Object.defineProperty(iframe2.contentWindow, 'chrome', {
        configurable: true,
        get() {
          chromeGetterFired = true;
          iframe2.remove();
          return {};
        }
      });

      const status2 = await load2;
      return `${status1}:${protoSetterFired}:${hasSearchBox1}:` +
             `${status2}:${chromeGetterFired}`;
    })()
  )";
  EXPECT_EQ("loaded1:false:true:loaded2:false",
            content::EvalJs(contents, kScript));
}
