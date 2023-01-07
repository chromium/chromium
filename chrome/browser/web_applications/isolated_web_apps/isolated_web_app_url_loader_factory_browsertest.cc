// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

std::unique_ptr<WebApp> CreateIsolatedWebApp(const GURL& start_url,
                                             IsolationData isolation_data) {
  auto web_app = CreateWebApp(start_url);
  web_app->SetIsolationData(isolation_data);
  web_app->SetIsLocallyInstalled(true);
  return web_app;
}

}  // namespace

class IsolatedWebAppURLLoaderFactoryBrowserTest : public InProcessBrowserTest {
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

    InProcessBrowserTest::SetUp();
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

  base::FilePath SignAndWriteBundleToDisk(
      const std::vector<uint8_t>& unsigned_bundle) {
    auto signed_bundle = web_package::WebBundleSigner::SignBundle(
        unsigned_bundle,
        {web_package::WebBundleSigner::KeyPair::CreateRandom()});

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_THAT(temp_dir_.CreateUniqueTempDir(), IsTrue());
      base::FilePath web_bundle_path;
      EXPECT_THAT(
          CreateTemporaryFileInDir(temp_dir_.GetPath(), &web_bundle_path),
          IsTrue());
      EXPECT_THAT(
          static_cast<size_t>(base::WriteFile(
              web_bundle_path, reinterpret_cast<char*>(signed_bundle.data()),
              signed_bundle.size())),
          Eq(signed_bundle.size()));

      return web_bundle_path;
    }
  }

  void NavigateAndWaitForTitle(const GURL& url,
                               const std::u16string& page_title) {
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), page_title);

    content::RenderFrameHost* render_frame_host =
        ui_test_utils::NavigateToURL(browser(), GURL(kPrimaryUrl));
    ASSERT_THAT(render_frame_host, NotNull());

    EXPECT_THAT(title_watcher.WaitAndGetTitle(), Eq(page_title));
    EXPECT_THAT(render_frame_host->IsErrorDocument(), IsFalse());
  }

  void NavigateAndWaitForError(const GURL& url,
                               const std::string& error_messsage) {
    content::WebContentsConsoleObserver console_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    console_observer.SetFilter(base::BindRepeating(
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.log_level == blink::mojom::ConsoleMessageLevel::kError;
        }));

    content::RenderFrameHost* render_frame_host =
        ui_test_utils::NavigateToURL(browser(), url);
    ASSERT_THAT(render_frame_host, NotNull());

    console_observer.Wait();
    EXPECT_THAT(render_frame_host->IsErrorDocument(), IsTrue());
    EXPECT_THAT(render_frame_host->GetLastCommittedURL(), Eq(url));
    EXPECT_THAT(console_observer.messages().size(), Eq(1ul))
        << MessagesAsString(console_observer.messages());
    EXPECT_THAT(console_observer.GetMessageAt(0), Eq(error_messsage));
  }

  const std::string kWebBundleId =
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";
  const std::string kPrimaryUrl = "isolated-app://" + kWebBundleId;

  bool enable_isolated_web_apps_feature_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<FakeWebAppProviderCreator> provider_creator_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest, LoadsBundle) {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Hello Isolated Apps</title>");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      GURL(kPrimaryUrl),
      IsolationData{IsolationData::InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));

  NavigateAndWaitForTitle(GURL(kPrimaryUrl), u"Hello Isolated Apps");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       LoadsSubResourcesFromBundle) {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<script src=\"script.js\"></script>");
  builder.AddExchange(
      kPrimaryUrl + "/script.js",
      {{":status", "200"}, {"content-type", "application/javascript"}},
      "document.title = 'title from js';");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      GURL(kPrimaryUrl),
      IsolationData{IsolationData::InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));

  NavigateAndWaitForTitle(GURL(kPrimaryUrl), u"title from js");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       CanFetchSubresources) {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      R"(
    <script>
      fetch('data.txt')
        .then(res => res.text())
        .then(data => { console.log(data); document.title = data; })
        .catch(err => console.error(err));
    </script>)");
  builder.AddExchange(kPrimaryUrl + "/data.txt",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "some data");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      GURL(kPrimaryUrl),
      IsolationData{IsolationData::InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));

  NavigateAndWaitForTitle(GURL(kPrimaryUrl), u"some data");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       InvalidStatusCode) {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "201"}, {"content-type", "text/html"}},
                      "<title>Hello Isolated Apps</title>");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      GURL(kPrimaryUrl),
      IsolationData{IsolationData::InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));

  NavigateAndWaitForError(
      GURL(kPrimaryUrl),
      "Failed to read response from Signed Web Bundle: The response has an "
      "unsupported HTTP status code: 201 (only status code 200 is allowed).");
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       NonExistingResource) {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "200"}, {"content-type", "text/html"}},
                      "<title>Hello Isolated Apps</title>");
  base::FilePath bundle_path = SignAndWriteBundleToDisk(builder.CreateBundle());

  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      GURL(kPrimaryUrl),
      IsolationData{IsolationData::InstalledBundle{.path = bundle_path}});
  RegisterWebApp(std::move(iwa));

  NavigateAndWaitForError(
      GURL(kPrimaryUrl + "/non-existing"),
      "Failed to read response from Signed Web Bundle: The Web Bundle does not "
      "contain a response for the provided URL: "
      "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/"
      "non-existing");
}

class IsolatedWebAppURLLoaderFactoryNonExistingBundlesBrowserTest
    : public IsolatedWebAppURLLoaderFactoryBrowserTest,
      public testing::WithParamInterface<std::pair<GURL, std::string>> {
 public:
  IsolatedWebAppURLLoaderFactoryNonExistingBundlesBrowserTest()
      : url_(std::get<0>(GetParam())),
        error_message_(std::get<1>(GetParam())) {}

 protected:
  GURL url_;
  std::string error_message_;
};

IN_PROC_BROWSER_TEST_P(
    IsolatedWebAppURLLoaderFactoryNonExistingBundlesBrowserTest,
    NonExistingBundle) {
  std::unique_ptr<WebApp> iwa = CreateIsolatedWebApp(
      GURL(kPrimaryUrl),
      IsolationData{IsolationData::InstalledBundle{
          .path =
              base::FilePath(FILE_PATH_LITERAL("/this/path/does/not/exist"))}});
  RegisterWebApp(std::move(iwa));

  NavigateAndWaitForError(url_, error_message_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppURLLoaderFactoryNonExistingBundlesBrowserTest,
    ::testing::Values(
        std::make_pair(
            GURL("isolated-app://foo"),
            "The host of isolated-app:// URLs must be a valid Signed Web "
            "Bundle ID (got foo): The signed web bundle ID must be exactly 56 "
            "characters long, but was 3 characters long."),
        std::make_pair(
            GURL("isolated-app://"
                 "9erugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac"),
            "The host of isolated-app:// URLs must be a valid Signed Web "
            "Bundle ID (got "
            "9erugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac): The "
            "signed web bundle ID must only contain lowercase ASCII characters "
            "and digits between 2 and 7 (without any padding)."),
        std::make_pair(
            GURL("isolated-app://"
                 "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac"),
            "Failed to read response from Signed Web Bundle: Failed to parse "
            "integrity block: FILE_ERROR_NOT_FOUND")));

}  // namespace web_app
