// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/web_contents/web_app_url_loader.h"

#include <optional>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace webapps {

using UrlResult = WebAppUrlLoader::Result;
using UrlComparison = WebAppUrlLoader::UrlComparison;

const char kGenericPageContent[] = "<html><body>Content</body></html>";

// Returns a redirect response to |dest| URL.
std::unique_ptr<net::test_server::HttpResponse> HandleServerRedirect(
    const std::string& dest,
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  LOG(INFO) << "Redirecting from " << request_url << " to " << dest;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader("Location", dest);
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      dest.c_str()));
  return http_response;
}

// Run |handler| on requests that exactly match the |relative_url|.
std::unique_ptr<net::test_server::HttpResponse>
HandleMatchingRequestOrReturnEmptyPage(
    const std::string& relative_url,
    const net::EmbeddedTestServer::HandleRequestCallback& handler,
    const net::test_server::HttpRequest& request) {
  GURL match = request.GetURL().Resolve(relative_url);
  if (request.GetURL() == match) {
    return handler.Run(request);
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  return http_response;
}

class WebAppUrlLoaderTest : public PlatformBrowserTest {
 public:
  WebAppUrlLoaderTest() = default;
  WebAppUrlLoaderTest(const WebAppUrlLoaderTest&) = delete;
  WebAppUrlLoaderTest& operator=(const WebAppUrlLoaderTest&) = delete;
  ~WebAppUrlLoaderTest() override = default;

  void SetUpOnMainThread() override {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));

    host_resolver()->AddRule("*", "127.0.0.1");
    PlatformBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // The WebContents needs to be destroyed before the profile.
    web_contents_.reset();
    PlatformBrowserTest::TearDownOnMainThread();
  }

  UrlResult LoadUrlAndWait(UrlComparison url_comparison,
                           const std::string& path) {
    std::optional<UrlResult> result;
    base::RunLoop run_loop;
    WebAppUrlLoader loader;
    loader.LoadUrl(embedded_test_server()->GetURL(path), web_contents(),
                   url_comparison, base::BindLambdaForTesting([&](UrlResult r) {
                     result = r;
                     run_loop.Quit();
                   }));
    EXPECT_TRUE(web_contents()->IsLoading());
    run_loop.Run();
    // Currently WebAppUrlLoader uses DidFinishLoad which might get called
    // even if WebContents::IsLoading() is true.
    // TODO(ortuno): Check that the WebContents is no longer loading after
    // the callback is called.

    return result.value();
  }

 protected:
Profile* profile() {
  return chrome_test_utils::GetProfile(this);
}

  content::WebContents* web_contents() { return web_contents_.get(); }

  void ResetWebContents() { web_contents_.reset(); }

  // Set up a server redirect from |src_relative| (a relative URL) to |dest|.
  // Must be called before the server is started.
  void SetupRedirect(const std::string& src_relative, const std::string& dest) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HandleMatchingRequestOrReturnEmptyPage, src_relative,
        base::BindRepeating(&HandleServerRedirect, dest)));
  }

  // Set up the server to always report the given HTTP response `code` and
  // optionally respond with the given `content`. Must be called before the
  // server is started.
  void SetupHttpResponseWithContent(const net::HttpStatusCode code,
                                    std::optional<std::string> content) {
    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [code, content](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(code);
          if (content.has_value()) {
            http_response->set_content_type("text/html");
            http_response->set_content(content.value());
          }
          return http_response;
        }));
  }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
};

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Loaded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kUrlLoaded,
            LoadUrlAndWait(UrlComparison::kExact, "/simple.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, LoadedWithParamChangeIgnored) {
  SetupRedirect("/test-redirect", "/test-redirect?param=stuff");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kUrlLoaded,
            LoadUrlAndWait(UrlComparison::kIgnoreQueryParamsAndRef,
                           "/test-redirect"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest,
                       LoadedWithParamAndRefChangeIgnored) {
  // Note we cannot test the ref change in isolation as it is not sent to the
  // server, so we cannot check it in the request handler.
  SetupRedirect("/test-redirect", "/test-redirect?param=foo#ref");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kUrlLoaded,
            LoadUrlAndWait(UrlComparison::kIgnoreQueryParamsAndRef,
                           "/test-redirect"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, LoadedWithPathChangeIgnored) {
  SetupRedirect("/test-redirect", "/test-redirect-new-path");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kUrlLoaded,
            LoadUrlAndWait(UrlComparison::kSameOrigin, "/test-redirect"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, RedirectWithParamChange) {
  SetupRedirect("/test-redirect", "/test-redirect?param=stuff");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kRedirectedUrlLoaded,
            LoadUrlAndWait(UrlComparison::kExact, "/test-redirect"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, RedirectWithPathChange) {
  SetupRedirect("/test-redirect", "/test-redirect-new-path");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kRedirectedUrlLoaded,
            LoadUrlAndWait(UrlComparison::kIgnoreQueryParamsAndRef,
                           "/test-redirect"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, 302FoundRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL final_url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_EQ(UrlResult::kRedirectedUrlLoaded,
            LoadUrlAndWait(UrlComparison::kIgnoreQueryParamsAndRef,
                           "/server-redirect-302?" + final_url.spec()));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Http404ErrorWithContent) {
  SetupHttpResponseWithContent(net::HTTP_NOT_FOUND, kGenericPageContent);
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kFailedErrorPageLoaded,
            LoadUrlAndWait(UrlComparison::kExact, "/unused.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Http407ErrorWithoutContent) {
  SetupHttpResponseWithContent(net::HTTP_PROXY_AUTHENTICATION_REQUIRED,
                               /*content=*/std::nullopt);
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kFailedErrorPageLoaded,
            LoadUrlAndWait(UrlComparison::kExact, "/unused.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Http500ErrorWithContent) {
  SetupHttpResponseWithContent(net::HTTP_INTERNAL_SERVER_ERROR,
                               kGenericPageContent);
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kFailedErrorPageLoaded,
            LoadUrlAndWait(UrlComparison::kExact, "/unused.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Http500ErrorWithoutContent) {
  SetupHttpResponseWithContent(net::HTTP_INTERNAL_SERVER_ERROR,
                               /*content=*/std::nullopt);
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(UrlResult::kFailedErrorPageLoaded,
            LoadUrlAndWait(UrlComparison::kExact, "/unused.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Hung) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  WebAppUrlLoader loader;
  std::optional<UrlResult> result;

  loader.LoadUrl(embedded_test_server()->GetURL("/hung"), web_contents(),
                 UrlComparison::kExact,
                 base::BindLambdaForTesting([&](UrlResult r) { result = r; }));
  // Forward the clock so that |loader| times out first load of about:blank.
  // It is unclear why this load also needs to time out, and can't just load
  // correctly.
  task_runner->FastForwardBy(
    webapps::WebAppUrlLoader::kSecondsToWaitForWebContentsLoad);
  task_runner->RunUntilIdle();

  // Run all pending tasks. The URL should still be loading now.
  EXPECT_TRUE(web_contents()->IsLoading());
  task_runner->RunUntilIdle();
  EXPECT_TRUE(web_contents()->IsLoading());

  // The callback didn't run yet because the site is still loading.
  EXPECT_FALSE(result.has_value());

  // Forward the clock so that |loader| times out.
  task_runner->FastForwardBy(
    webapps::WebAppUrlLoader::kSecondsToWaitForWebContentsLoad);
  task_runner->RunUntilIdle();
  ASSERT_TRUE(result);
  EXPECT_EQ(UrlResult::kFailedPageTookTooLong, result.value());
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, WebContentsDestroyed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppUrlLoader loader;
  std::optional<UrlResult> result;

  base::RunLoop run_loop;
  loader.LoadUrl(embedded_test_server()->GetURL("/hung"), web_contents(),
                 UrlComparison::kExact,
                 base::BindLambdaForTesting([&](UrlResult r) {
                   result = r;
                   run_loop.Quit();
                 }));

  // Run all pending tasks. The URL should still be loading.
  EXPECT_TRUE(web_contents()->IsLoading());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents()->IsLoading());

  // The callback didn't run yet because the site is still loading.
  EXPECT_FALSE(result.has_value());

  ResetWebContents();
  run_loop.Run();

  EXPECT_EQ(UrlResult::kFailedWebContentsDestroyed, result.value());
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, MultipleLoadUrlCalls) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppUrlLoader loader;
  std::optional<UrlResult> title1_result;
  std::optional<UrlResult> title2_result;

  std::unique_ptr<content::WebContents> web_contents1 =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  loader.LoadUrl(embedded_test_server()->GetURL("/title1.html"),
                 web_contents1.get(), UrlComparison::kExact,
                 base::BindLambdaForTesting([&](UrlResult r) {
                   title1_result = r;
                   barrier_closure.Run();
                 }));
  loader.LoadUrl(embedded_test_server()->GetURL("/title2.html"),
                 web_contents2.get(), UrlComparison::kExact,
                 base::BindLambdaForTesting([&](UrlResult r) {
                   title2_result = r;
                   barrier_closure.Run();
                 }));
  run_loop.Run();
  EXPECT_EQ(UrlResult::kUrlLoaded, title1_result.value());
  EXPECT_EQ(UrlResult::kUrlLoaded, title2_result.value());
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, NetworkError) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(
      UrlResult::kFailedErrorPageLoaded,
      LoadUrlAndWait(UrlComparison::kIgnoreQueryParamsAndRef, "/close-socket"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest,
                       PrepareForLoad_AfterNavigationComplete) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppUrlLoader loader;

  // Load a URL, and wait for its completion.
  LoadUrlAndWait(UrlComparison::kExact, "/title1.html");

  // Load the next URL.
  LoadUrlAndWait(UrlComparison::kExact, "/title2.html");
}

namespace {
class WebContentsLoadingObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsLoadingObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}
  WebContentsLoadingObserver(const WebContentsLoadingObserver&) = delete;
  WebContentsLoadingObserver& operator=(const WebContentsLoadingObserver&) =
      delete;
  ~WebContentsLoadingObserver() override = default;

  void Wait() { run_loop_.Run(); }

  // content::WebContentsObserver:
  void DidStartLoading() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest,
                       PrepareForLoad_BeforeNavigationComplete) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppUrlLoader loader;

  // Load a URL that takes a long time to load. Use /hung-after-headers here
  // because it starts the HTTP response, but never returns a HTML document.
  // We intentionally don't wait for load completion.
  {
    WebContentsLoadingObserver observer(web_contents());
    loader.LoadUrl(embedded_test_server()->GetURL("/hung-after-headers"),
                   web_contents(), UrlComparison::kExact, base::DoNothing());
    observer.Wait();
  }

  // Load the next URL.
  LoadUrlAndWait(UrlComparison::kExact, "/title2.html");
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, PrepareForLoad_RecordResultMetric) {
  base::HistogramTester histograms;
  static constexpr char kPrepareForLoadResultHistogramName[] =
      "Webapp.WebAppUrlLoaderPrepareForLoadResult";

  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppUrlLoader loader;

  // Load a URL, and wait for its completion.
  LoadUrlAndWait(UrlComparison::kExact, "/title1.html");
  histograms.ExpectTotalCount(kPrepareForLoadResultHistogramName, 1);
  histograms.ExpectBucketCount(kPrepareForLoadResultHistogramName,
                               UrlResult::kUrlLoaded, 1);

  // Load the next URL.
  LoadUrlAndWait(UrlComparison::kExact, "/title2.html");
  histograms.ExpectTotalCount(kPrepareForLoadResultHistogramName, 2);
  histograms.ExpectBucketCount(kPrepareForLoadResultHistogramName,
                               UrlResult::kUrlLoaded, 2);
}

}  // namespace webapps
