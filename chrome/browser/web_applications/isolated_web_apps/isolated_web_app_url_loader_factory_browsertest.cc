// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class IsolatedWebAppURLLoaderFactoryBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 protected:
  void TearDown() override { IsolatedWebAppBrowserTestHarness::TearDown(); }

  std::pair<content::RenderFrameHost*, IsolatedWebAppUrlInfo> InstallAndOpenApp(
      ScopedBundledIsolatedWebApp* app) {
    CHECK(app) << "Creating web bundle unsuccessful!";
    app->TrustSigningKey();
    IsolatedWebAppUrlInfo url_info = app->InstallChecked(profile());
    content::RenderFrameHost* iwa_frame =
        OpenIsolatedWebApp(profile(), url_info.app_id());
    CHECK(iwa_frame) << "Opening installed app unsuccessful!";
    return {iwa_frame, url_info};
  }

  void NavigateAndWaitForTitle(content::RenderFrameHost*& iwa_frame,
                               const GURL& url,
                               const std::u16string& page_title) {
    ASSERT_THAT(iwa_frame, NotNull());
    content::TitleWatcher title_watcher(
        content::WebContents::FromRenderFrameHost(iwa_frame), page_title);

    iwa_frame =
        ui_test_utils::NavigateToURL(GetBrowserFromFrame(iwa_frame), url);

    EXPECT_THAT(title_watcher.WaitAndGetTitle(), Eq(page_title));
    EXPECT_THAT(iwa_frame->IsErrorDocument(), IsFalse());
  }

  void NavigateAndWaitForError(content::RenderFrameHost*& iwa_frame,
                               const GURL& url,
                               const std::string& error_messsage) {
    ASSERT_THAT(iwa_frame, NotNull());
    content::WebContentsConsoleObserver console_observer(
        content::WebContents::FromRenderFrameHost(iwa_frame));
    console_observer.SetFilter(base::BindRepeating(
        [](const content::WebContentsConsoleObserver::Message& message) {
          return message.log_level ==
                     blink::mojom::ConsoleMessageLevel::kError &&
                 base::Contains(
                     message.message,
                     u"Failed to read response from Signed Web Bundle");
        }));

    iwa_frame =
        ui_test_utils::NavigateToURL(GetBrowserFromFrame(iwa_frame), url);

    ASSERT_TRUE(console_observer.Wait());
    EXPECT_THAT(iwa_frame->IsErrorDocument(), IsTrue());
    EXPECT_THAT(iwa_frame->GetLastCommittedURL(), Eq(url));
    EXPECT_THAT(console_observer.messages().size(), Eq(1ul))
        << MessagesAsString(console_observer.messages());
    EXPECT_THAT(console_observer.GetMessageAt(0), Eq(error_messsage));
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest, LoadsBundle) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/", "<title>Hello Isolated Apps</title>")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForTitle(
      iwa_frame, url_info.origin().GetURL(), u"Hello Isolated Apps"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       LoadsSubResourcesFromBundle) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/", "<script src=\"script.js\"></script>")
          .AddJs("/script.js", "document.title = 'title from js';")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForTitle(
      iwa_frame, url_info.origin().GetURL(), u"title from js"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       CanFetchSubresources) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml(
              "/",
              R"(<script type="text/javascript" src="/script.js"></script>)")
          .AddJs("/script.js", R"(
            fetch('title.txt')
              .then(res => res.text())
              .then(data => { console.log(data); document.title = data; })
              .catch(err => console.error(err));
            )")
          .AddResource("/title.txt", "some data", "text/plain")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForTitle(
      iwa_frame, url_info.origin().GetURL(), u"some data"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       InvalidStatusCode) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddResource("/", "<title>Hello Isolated Apps</title>",
                       {{.name = "Content-Type", .value = "text/html"}},
                       net::HttpStatusCode::HTTP_CREATED)
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForError(
      iwa_frame, url_info.origin().GetURL(),
      "Failed to read response from Signed Web Bundle: The response has an "
      "unsupported HTTP status code: 201 (only status code 200 is allowed)."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       NonExistingResource) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/", "<title>Hello Isolated Apps</title>")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForError(
      iwa_frame, url_info.origin().GetURL().Resolve("/non-existing"),
      "Failed to read response from Signed Web Bundle: The Web Bundle does not "
      "contain a response for the provided URL: "
      "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/"
      "non-existing"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       UrlLoaderFactoryCanUseServiceWorker) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/", R"html(
                  <html>
                    <head>
                      <script type="text/javascript" src="/script.js"></script>
                    </head>
                  </html>
                  )html")
          .AddResource("/title.txt", "data from web bundle", "text/plain")
          .AddJs("/script.js", R"js(
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
            )js")
          .AddJs("/service_worker.js", R"js(
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
            )js")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForTitle(
      iwa_frame, url_info.origin().GetURL(),
      u"data from web bundle data from service worker"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryBrowserTest,
                       NoCrashIfBrowserIsClosed) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/", std::string(1000000, 'a'))
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  // Do not wait for the request to complete, and immediately close the browser.
  // No crash should occur due to this.
  NavigateToURLWithDisposition(
      GetBrowserFromFrame(iwa_frame), url_info.origin().GetURL(),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NO_WAIT);
  ASSERT_NO_FATAL_FAILURE(chrome::CloseAllBrowsers());
}

class IsolatedWebAppURLLoaderFactoryFrameBrowserTest
    : public IsolatedWebAppURLLoaderFactoryBrowserTest {
 protected:
  void NavigateAndCheckForErrors(content::RenderFrameHost*& iwa_frame,
                                 const GURL& url) {
    iwa_frame =
        ui_test_utils::NavigateToURL(GetBrowserFromFrame(iwa_frame), url);

    // It is not easily possible from JavaScript to determine whether a frame
    // has loaded successfully or errored (`frame.onload` also triggers when an
    // error page is loaded in the frame, `frame.onerror` never triggers). Thus,
    // we eval JS inside the created frame to compare the frame's content to the
    // expected content.
    int sub_frame_count = 0;
    iwa_frame->ForEachRenderFrameHost(
        [&sub_frame_count](content::RenderFrameHost* rfh) {
          if (rfh->IsInPrimaryMainFrame()) {
            return;
          }
          ++sub_frame_count;
          EXPECT_THAT(content::EvalJs(rfh, "document.body.innerText"),
                      Eq("inner frame content"));
        });
    EXPECT_THAT(sub_frame_count, Eq(1));
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryFrameBrowserTest,
                       CanUseDataUrlForFrame) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/",
                   "<iframe src=\"data:text/html,<h1>inner frame "
                   "content</h1>\"></iframe>")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());
  ASSERT_NO_FATAL_FAILURE(
      NavigateAndCheckForErrors(iwa_frame, url_info.origin().GetURL()));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppURLLoaderFactoryFrameBrowserTest,
                       CanUseBlobUrlForFrame) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/", "<script src=\"script.js\"></script>")
          .AddJs("/script.js", R"(
            const iframe = document.createElement("iframe");
            document.currentScript.appendChild(iframe);
            const blob = new Blob(
              ['<h1>inner frame content</h1>'],
              {type : 'text/html'}
            );
            iframe.src = window.URL.createObjectURL(blob);
            )")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());
  ASSERT_NO_FATAL_FAILURE(
      NavigateAndCheckForErrors(iwa_frame, url_info.origin().GetURL()));
}

class IsolatedWebAppURLLoaderFactoryCSPBrowserTest
    : public IsolatedWebAppURLLoaderFactoryBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  IsolatedWebAppBuilder GetBuilderWithIndexHtml(const std::string& csp) {
    IsolatedWebAppBuilder::Headers headers{{"content-type", "text/html"}};
    std::string html;
    if (GetParam()) {
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
    return IsolatedWebAppBuilder(ManifestBuilder())
        .AddResource("/", html, headers);
  }
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppURLLoaderFactoryCSPBrowserTest,
                       CanMakeCSPStricter) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      GetBuilderWithIndexHtml("connect-src 'none'")
          .AddJs("/script.js", R"(
            fetch('file.txt')
              .then(res =>
                console.error(`Unexpectedly fetched file: ` + res.text()))
              .catch(err => {
                console.log(err);
                document.title = "unable to fetch";
              });
            )")
          .AddResource("/file.txt", "some data", "text/plain")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForTitle(
      iwa_frame, url_info.origin().GetURL(), u"unable to fetch"));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppURLLoaderFactoryCSPBrowserTest,
                       CannotMakeCSPLessStrict) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      GetBuilderWithIndexHtml("script-src 'self' 'unsafe-eval'")
          .AddJs("/script.js", R"(
            try {
              eval("1+1");
              console.error("Eval unexpectedly ran.");
            } catch (err) {
              console.log(err);
              document.title = "unable to eval";
            }
            )")
          .BuildBundle(test::GetDefaultEd25519KeyPair());
  auto [iwa_frame, url_info] = InstallAndOpenApp(app.get());

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForTitle(
      iwa_frame, url_info.origin().GetURL(), u"unable to eval"));
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
