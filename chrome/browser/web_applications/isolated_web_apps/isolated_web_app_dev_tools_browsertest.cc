// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/devtools/devtools_availability_checker.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Eq;
using ::testing::StartsWith;

namespace web_app {

namespace {
constexpr std::string_view kTypeApp = "app";
constexpr std::string_view kIsolatedAppName = "Simple Isolated App";
constexpr std::string_view kIsolatedAppVersion = "1.0.0";
constexpr std::string_view kIsolatedAppDevToolsTitle =
    "Simple Isolated App (1.0.0)";
}  // namespace
class IsolatedWebAppDevToolsTest : public base::test::WithFeatureOverride,
                                   public IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppDevToolsTest()
      : base::test::WithFeatureOverride(::features::kWebAppInstallDialog) {}

  void TearDownOnMainThread() override {
    if (devtools_client_) {
      devtools_client_->DetachProtocolClient();
      devtools_client_.reset();
    }
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  void AttachDevTools(content::WebContents* web_contents) {
    if (devtools_client_) {
      devtools_client_->DetachProtocolClient();
    }
    devtools_client_ = std::make_unique<content::TestDevToolsProtocolClient>();
    devtools_client_->AttachToWebContents(web_contents);
  }

  content::TestDevToolsProtocolClient* devtools_client() {
    return devtools_client_.get();
  }

 protected:
  IsolatedWebAppUrlInfo InstallIsolatedWebApp() {
    auto app =
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                           .SetName(kIsolatedAppName)
                                           .SetVersion(kIsolatedAppVersion))
            .BuildBundle();
    return app->InstallChecked(profile());
  }

 private:
  std::unique_ptr<content::TestDevToolsProtocolClient> devtools_client_;
};

// TODO (crbug.com/41495909): Resolve flakiness on linux debug builds.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_ErrorPage DISABLED_ErrorPage
#else
#define MAYBE_ErrorPage ErrorPage
#endif
IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTest, MAYBE_ErrorPage) {
  std::unique_ptr<net::EmbeddedTestServer> server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  IsolatedWebAppUrlInfo url_info =
      InstallDevModeProxyIsolatedWebApp(server->GetOrigin());
  BrowserWindowInterface* browser =
      LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  DevToolsWindowTesting::OpenDevToolsWindowSync(web_contents,
                                                /*is_docked=*/true);

  content::TestNavigationObserver navigation_observer(web_contents);
  EXPECT_TRUE(server->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(ExecJs(web_contents, "location.reload()"));
  navigation_observer.Wait();

  GURL expected_url = url_info.origin().GetURL().Resolve("/index.html");
  EXPECT_THAT(navigation_observer.last_navigation_url(), Eq(expected_url));
  EXPECT_THAT(navigation_observer.last_net_error_code(),
              Eq(net::ERR_CONNECTION_REFUSED));
  content::RenderFrameHost* error_frame = web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(error_frame->GetLastCommittedOrigin().opaque());
  EXPECT_THAT(error_frame->GetStoragePartition(),
              Eq(profile()->GetDefaultStoragePartition()));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTest, IwaIdentifiedAsApp) {
  // 1) Install an Isolated Web App and check its type in DevTools
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  BrowserWindowInterface* iwa_app =
      LaunchWebAppBrowserAndWait(url_info.app_id());
  scoped_refptr<content::DevToolsAgentHost> iwa_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          iwa_app->tab_strip_model()->GetActiveWebContents());
  const std::string& iwa_type = iwa_host->GetType();
  EXPECT_EQ(kTypeApp, iwa_type);

  // 2) Navigate to the Chrome Inspect page and check its js content
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(1, content::EvalJs(
                   web_contents,
                   "document.getElementById('apps-list').children.length"));
  EXPECT_EQ(kIsolatedAppDevToolsTitle,
            content::EvalJs(web_contents,
                            "document.getElementById('apps-list').children[0]."
                            "getElementsByClassName('name')[0].innerText"));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTest, IwaWithCorrectTitle) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  BrowserWindowInterface* iwa_app =
      LaunchWebAppBrowserAndWait(url_info.app_id());
  scoped_refptr<content::DevToolsAgentHost> iwa_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          iwa_app->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(iwa_host->GetType(), kTypeApp);
  EXPECT_EQ(iwa_host->GetTitle(), kIsolatedAppDevToolsTitle);
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTest, PwaIdentifiedAsPage) {
  // 1) Regression test to install PWA and make sure they still show as page
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  webapps::AppId pwa_id = web_app::test::InstallPwaForCurrentUrl(browser());
  BrowserWindowInterface* pwa_app =
      web_app::LaunchWebAppBrowserAndWait(browser()->GetProfile(), pwa_id);
  scoped_refptr<content::DevToolsAgentHost> pwa_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          pwa_app->tab_strip_model()->GetActiveWebContents());
  const std::string& pwa_type = pwa_host->GetType();
  EXPECT_EQ(content::DevToolsAgentHost::kTypePage, pwa_type);

  // 2) Navigate to the Chrome Inspect page and check its js content
  //    App list should be empty since PWA is identified as a page
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIInspectURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(0, content::EvalJs(
                   web_contents,
                   "document.getElementById('apps-list').children.length"));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTest, IwaManifestWithoutRelLink) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  BrowserWindowInterface* iwa_app =
      LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      iwa_app->tab_strip_model()->GetActiveWebContents();

  AttachDevTools(web_contents);

  // Page.getAppManifest returns the authoritative IWA manifest URL and
  // parsed manifest.
  const base::DictValue* manifest_result =
      devtools_client()->SendCommandSync("Page.getAppManifest");
  ASSERT_TRUE(manifest_result);
  EXPECT_EQ(
      *manifest_result->FindString("url"),
      url_info.origin().GetURL().Resolve(".well-known/manifest.webmanifest"));
  const base::DictValue* manifest = manifest_result->FindDict("manifest");
  ASSERT_TRUE(manifest);
  EXPECT_EQ(*manifest->FindString("name"), kIsolatedAppName);
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDevToolsTest, SubAppManifestResolution) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(
          ManifestBuilder()
              .SetName("Parent App")
              .SetVersion("1.0.0")
              .AddPermissionsPolicy(
                  network::mojom::PermissionsPolicyFeature::kSubApps,
                  /*self=*/true,
                  /*origins=*/{}))
          .AddHtml("/",
                   "<!DOCTYPE html><html><head><title>Parent</title></"
                   "head><body>Parent App</body></html>")
          .AddHtml("/sub1/page.html",
                   "<!DOCTYPE html><html><head><title>Sub1</title></"
                   "head><body>Sub1 Main Page</body></html>")
          .AddHtml("/sub1/second_page.html",
                   "<!DOCTYPE html><html><head><title>Sub1 Page 2</title></"
                   "head><body>Sub1 Page 2</body></html>")
          .AddResource("/sub1/manifest.webmanifest",
                       R"({
                            "name": "Sub App 1",
                            "start_url": "/sub1/page.html",
                            "scope": "/sub1/"
                          })",
                       "application/manifest+json")
          .BuildBundle();
  app->TrustSigningKey();
  IsolatedWebAppUrlInfo parent_url_info = app->InstallChecked(profile());

  // Install the Sub-App.
  GURL sub_app_start_url =
      parent_url_info.origin().GetURL().Resolve("/sub1/page.html");
  GURL sub_app_scope = parent_url_info.origin().GetURL().Resolve("/sub1/");
  GURL sub_app_manifest_url =
      parent_url_info.origin().GetURL().Resolve("/sub1/manifest.webmanifest");

  auto sub_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(sub_app_start_url);
  sub_app_info->scope = sub_app_scope;
  sub_app_info->title = u"Sub App 1";
  sub_app_info->manifest_url = sub_app_manifest_url;
  sub_app_info->parent_app_id = parent_url_info.app_id();
  sub_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId sub_app_id =
      test::InstallWebApp(profile(), std::move(sub_app_info),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::SUB_APP);

  // Sub-App main page (/sub1/page.html).
  BrowserWindowInterface* sub_app_browser =
      LaunchWebAppBrowserAndWait(sub_app_id);
  content::WebContents* sub_app_contents =
      sub_app_browser->tab_strip_model()->GetActiveWebContents();

  AttachDevTools(sub_app_contents);

  const base::DictValue* sub_manifest_result =
      devtools_client()->SendCommandSync("Page.getAppManifest");
  ASSERT_TRUE(sub_manifest_result);
  EXPECT_EQ(*sub_manifest_result->FindString("url"),
            sub_app_manifest_url.spec());
  const base::DictValue* sub_manifest =
      sub_manifest_result->FindDict("manifest");
  ASSERT_TRUE(sub_manifest);
  EXPECT_EQ(*sub_manifest->FindString("name"), "Sub App 1");

  // Sub-App second page (/sub1/second_page.html).
  GURL second_page_url =
      parent_url_info.origin().GetURL().Resolve("/sub1/second_page.html");
  ASSERT_TRUE(content::NavigateToURL(sub_app_contents, second_page_url));

  const base::DictValue* page2_manifest_result =
      devtools_client()->SendCommandSync("Page.getAppManifest");
  ASSERT_TRUE(page2_manifest_result);
  EXPECT_EQ(*page2_manifest_result->FindString("url"),
            sub_app_manifest_url.spec());
  const base::DictValue* page2_manifest =
      page2_manifest_result->FindDict("manifest");
  ASSERT_TRUE(page2_manifest);
  EXPECT_EQ(*page2_manifest->FindString("name"), "Sub App 1");
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(IsolatedWebAppDevToolsTest);

namespace {

constexpr char kWorkerIndexHtml[] = R"(
<!doctype html>
<script src="/register.js"></script>
)";

constexpr char kWorkerRegisterScript[] = R"(
const workerPolicy = trustedTypes.createPolicy('iwa-worker-test', {
  createScriptURL: value => value,
});
window.workerReady = navigator.serviceWorker.register(
        workerPolicy.createScriptURL('/sw.js'))
    .then(() => navigator.serviceWorker.ready)
    .then(() => true);
)";

constexpr char kServiceWorkerScript[] = R"(
self.IWA_POLICY_SECRET = 'iwa-worker-secret';
self.addEventListener('install', event => event.waitUntil(self.skipWaiting()));
self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
)";

class DirectWorkerProtocolClient final
    : public content::TestDevToolsProtocolClient {
 public:
  bool AttachTo(scoped_refptr<content::DevToolsAgentHost> host) {
    if (!host->AttachClient(this)) {
      return false;
    }
    agent_host_ = std::move(host);
    return true;
  }
};

}  // namespace

class DevToolsIwaWorkerPolicyBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    const auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
    data_provider_->Update(
        [&](auto& update) { update.AddToManagedAllowlist(web_bundle_id); });

    update_server_.AddBundle(
        IsolatedWebAppBuilder(
            ManifestBuilder().SetName("Policy IWA worker").SetVersion("1.0.0"))
            .AddHtml("/", kWorkerIndexHtml)
            .AddJs("/register.js", kWorkerRegisterScript)
            .AddJs("/sw.js", kServiceWorkerScript)
            .BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair()}));
  }

  IsolatedWebAppUrlInfo InstallPolicyIwa() {
    const auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);

    WebAppTestInstallObserver observer(profile());
    observer.BeginListening({url_info.app_id()});
    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        update_server_.CreateForceInstallPolicyEntry(web_bundle_id));
    EXPECT_EQ(url_info.app_id(), observer.Wait());
    return url_info;
  }

 private:
  IsolatedWebAppTestUpdateServer update_server_;
  FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DevToolsIwaWorkerPolicyBrowserTest,
                       AttachPolicyIwaServiceWorkerDisallowed) {
  IsolatedWebAppUrlInfo url_info = InstallPolicyIwa();
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_TRUE(web_app);
  ASSERT_TRUE(web_app->IsIwaPolicyInstalledApp());

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(app_frame);
  ASSERT_EQ(true, content::EvalJs(app_frame, "window.workerReady"));

  profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::
                           kDisallowedForForceInstalledExtensions));

  // The app's page target is rejected by the intended WebApp policy branch.
  EXPECT_FALSE(IsInspectionAllowed(profile(), web_app));

  const GURL worker_url = url_info.origin().GetURL().Resolve("/sw.js");
  scoped_refptr<content::DevToolsAgentHost> worker_target;
  ASSERT_TRUE(base::test::RunUntil([&] {
    for (auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
      if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
          host->GetURL() == worker_url) {
        worker_target = host;
        return true;
      }
    }
    return false;
  }));

  ASSERT_TRUE(worker_target);
  ASSERT_EQ(nullptr, worker_target->GetWebContents());
  ASSERT_EQ(profile(), worker_target->GetBrowserContext());

  // Policy decision: WebApp lookup occurs for URL-only targets.
  EXPECT_FALSE(IsInspectionAllowed(profile(), worker_target.get()));

  // Exercise AllowInspectingTarget gate and verify attaching to the managed
  // IWA worker target is rejected.
  DirectWorkerProtocolClient client;
  EXPECT_FALSE(client.AttachTo(worker_target));
}

IN_PROC_BROWSER_TEST_F(
    DevToolsIwaWorkerPolicyBrowserTest,
    AttachAndEvaluatePolicyIwaServiceWorkerWhenDevToolsAllowed) {
  IsolatedWebAppUrlInfo url_info = InstallPolicyIwa();
  const WebApp* web_app =
      provider().registrar_unsafe().GetAppById(url_info.app_id());
  ASSERT_TRUE(web_app);
  ASSERT_TRUE(web_app->IsIwaPolicyInstalledApp());

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_TRUE(app_frame);
  ASSERT_EQ(true, content::EvalJs(app_frame, "window.workerReady"));

  profile()->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kAllowed));

  EXPECT_TRUE(IsInspectionAllowed(profile(), web_app));

  const GURL worker_url = url_info.origin().GetURL().Resolve("/sw.js");
  scoped_refptr<content::DevToolsAgentHost> worker_target;
  ASSERT_TRUE(base::test::RunUntil([&] {
    for (auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
      if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
          host->GetURL() == worker_url) {
        worker_target = host;
        return true;
      }
    }
    return false;
  }));

  ASSERT_TRUE(worker_target);
  ASSERT_EQ(nullptr, worker_target->GetWebContents());
  ASSERT_EQ(profile(), worker_target->GetBrowserContext());

  EXPECT_TRUE(IsInspectionAllowed(profile(), worker_target.get()));

  DirectWorkerProtocolClient client;
  ASSERT_TRUE(client.AttachTo(worker_target));
  base::DictValue params;
  params.Set("expression", "self.IWA_POLICY_SECRET");
  params.Set("returnByValue", true);
  const base::DictValue* result =
      client.SendCommandSync("Runtime.evaluate", std::move(params));
  ASSERT_TRUE(result);
  const std::string* value = result->FindStringByDottedPath("result.value");
  ASSERT_TRUE(value);
  EXPECT_EQ("iwa-worker-secret", *value);
  client.DetachProtocolClient();
}

}  // namespace web_app
