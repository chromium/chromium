// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace web_app {
namespace {

constexpr char kAbortError[] = "AbortError";
constexpr char kDataError[] = "DataError";
constexpr char kSecurityError[] = "SecurityError";
constexpr char kValidManifestNoId[] = "/banners/manifest.json";

// Browser tests for the navigator.install({manifest: ...}) flow.
// These require a real renderer because manifest parsing uses the
// ManifestManager mojo interface via ParseManifestFromStringJob.
class WebInstallFromManifestBrowserTest : public WebAppBrowserTestBase {
 public:
  WebInstallFromManifestBrowserTest() = default;

  void SetUpOnMainThread() override {
    // Register a handler for dynamic test-specific responses (e.g., invalid
    // JSON). For recognized paths it returns a response; for everything else
    // it returns nullptr and the server falls through to serve static files.
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &WebInstallFromManifestBrowserTest::HandleDynamicRequest,
        base::Unretained(this)));
    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateToValidUrl() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_https_test_server().GetURL("/simple.html")));
  }

  // Calls navigator.install({manifest: manifest_url}) with a user gesture.
  bool TryInstallFromManifest(const GURL& manifest_url,
                              content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    const std::string script = content::JsReplace(
        "navigator.install({manifest: $1})"
        ".then(result => { webInstallResult = result; })"
        ".catch(error => { webInstallError = error; });",
        manifest_url.spec());
    return content::ExecJs(wc, script);
  }

  // Calls navigator.install({manifest: manifest_url, id: manifest_id}) with
  // a user gesture.
  bool TryInstallFromManifestWithId(const GURL& manifest_url,
                                    const GURL& manifest_id,
                                    content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    const std::string script = content::JsReplace(
        "navigator.install({manifest: $1, id: $2})"
        ".then(result => { webInstallResult = result; })"
        ".catch(error => { webInstallError = error; });",
        manifest_url.spec(), manifest_id.spec());
    return content::ExecJs(wc, script);
  }

  bool ResultExists() {
    return content::ExecJs(web_contents(), "webInstallResult");
  }

  bool ErrorExists(content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    return content::ExecJs(wc, "webInstallError");
  }

  std::string GetErrorName(content::WebContents* contents = nullptr) {
    content::WebContents* wc = contents ? contents : web_contents();
    return content::EvalJs(wc, "webInstallError.name").ExtractString();
  }

  // Sets a dynamic manifest response for /dynamic_manifest.json.
  void SetDynamicManifestResponse(const std::string& json) {
    dynamic_manifest_json_ = json;
  }

  // When the permission prompt shows, it must be granted or denied.
  void SetPermissionResponse(bool permission_granted,
                             content::WebContents* contents = nullptr) {
    permissions::PermissionRequestManager::AutoResponseType response =
        permission_granted
            ? permissions::PermissionRequestManager::AutoResponseType::
                  ACCEPT_ALL
            : permissions::PermissionRequestManager::AutoResponseType::DENY_ALL;

    permissions::PermissionRequestManager::FromWebContents(
        contents ? contents : web_contents())
        ->set_auto_response_for_test(response);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleDynamicRequest(
      const net::test_server::HttpRequest& request) {
    // A top-level document that disallows the "web-app-installation" feature
    // via its Permissions-Policy response header.
    if (request.relative_url == "/disallow_web_install.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->AddCustomHeader("Permissions-Policy",
                                "web-app-installation=()");
      response->set_content("<!doctype html><title>no install</title>");
      return response;
    }
    if (request.relative_url == "/dynamic_manifest.json") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(dynamic_manifest_json_);
      return response;
    }
    return nullptr;
  }

  std::string dynamic_manifest_json_;
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppInstallation};
};

// Valid manifest with custom id, no id option provided.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       ManifestOnly_Succeeds) {
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");

  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifest(manifest_url));
  observer.Wait();

  // The permission prompt must have been surfaced before granting.
  EXPECT_TRUE(observer.request_shown());

  // TODO(liahiscock): validation passes, but install is not yet implemented.
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

// Valid manifest with custom id, matching id option.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       ManifestAndId_Succeeds) {
  NavigateToValidUrl();
  SetPermissionResponse(/*permission_granted=*/true);
  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");
  // manifest_with_id.json has "id": "some_id", which resolves relative to
  // the manifest URL's origin.
  GURL manifest_id = embedded_https_test_server().GetURL("/some_id");

  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifestWithId(manifest_url, manifest_id));
  observer.Wait();

  // The permission prompt must have been surfaced before granting.
  EXPECT_TRUE(observer.request_shown());

  // TODO(liahiscock): validation passes, but install is not yet implemented.
  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

// When the user denies the Web Install permission prompt, the install is
// rejected with AbortError.
IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       PermissionDenied_AbortError) {
  NavigateToValidUrl();
  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");

  SetPermissionResponse(/*permission_granted=*/false);
  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifest(manifest_url));
  observer.Wait();

  // The permission prompt must have been surfaced before denial.
  EXPECT_TRUE(observer.request_shown());

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       PermissionsPolicyDisallowed_SecurityError) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("/disallow_web_install.html")));

  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");

  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(TryInstallFromManifest(manifest_url));

  // No prompt should ever be surfaced; the rejection is synchronous.
  EXPECT_FALSE(observer.request_shown());

  EXPECT_FALSE(ResultExists());
  ASSERT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kSecurityError);
}

///////////////////////////////////////////////////////////////////////////////
// Privacy invariant: DataErrors must surface identically in regular and
// Incognito profiles to prevent sites from using error differences to detect
// profile type. These cases are parameterized to run in both modes and assert
// identical behavior.
///////////////////////////////////////////////////////////////////////////////

enum class ProfileMode { kRegular, kIncognito };

class WebInstallFromManifestPrivacyInvariantTest
    : public WebInstallFromManifestBrowserTest,
      public testing::WithParamInterface<ProfileMode> {
 public:
  // Navigates the appropriate browser (the regular browser, or a freshly
  // created Incognito browser depending on the test parameter) to a valid page
  // and returns its WebContents to run the install in.
  content::WebContents* NavigateAndGetWebContents() {
    Browser* test_browser = GetParam() == ProfileMode::kIncognito
                                ? CreateIncognitoBrowser()
                                : browser();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        test_browser, embedded_https_test_server().GetURL("/simple.html")));
    return test_browser->tab_strip_model()->GetActiveWebContents();
  }
};

// A valid manifest without a custom id, and no id option provided, should
// return DataError identically in regular and Incognito modes.
IN_PROC_BROWSER_TEST_P(WebInstallFromManifestPrivacyInvariantTest,
                       MissingManifestId_DataError) {
  content::WebContents* wc = NavigateAndGetWebContents();
  GURL manifest_url = embedded_https_test_server().GetURL(kValidManifestNoId);

  ASSERT_TRUE(TryInstallFromManifest(manifest_url, wc));

  ASSERT_TRUE(ErrorExists(wc));
  EXPECT_EQ(GetErrorName(wc), kDataError);
}

// A valid manifest with a custom id, but a non-matching id option, should
// return DataError identically in regular and Incognito modes.
IN_PROC_BROWSER_TEST_P(WebInstallFromManifestPrivacyInvariantTest,
                       MismatchedManifestId_DataError) {
  content::WebContents* wc = NavigateAndGetWebContents();
  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");
  GURL manifest_id = embedded_https_test_server().GetURL("/wrong-id");

  ASSERT_TRUE(TryInstallFromManifestWithId(manifest_url, manifest_id, wc));

  ASSERT_TRUE(ErrorExists(wc));
  EXPECT_EQ(GetErrorName(wc), kDataError);
}

// Invalid JSON causes a parse failure that should return DataError identically
// in regular and Incognito modes.
IN_PROC_BROWSER_TEST_P(WebInstallFromManifestPrivacyInvariantTest,
                       InvalidJson_DataError) {
  SetDynamicManifestResponse("this is not valid json {{{");

  content::WebContents* wc = NavigateAndGetWebContents();
  GURL manifest_url =
      embedded_https_test_server().GetURL("/dynamic_manifest.json");

  ASSERT_TRUE(TryInstallFromManifest(manifest_url, wc));

  ASSERT_TRUE(ErrorExists(wc));
  EXPECT_EQ(GetErrorName(wc), kDataError);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebInstallFromManifestPrivacyInvariantTest,
                         testing::Values(ProfileMode::kRegular,
                                         ProfileMode::kIncognito),
                         [](const testing::TestParamInfo<ProfileMode>& info) {
                           return info.param == ProfileMode::kIncognito
                                      ? "Incognito"
                                      : "Regular";
                         });

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestBrowserTest,
                       Incognito_ValidManifest_ShowsNotSupportedDialog) {
  // A valid manifest in Incognito should show the "not supported" dialog
  // and reject with AbortError (not DataError).
  GURL test_url = embedded_https_test_server().GetURL("/simple.html");
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito_browser, test_url));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* incognito_web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");

  // Execute async so we can catch the dialog open. We can't wait for the
  // promise, otherwise the dialog would have closed.
  content::ExecuteScriptAsync(
      incognito_web_contents,
      content::JsReplace("navigator.install({manifest: $1})"
                         ".then(result => { webInstallResult = result; })"
                         ".catch(error => { webInstallError = error; });",
                         manifest_url.spec()));

  // Wait for the dialog to show.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  // Verify dialog title for Incognito mode.
  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installs aren't supported in Incognito mode");

  // Simulate the user accepting the dialog.
  views::test::AcceptDialog(widget);
  destroyed.Wait();

  // Validate JS results - should be AbortError, not DataError.
  ASSERT_TRUE(ErrorExists(incognito_web_contents));
  EXPECT_EQ(GetErrorName(incognito_web_contents), kAbortError);
}

///////////////////////////////////////////////////////////////////////////////
// Guest mode: installs should show the "not supported" dialog.
///////////////////////////////////////////////////////////////////////////////

class WebInstallFromManifestGuestModeTest
    : public WebInstallFromManifestBrowserTest {
 public:
  WebInstallFromManifestGuestModeTest() = default;
  WebInstallFromManifestGuestModeTest(
      const WebInstallFromManifestGuestModeTest&) = delete;
  WebInstallFromManifestGuestModeTest& operator=(
      const WebInstallFromManifestGuestModeTest&) = delete;

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestGuestModeTest,
                       GuestMode_NotSupportedDialog) {
#if BUILDFLAG(IS_CHROMEOS)
  Browser* guest_browser = browser();
#else
  Browser* guest_browser = CreateGuestBrowser();
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(guest_browser->GetProfile()->IsGuestSession());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      guest_browser, embedded_https_test_server().GetURL("/simple.html")));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");
  content::WebContents* guest_web_contents =
      guest_browser->tab_strip_model()->GetActiveWebContents();

  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");

  content::ExecuteScriptAsync(
      guest_web_contents,
      content::JsReplace("navigator.install({manifest: $1})"
                         ".then(result => { webInstallResult = result; })"
                         ".catch(error => { webInstallError = error; });",
                         manifest_url.spec()));

  // Wait for the "not supported" dialog.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installs aren't supported in Guest mode");

  views::test::AcceptDialog(widget);
  destroyed.Wait();

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists(guest_web_contents));
  EXPECT_EQ(GetErrorName(guest_web_contents), kAbortError);
}

///////////////////////////////////////////////////////////////////////////////
// Policy disabled: installs should show the "not supported" dialog.
///////////////////////////////////////////////////////////////////////////////

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class WebInstallFromManifestPolicyDisabledTest
    : public WebInstallFromManifestBrowserTest {
 public:
  WebInstallFromManifestPolicyDisabledTest() = default;
  WebInstallFromManifestPolicyDisabledTest(
      const WebInstallFromManifestPolicyDisabledTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    WebAppBrowserTestBase::SetUpInProcessBrowserTestFixture();

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    policy::PolicyMap policies;
    policies.Set(policy::key::kWebAppInstallByUserEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(WebInstallFromManifestPolicyDisabledTest,
                       PolicyDisabled_NotSupportedDialog) {
  ASSERT_FALSE(
      web_app::IsWebAppInstallByUserPolicyEnabled(browser()->GetProfile()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL("/simple.html")));

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppInstallNotSupportedDialog");

  GURL manifest_url =
      embedded_https_test_server().GetURL("/banners/manifest_with_id.json");

  content::ExecuteScriptAsync(
      web_contents(),
      content::JsReplace("navigator.install({manifest: $1})"
                         ".then(result => { webInstallResult = result; })"
                         ".catch(error => { webInstallError = error; });",
                         manifest_url.spec()));

  // Wait for the "not supported" dialog.
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  views::test::WidgetDestroyedWaiter destroyed(widget);

  EXPECT_EQ(
      widget->widget_delegate()->AsBubbleDialogDelegate()->GetWindowTitle(),
      u"Web app installation is blocked by administrator policy.");

  views::test::AcceptDialog(widget);
  destroyed.Wait();

  EXPECT_FALSE(ResultExists());
  EXPECT_TRUE(ErrorExists());
  EXPECT_EQ(GetErrorName(), kAbortError);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace
}  // namespace web_app
