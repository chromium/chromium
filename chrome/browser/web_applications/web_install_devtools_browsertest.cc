// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

namespace web_app {
namespace {

// Web Install DevTools Issues are supported only when the manifest URL is
// same-origin with the initiating document. This applies to both
// current-document installs and background-manifest installs; cross-origin
// failures retain their API result without exposing an additional Issue.

constexpr char kInstallElementId[] = "install-app";
constexpr char kInstallElementPage[] = "/web_apps/install_element/index.html";
constexpr char kCurrentDocumentMalformedPage[] =
    "/current-document-malformed.html";
constexpr char kCurrentDocumentElementMalformedPage[] =
    "/current-document-element-malformed.html";
constexpr char kCurrentDocumentElementNoManifestPage[] =
    "/current-document-element-no-manifest.html";
constexpr char kCurrentDocumentMissingIdPage[] =
    "/current-document-missing-id.html";
constexpr char kCurrentDocumentCrossOriginPage[] =
    "/current-document-cross-origin.html";
constexpr char kCurrentDocumentCrossOriginMissingIdPage[] =
    "/current-document-cross-origin-missing-id.html";
constexpr char kCurrentDocumentRedirectPage[] =
    "/current-document-redirect.html";
constexpr char kCurrentDocumentCrossOriginRedirectPage[] =
    "/current-document-cross-origin-redirect.html";
constexpr char kCurrentDocumentStartUrlInvalidPage[] =
    "/current-document-start-url-invalid.html";
constexpr char kCurrentDocumentMissingNamePage[] =
    "/current-document-missing-name.html";
constexpr char kMalformedManifestPath[] = "/malformed.webmanifest";
constexpr char kMissingIdManifestPath[] = "/missing-id.webmanifest";
constexpr char kCrossOriginMissingIdManifestPath[] =
    "/cross-origin-missing-id.webmanifest";
constexpr char kRedirectManifestPath[] = "/redirect.webmanifest";
constexpr char kCrossOriginRedirectManifestPath[] =
    "/cross-origin-redirect.webmanifest";
constexpr char kCrossOriginRedirectTargetManifestPath[] =
    "/cross-origin-redirect-target.webmanifest";
constexpr char kStartUrlInvalidManifestPath[] =
    "/start-url-invalid.webmanifest";
constexpr char kMissingNameManifestPath[] = "/missing-name.webmanifest";

bool IsWebInstallIssue(const base::DictValue& params) {
  const std::string* code = params.FindStringByDottedPath("issue.code");
  return code && *code == "WebInstallIssue";
}

class WebInstallDevToolsBrowserTest
    : public WebAppBrowserTestBase,
      public content::TestDevToolsProtocolClient {
 public:
  WebInstallDevToolsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppInstallation, blink::features::kInstallElement,
         blink::features::kBypassPepcSecurityForTesting},
        {});
  }

  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  content::WebContents* web_contents(
      BrowserWindowInterface* target_browser = nullptr) {
    target_browser = target_browser ? target_browser : browser();
    return target_browser->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateAndEnableAudits(
      const GURL& url,
      BrowserWindowInterface* target_browser = nullptr) {
    target_browser = target_browser ? target_browser : browser();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(target_browser, url));
    content::WebContents* contents = web_contents(target_browser);
    AttachToWebContents(contents);
    SendCommandSync("Audits.enable");
    ClearNotifications();
  }

  base::DictValue WaitForWebInstallIssue() {
    return WaitForMatchingNotification("Audits.issueAdded",
                                       base::BindRepeating(&IsWebInstallIssue));
  }

  bool HasWebInstallIssue() const {
    return HasExistingNotificationMatching(&IsWebInstallIssue);
  }

  void ExpectManifestIssue(const base::DictValue& params,
                           const std::optional<GURL>& manifest_url,
                           std::string_view reason) {
    EXPECT_EQ(*params.FindStringByDottedPath("issue.code"), "WebInstallIssue");
    const std::string* reported_url = params.FindStringByDottedPath(
        "issue.details.webInstallIssueDetails.manifestUrl");
    if (manifest_url) {
      ASSERT_TRUE(reported_url);
      EXPECT_EQ(*reported_url, manifest_url->spec());
    } else {
      EXPECT_FALSE(reported_url);
    }
    EXPECT_EQ(*params.FindStringByDottedPath(
                  "issue.details.webInstallIssueDetails.reason"),
              reason);
  }

  void TriggerNavigatorInstall(const GURL& manifest_url,
                               content::WebContents* contents = nullptr) {
    contents = contents ? contents : web_contents();
    ASSERT_TRUE(content::ExecJs(
        contents, content::JsReplace(
                      "navigator.install({manifest: $1})"
                      ".catch(error => { window.installError = error.name; });",
                      manifest_url.spec())));
    EXPECT_EQ(content::EvalJs(contents, "window.installError"), "DataError");
  }

  void TriggerElementInstall(const GURL& manifest_url) {
    ASSERT_TRUE(content::ExecJs(
        web_contents(),
        content::JsReplace(
            "new Promise(resolve => {"
            "  const element = document.getElementById($1);"
            "  element.setAttribute('manifest', $2);"
            "  element.addEventListener('installresult', event => {"
            "    window.installResult = event.result;"
            "    resolve();"
            "  }, {once: true});"
            "  element.click();"
            "})",
            kInstallElementId, manifest_url.spec())));
    EXPECT_EQ(content::EvalJs(web_contents(), "window.installResult"),
              "invalid_data");
  }

  void TriggerCurrentDocumentInstall(content::WebContents* contents = nullptr) {
    contents = contents ? contents : web_contents();
    ASSERT_TRUE(content::ExecJs(
        contents,
        "navigator.install()"
        ".catch(error => { window.installError = error.name; });"));
    EXPECT_EQ(content::EvalJs(contents, "window.installError"), "DataError");
  }

  void TriggerCurrentDocumentElementInstall() {
    ASSERT_TRUE(content::ExecJs(
        web_contents(),
        content::JsReplace(
            "new Promise(resolve => {"
            "  const element = document.getElementById($1);"
            "  element.addEventListener('installresult', event => {"
            "    window.installResult = event.result;"
            "    resolve();"
            "  }, {once: true});"
            "  element.click();"
            "})",
            kInstallElementId)));
    EXPECT_EQ(content::EvalJs(web_contents(), "window.installResult"),
              "invalid_data");
  }

  void AcceptCrossOriginPermission() {
    permissions::PermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(permissions::PermissionRequestManager::
                                         AutoResponseType::ACCEPT_ALL);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> ManifestPage(
      const std::string& manifest_url,
      bool include_install_element = false) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    const std::string manifest_link =
        manifest_url.empty() ? ""
                             : base::StrCat({"<link rel=\"manifest\" href=\"",
                                             manifest_url, "\">"});
    response->set_content(base::StrCat(
        {manifest_link, include_install_element
                            ? "<install id=\"install-app\"></install>"
                            : ""}));
    return response;
  }

  GURL TestUrl(const net::test_server::HttpRequest& request,
               std::string_view host,
               std::string_view path) {
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPathStr(path);
    return request.GetURL().ReplaceComponents(replacements);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kCurrentDocumentMalformedPage) {
      return ManifestPage(kMalformedManifestPath);
    }
    if (request.relative_url == kCurrentDocumentElementMalformedPage) {
      return ManifestPage(kMalformedManifestPath, true);
    }
    if (request.relative_url == kCurrentDocumentElementNoManifestPage) {
      return ManifestPage("", true);
    }
    if (request.relative_url == kCurrentDocumentMissingIdPage) {
      return ManifestPage(kMissingIdManifestPath);
    }
    if (request.relative_url == kCurrentDocumentCrossOriginPage) {
      return ManifestPage(
          TestUrl(request, "b.test", kMalformedManifestPath).spec());
    }
    if (request.relative_url == kCurrentDocumentCrossOriginMissingIdPage) {
      return ManifestPage(
          TestUrl(request, "b.test", kCrossOriginMissingIdManifestPath).spec());
    }
    if (request.relative_url == kCurrentDocumentRedirectPage) {
      return ManifestPage(kRedirectManifestPath);
    }
    if (request.relative_url == kCurrentDocumentCrossOriginRedirectPage) {
      return ManifestPage(kCrossOriginRedirectManifestPath);
    }
    if (request.relative_url == kCurrentDocumentStartUrlInvalidPage) {
      return ManifestPage(kStartUrlInvalidManifestPath);
    }
    if (request.relative_url == kCurrentDocumentMissingNamePage) {
      return ManifestPage(kMissingNameManifestPath);
    }
    if (request.relative_url == kRedirectManifestPath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader("Location", kMalformedManifestPath);
      return response;
    }
    if (request.relative_url == kCrossOriginRedirectManifestPath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location",
          TestUrl(request, "b.test", kCrossOriginRedirectTargetManifestPath)
              .spec());
      return response;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/manifest+json");
    if (request.relative_url == kMalformedManifestPath) {
      response->set_content("not valid JSON");
    } else if (request.relative_url == kCrossOriginRedirectTargetManifestPath) {
      response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      response->set_content("not valid JSON");
    } else if (request.relative_url == kMissingIdManifestPath) {
      response->set_content(
          R"({"name":"App","start_url":"/app","icons":[{"src":"/banners/launcher-icon-4x.png","sizes":"192x192","type":"image/png"}]})");
    } else if (request.relative_url == kCrossOriginMissingIdManifestPath) {
      // Keep the fallback manifest ID on `a.test` so validation reaches the
      // missing custom ID check. CORS allows `a.test` to parse this response
      // from `b.test`.
      response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      response->set_content(base::StrCat(
          {R"({"name":"App","start_url":")",
           TestUrl(request, "a.test", "/app").spec(),
           R"(","icons":[{"src":"/banners/launcher-icon-4x.png","sizes":"192x192","type":"image/png"}]})"}));
    } else if (request.relative_url == kStartUrlInvalidManifestPath) {
      response->set_content(R"({"name":"App","id":"/app"})");
    } else if (request.relative_url == kMissingNameManifestPath) {
      response->set_content(R"({"id":"/app","start_url":"/app"})");
    } else {
      return nullptr;
    }
    return response;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

void WebInstallDevToolsBrowserTest::SetUpOnMainThread() {
  embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
      &WebInstallDevToolsBrowserTest::HandleRequest, base::Unretained(this)));
  WebAppBrowserTestBase::SetUpOnMainThread();
}

void WebInstallDevToolsBrowserTest::TearDownOnMainThread() {
  DetachProtocolClient();
  WebAppBrowserTestBase::TearDownOnMainThread();
}

// Identifies how a test invokes Web Install. Current-document operations use
// the page's linked manifest; background-manifest operations pass a manifest
// URL explicitly.
enum class InstallOperation {
  kCurrentDocumentNavigator,
  kCurrentDocumentNavigatorAfterManifestRemoval,
  kCurrentDocumentElement,
  kBackgroundManifestNavigator,
  kBackgroundManifestElement,
};

// Maps an install operation and HTTP fixture to the expected public protocol
// reason. `manifest_path` is also the expected affected request; it is null
// only when the Issue has no manifest resource.
struct SameOriginIssueCase {
  const char* test_name;
  InstallOperation operation;
  const char* page_path;
  const char* manifest_path;
  const char* reason;
};

class WebInstallSameOriginDevToolsBrowserTest
    : public WebInstallDevToolsBrowserTest,
      public testing::WithParamInterface<SameOriginIssueCase> {};

IN_PROC_BROWSER_TEST_P(WebInstallSameOriginDevToolsBrowserTest, ReportsIssue) {
  const SameOriginIssueCase& test_case = GetParam();
  const GURL page_url =
      embedded_https_test_server().GetURL(test_case.page_path);
  NavigateAndEnableAudits(page_url);

  const std::optional<GURL> manifest_url =
      test_case.manifest_path
          ? std::make_optional(page_url.Resolve(test_case.manifest_path))
          : std::nullopt;
  switch (test_case.operation) {
    case InstallOperation::kCurrentDocumentNavigator:
      TriggerCurrentDocumentInstall();
      break;
    case InstallOperation::kCurrentDocumentNavigatorAfterManifestRemoval:
      ASSERT_TRUE(content::ExecJs(
          web_contents(),
          "document.querySelector('link[rel=manifest]').remove()"));
      // Wait for the browser to observe the empty manifest URL before invoking
      // Web Install.
      ASSERT_TRUE(base::test::RunUntil([&]() {
        const std::optional<GURL>& current_manifest_url =
            web_contents()->GetPrimaryMainFrame()->GetPage().GetManifestUrl();
        return current_manifest_url && current_manifest_url->is_empty();
      }));
      TriggerCurrentDocumentInstall();
      break;
    case InstallOperation::kCurrentDocumentElement:
      TriggerCurrentDocumentElementInstall();
      break;
    case InstallOperation::kBackgroundManifestNavigator:
      ASSERT_TRUE(manifest_url);
      TriggerNavigatorInstall(*manifest_url);
      break;
    case InstallOperation::kBackgroundManifestElement:
      ASSERT_TRUE(manifest_url);
      TriggerElementInstall(*manifest_url);
      break;
  }

  ExpectManifestIssue(WaitForWebInstallIssue(), manifest_url, test_case.reason);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebInstallSameOriginDevToolsBrowserTest,
    testing::Values(
        SameOriginIssueCase{"CurrentDocumentElementParsing",
                            InstallOperation::kCurrentDocumentElement,
                            kCurrentDocumentElementMalformedPage,
                            kMalformedManifestPath,
                            "ManifestParsingOrNetworkError"},
        SameOriginIssueCase{"CurrentDocumentNavigatorMissingId",
                            InstallOperation::kCurrentDocumentNavigator,
                            kCurrentDocumentMissingIdPage,
                            kMissingIdManifestPath, "ManifestMissingId"},
        SameOriginIssueCase{"CurrentDocumentElementNoManifest",
                            InstallOperation::kCurrentDocumentElement,
                            kCurrentDocumentElementNoManifestPage, nullptr,
                            "NoManifest"},
        SameOriginIssueCase{
            "CurrentDocumentNavigatorRemovedManifest",
            InstallOperation::kCurrentDocumentNavigatorAfterManifestRemoval,
            kCurrentDocumentMalformedPage, nullptr, "NoManifest"},
        SameOriginIssueCase{"CurrentDocumentNavigatorRedirect",
                            InstallOperation::kCurrentDocumentNavigator,
                            kCurrentDocumentRedirectPage, kRedirectManifestPath,
                            "ManifestParsingOrNetworkError"},
        SameOriginIssueCase{"CurrentDocumentNavigatorCrossOriginRedirect",
                            InstallOperation::kCurrentDocumentNavigator,
                            kCurrentDocumentCrossOriginRedirectPage,
                            kCrossOriginRedirectManifestPath,
                            "ManifestParsingOrNetworkError"},
        SameOriginIssueCase{"CurrentDocumentNavigatorStartUrl",
                            InstallOperation::kCurrentDocumentNavigator,
                            kCurrentDocumentStartUrlInvalidPage,
                            kStartUrlInvalidManifestPath, "StartUrlInvalid"},
        SameOriginIssueCase{"CurrentDocumentNavigatorMissingName",
                            InstallOperation::kCurrentDocumentNavigator,
                            kCurrentDocumentMissingNamePage,
                            kMissingNameManifestPath,
                            "ManifestMissingNameOrShortName"},
        SameOriginIssueCase{"BackgroundElementParsing",
                            InstallOperation::kBackgroundManifestElement,
                            kInstallElementPage, kMalformedManifestPath,
                            "ManifestParsingOrNetworkError"},
        SameOriginIssueCase{"BackgroundNavigatorMissingId",
                            InstallOperation::kBackgroundManifestNavigator,
                            "/simple.html", kMissingIdManifestPath,
                            "ManifestMissingId"}),
    [](const testing::TestParamInfo<SameOriginIssueCase>& info) {
      return info.param.test_name;
    });

enum class ProfileMode { kRegular, kIncognito };

// Holds the failure reason constant while covering the cross product of
// current-document/background-manifest installs and regular/Incognito modes.
struct PrivacyInvariantIssueCase {
  const char* test_name;
  ProfileMode profile_mode;
  bool current_document;
};

class WebInstallDevToolsPrivacyInvariantTest
    : public WebInstallDevToolsBrowserTest,
      public testing::WithParamInterface<PrivacyInvariantIssueCase> {
 protected:
  BrowserWindowInterface* test_browser() {
    return GetParam().profile_mode == ProfileMode::kIncognito
               ? CreateIncognitoBrowser()
               : browser();
  }
};

IN_PROC_BROWSER_TEST_P(WebInstallDevToolsPrivacyInvariantTest, ReportsIssue) {
  const PrivacyInvariantIssueCase& test_case = GetParam();
  BrowserWindowInterface* target_browser = test_browser();
  const GURL page_url = embedded_https_test_server().GetURL(
      test_case.current_document ? kCurrentDocumentMalformedPage
                                 : "/simple.html");
  const GURL manifest_url =
      embedded_https_test_server().GetURL(kMalformedManifestPath);
  NavigateAndEnableAudits(page_url, target_browser);
  content::WebContents* contents = web_contents(target_browser);

  if (test_case.current_document) {
    TriggerCurrentDocumentInstall(contents);
  } else {
    TriggerNavigatorInstall(manifest_url, contents);
  }

  ExpectManifestIssue(WaitForWebInstallIssue(), manifest_url,
                      "ManifestParsingOrNetworkError");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebInstallDevToolsPrivacyInvariantTest,
    testing::Values(PrivacyInvariantIssueCase{"CurrentDocumentRegular",
                                              ProfileMode::kRegular, true},
                    PrivacyInvariantIssueCase{"CurrentDocumentIncognito",
                                              ProfileMode::kIncognito, true},
                    PrivacyInvariantIssueCase{"BackgroundManifestRegular",
                                              ProfileMode::kRegular, false},
                    PrivacyInvariantIssueCase{"BackgroundManifestIncognito",
                                              ProfileMode::kIncognito, false}),
    [](const testing::TestParamInfo<PrivacyInvariantIssueCase>& info) {
      return info.param.test_name;
    });

// Covers cross-origin suppression for failures detected both while fetching or
// parsing a manifest and after a manifest has parsed without a custom ID.
struct CrossOriginIssueCase {
  const char* test_name;
  bool current_document;
  const char* page_path;
  const char* manifest_path;
};

class WebInstallCrossOriginDevToolsBrowserTest
    : public WebInstallDevToolsBrowserTest,
      public testing::WithParamInterface<CrossOriginIssueCase> {};

IN_PROC_BROWSER_TEST_P(WebInstallCrossOriginDevToolsBrowserTest,
                       DoesNotReportIssue) {
  const CrossOriginIssueCase& test_case = GetParam();
  const GURL page_url =
      embedded_https_test_server().GetURL("a.test", test_case.page_path);
  NavigateAndEnableAudits(page_url);

  // Cross-origin failures retain their API result but do not expose an
  // additional Web Install Issue.
  if (test_case.current_document) {
    TriggerCurrentDocumentInstall();
  } else {
    const GURL manifest_url =
        embedded_https_test_server().GetURL("b.test", test_case.manifest_path);
    AcceptCrossOriginPermission();
    TriggerNavigatorInstall(manifest_url);
  }

  EXPECT_FALSE(HasWebInstallIssue());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebInstallCrossOriginDevToolsBrowserTest,
    testing::Values(
        CrossOriginIssueCase{"CurrentDocumentParsing", true,
                             kCurrentDocumentCrossOriginPage, nullptr},
        CrossOriginIssueCase{"CurrentDocumentMissingId", true,
                             kCurrentDocumentCrossOriginMissingIdPage, nullptr},
        CrossOriginIssueCase{"BackgroundParsing", false, "/simple.html",
                             kMalformedManifestPath},
        CrossOriginIssueCase{"BackgroundMissingId", false, "/simple.html",
                             kMissingIdManifestPath}),
    [](const testing::TestParamInfo<CrossOriginIssueCase>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace web_app
