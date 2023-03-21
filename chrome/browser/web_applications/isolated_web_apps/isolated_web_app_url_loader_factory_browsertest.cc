// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NotNull;

std::u16string MessagesAsString(
    const std::vector<content::WebContentsConsoleObserver::Message>& messages) {
  std::u16string text;
  for (const auto& message : messages) {
    text += message.message + u'\n';
  }
  return text;
}

std::unique_ptr<WebApp> CreateWebApp(const GURL& start_url) {
  AppId app_id = GenerateAppId(/*manifest_id=*/"", start_url);
  auto web_app = std::make_unique<WebApp>(app_id);
  web_app->SetStartUrl(start_url);
  web_app->SetName("Isolated Web App Example");
  web_app->SetScope(start_url.DeprecatedGetOriginAsURL());
  web_app->AddSource(WebAppManagement::Type::kCommandLine);
  return web_app;
}

std::unique_ptr<WebApp> CreateIsolatedWebApp(
    const GURL& start_url,
    WebApp::IsolationData isolation_data) {
  auto web_app = CreateWebApp(start_url);
  web_app->SetIsolationData(isolation_data);
  web_app->SetIsLocallyInstalled(true);
  return web_app;
}

class IsolatedWebAppURLLoaderFactoryBrowserTest
    : public WebAppControllerBrowserTest {
 public:
  explicit IsolatedWebAppURLLoaderFactoryBrowserTest(
      bool enable_isolated_web_apps_feature = true)
      : enable_isolated_web_apps_feature_(enable_isolated_web_apps_feature) {
    provider_creator_ =
        std::make_unique<FakeWebAppProviderCreator>(base::BindRepeating(
            &IsolatedWebAppURLLoaderFactoryBrowserTest::CreateWebAppProvider,
            base::Unretained(this)));
  }

 protected:
  void SetUp() override {
    if (enable_isolated_web_apps_feature_) {
      scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
    }

    WebAppControllerBrowserTest::SetUp();
  }

  void TearDown() override {
    SetTrustedWebBundleIdsForTesting({});
    WebAppControllerBrowserTest::TearDown();
  }

  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile) {
    auto provider = std::make_unique<FakeWebAppProvider>(profile);
    provider->SetDefaultFakeSubsystems();
    provider->Start();

    return provider;
  }

  FakeWebAppProvider* provider() {
    return static_cast<FakeWebAppProvider*>(
        WebAppProvider::GetForTest(browser()->profile()));
  }

  void RegisterWebApp(std::unique_ptr<WebApp> web_app) {
    provider()->GetRegistrarMutable().registry().emplace(web_app->app_id(),
                                                         std::move(web_app));
  }

  void TrustWebBundleId() {
    SetTrustedWebBundleIdsForTesting(
        {*web_package::SignedWebBundleId::Create(kTestEd25519WebBundleId)});
  }

  base::FilePath SignAndWriteBundleToDisk(
      const std::vector<uint8_t>& unsigned_bundle) {
    web_package::WebBundleSigner::KeyPair key_pair(kTestPublicKey,
                                                   kTestPrivateKey);
    auto signed_bundle =
        web_package::WebBundleSigner::SignBundle(unsigned_bundle, {key_pair});

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(temp_dir_.CreateUniqueTempDir());
      base::FilePath web_bundle_path;
      CHECK(CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path));

      CHECK(base::WriteFile(web_bundle_path, signed_bundle));
      return web_bundle_path;
    }
  }

  Browser* CreateAppWindow() {
    AppId app_id = GenerateAppId(/*manifest_id=*/"", kUrl);

    return Browser::Create(Browser::CreateParams::CreateForApp(
        GenerateApplicationNameFromAppId(app_id),
        /*trusted_source=*/true, gfx::Rect(), browser()->profile(),
        /*user_gesture=*/true));
  }

  content::WebContents* AttachWebContents(Browser* app_window) {
    app_window->tab_strip_model()->AppendWebContents(
        content::WebContents::Create(
            content::WebContents::CreateParams(app_window->profile())),
        /*foreground=*/true);

    return app_window->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateAndWaitForTitle(const GURL& url,
                               const std::u16string& page_title) {
    Browser* app_window = CreateAppWindow();
    content::TitleWatcher title_watcher(AttachWebContents(app_window),
                                        page_title);

    content::RenderFrameHost* render_frame_host = NavigateToURLWithDisposition(
        app_window, url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    ASSERT_THAT(render_frame_host, NotNull());

    EXPECT_THAT(title_watcher.WaitAndGetTitle(), Eq(page_title));
    EXPECT_THAT(render_frame_host->IsErrorDocument(), IsFalse());
  }

  void NavigateAndWaitForError(const GURL& url,
                               const std::string& error_messsage) {
    Browser* app_window = CreateAppWindow();

    content::WebContentsConsoleObserver console_observer(
        AttachWebContents(app_window));
    console_observer.SetFilter(base::BindRepeating(
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
        }));

    content::RenderFrameHost* render_frame_host = NavigateToURLWithDisposition(
        app_window, url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    ASSERT_THAT(render_frame_host, NotNull());

    ASSERT_TRUE(console_observer.Wait());
    EXPECT_THAT(render_frame_host->IsErrorDocument(), IsTrue());
    EXPECT_THAT(render_frame_host->GetLastCommittedURL(), Eq(url));
    EXPECT_THAT(console_observer.messages().size(), Eq(1ul))
        << MessagesAsString(console_observer.messages());
    EXPECT_THAT(console_observer.GetMessageAt(0), Eq(error_messsage));
  }

  const GURL kUrl = GURL(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    kTestEd25519WebBundleId}));

  bool enable_isolated_web_apps_feature_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<FakeWebAppProviderCreator> provider_creator_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest, LoadsBundle) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kUrl, {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Hello Isolated Apps</title>");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForTitle(kUrl, u"Hello Isolated Apps");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       LoadsSubResourcesFromBundle) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kUrl, {{":status", "200"}, {"content-type", "text/html"}},
                      "<script src=\"script.js\"></script>");
  builder.AddExchange(
      kUrl.Resolve("/script.js"),
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'title from js';");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForTitle(kUrl, u"title from js");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       CanFetchSubresources) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kUrl, {{":status", "200"}, {"content-type", "text/html"}},
                      R"(
    <script type="text/javascript" src="/script.js"></script>
)");
  builder.AddExchange(kUrl.Resolve("/script.js"),
                      {{":status", "200"}, {"content-type", "text/javascript"}},
                      R"(
fetch('title.txt')
  .then(res => res.text())
  .then(data => { console.log(data); document.title = data; })
  .catch(err => console.error(err));
)");
  builder.AddExchange(kUrl.Resolve("/title.txt"),
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "some data");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForTitle(kUrl, u"some data");
}

// Disabled due to flakiness. http://crbug.com/1381002
IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       DISABLED_InvalidStatusCode) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kUrl, {{":status", "201"}, {"content-type", "text/html"}},
                      "<title>Hello Isolated Apps</title>");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForError(
      kUrl,
      "Failed to read response from Signed Web Bundle: The response has an "
      "unsupported HTTP status code: 201 (only status code 200 is allowed).");
}

// Disabled due to flakiness. http://crbug.com/1381002
IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       DISABLED_NonExistingResource) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kUrl, {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Hello Isolated Apps</title>");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForError(
      kUrl.Resolve("/non-existing"),
      "Failed to read response from Signed Web Bundle: The Web Bundle does not "
      "contain a response for the provided URL: "
      "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/"
      "non-existing");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       UrlLoaderFactoryCanUseServiceWorker) {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kUrl, {{":status", "200"}, {"content-type", "text/html"}},
                      R"html(
<html>
  <head>
    <script type="text/javascript" src="/script.js"></script>
  </head>
</html>
)html");
  builder.AddExchange(kUrl.Resolve("/title.txt"),
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "data from web bundle");
  builder.AddExchange(kUrl.Resolve("/script.js"),
                      {{":status", "200"}, {"content-type", "text/javascript"}},
                      R"js(
const policy = trustedTypes.createPolicy('default', {
  createScriptURL(url) {
    return new URL(url, document.baseURI);
  },
});

const wait_for_activated = async (registration) => {
  const worker = registration.active;
  if (worker.state == 'activated') {
    return;
  }

  await new Promise(resolve => {
    worker.addEventListener('statechange', () => {
      if (worker.state = 'activated') {
        resolve();
      }
    });
  });
};

const register_service_worker = async () => {
  const registration = await navigator.serviceWorker.register(
    policy.createScriptURL('service_worker.js'), {
      scope: '/',
    }
  );

  await wait_for_activated(await navigator.serviceWorker.ready);

  return registration;
};

window.addEventListener('load', (async () => {
  const registration = await register_service_worker();
  const request = await fetch('title.txt');
  document.title = await request.text();
}));
)js");
  builder.AddExchange(kUrl.Resolve("/service_worker.js"),
                      {{":status", "200"}, {"content-type", "text/javascript"}},
                      R"js(
addEventListener('fetch', (event) => {
  event.respondWith((async () => {
    response = await fetch(event.request);
    text = await response.text();
    return new Response(text + ' data from service worker');
  })());
});

self.addEventListener('activate', (event) => {
  event.waitUntil(clients.claim());
});
)js");
  RegisterWebApp(CreateIsolatedWebApp(
      GURL(kUrl),
      WebApp::IsolationData{InstalledBundle{
          .path = SignAndWriteBundleToDisk(builder.CreateBundle())}}));
  TrustWebBundleId();

  NavigateAndWaitForTitle(GURL(kUrl),
                          u"data from web bundle data from service worker");
}

class IsolatedWebAppURLLoaderFactoryCSPBrowserTest
    : public IsolatedWebAppURLLoaderFactoryBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void AddIndexHtml(web_package::WebBundleBuilder& builder,
                    const std::string& csp) {
    bool use_meta_tag = GetParam();

    std::string html;
    web_package::WebBundleBuilder::Headers headers = {
        {":status", "200"}, {"content-type", "text/html"}};
    if (use_meta_tag) {
      html += base::ReplaceStringPlaceholders(R"(
        <head>
          <meta http-equiv="Content-Security-Policy" content="$1">
        </head>
      )",
                                              {csp}, nullptr);
    } else {
      headers.emplace_back("content-security-policy", csp);
    }

    html += R"(
      <script type="text/javascript" src="/script.js"></script>
    )";

    builder.AddExchange(kUrl, std::move(headers), std::move(html));
  }
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppURLLoaderFactoryCSPBrowserTest,
                       CanMakeCSPStricter) {
  web_package::WebBundleBuilder builder;
  // Make connect-src stricter than is required for IWAs. This should cause any
  // `fetch()` request to fail.
  AddIndexHtml(builder, "connect-src 'none'");
  builder.AddExchange(kUrl.Resolve("/script.js"),
                      {{":status", "200"}, {"content-type", "text/javascript"}},
                      R"(
    fetch('file.txt')
      .then(res => console.error(`Unexpectedly fetched file: ` + res.text()))
      .catch(err => {
        console.log(err);
        document.title = "unable to fetch";
      });
    )");
  builder.AddExchange(kUrl.Resolve("/file.txt"),
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "some data");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForTitle(kUrl, u"unable to fetch");
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppURLLoaderFactoryCSPBrowserTest,
                       CannotMakeCSPLessStrict) {
  web_package::WebBundleBuilder builder;
  // Attempt to allow JavaScript `eval()`. This should fail due to the CSP that
  // we apply by default.
  AddIndexHtml(builder, "script-src 'self' 'unsafe-eval'");
  builder.AddExchange(kUrl.Resolve("/script.js"),
                      {{":status", "200"}, {"content-type", "text/javascript"}},
                      R"(
    try {
      eval("1+1");
      console.error("Eval unexpectedly ran.");
    } catch (err) {
      console.log(err);
      document.title = "unable to eval";
    }
    )");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      kUrl, WebApp::IsolationData{InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));
  TrustWebBundleId();

  NavigateAndWaitForTitle(kUrl, u"unable to eval");
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppURLLoaderFactoryCSPBrowserTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<
        IsolatedWebAppURLLoaderFactoryCSPBrowserTest::ParamType>& info) {
      return info.param ? "meta_tag" : "header";
    });

}  // namespace
}  // namespace web_app
