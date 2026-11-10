// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/content/browser/distiller_page_web_contents.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_distiller/content/browser/distiller_javascript_utils.h"
#include "components/web_distiller/content/browser/test/test_util.h"
#include "components/web_distiller/core/distiller_options.h"
#include "components/web_distiller/core/distiller_page.h"
#include "components/web_distiller/core/proto/distilled_article.pb.h"
#include "components/web_distiller/core/proto/distilled_page.pb.h"
#include "components/web_distiller/core/viewer.h"
#include "components/web_distiller/core/web_distiller_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::ContentBrowserTest;
using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleTestRequests(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/hung") {
    return std::make_unique<net::test_server::HungResponse>();
  }
  if (request.GetURL().path() == "/client-redirect-js") {
    std::string dest =
        base::UnescapeBinaryURLComponent(request.GetURL().query());
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<!doctype html><script>window.location.href = \"" + dest +
        "\";</script><script src=\"/hung\"></script>");
    return http_response;
  }
  if (request.GetURL().path() == "/post-dcl-redirect") {
    std::string dest =
        base::UnescapeBinaryURLComponent(request.GetURL().query());
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<!doctype html><html><head><title>Original Article Title</title>"
        "<script>document.addEventListener('DOMContentLoaded', () => {"
        " window.location.href = \"" +
        dest +
        "\"; });</script></head><body><div>"
        "<p>Original article paragraph text. Lorem ipsum dolor sit amet, at "
        "alia aliquip vel. Quas inani labore an vel. Sed an nemore minimum "
        "accusata. Sint inermis tacimates est ex, ad movet iracundia mei, "
        "delicata iracundia laboramus ei eos. Illud principes complectitur te "
        "nec, ius alienum insolens ea, cu quo oratio omnesque.</p>"
        "<p>Lorem ipsum dolor sit amet, at alia aliquip vel. Quas inani "
        "labore an vel. Sed an nemore minimum accusata. Sint inermis "
        "tacimates est ex, ad movet iracundia mei, delicata iracundia "
        "laboramus ei eos. Illud principes complectitur te nec, ius alienum "
        "insolens ea, cu quo oratio omnesque.</p>"
        "</div></body></html>");
    return http_response;
  }
  if (request.GetURL().path() == "/hung-dom") {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<!doctype html><script src=\"/hung\"></script>");
    return http_response;
  }
  if (request.GetURL().path() == "/same-document-nav") {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<!doctype html><html><head><title>Test Page Title</title>"
        "<script>"
        "history.replaceState(null, '', '/same-document-nav?cleaned=1');"
        "window.location.hash = '#section';"
        "</script></head><body><div>"
        "<p>Lorem ipsum dolor sit amet, at alia aliquip vel. Quas inani "
        "labore an vel. Sed an nemore minimum accusata. Sint inermis "
        "tacimates est ex, ad movet iracundia mei, delicata iracundia "
        "laboramus ei eos. Illud principes complectitur te nec, ius alienum "
        "insolens ea, cu quo oratio omnesque.</p>"
        "<p>Lorem ipsum dolor sit amet, at alia aliquip vel. Quas inani "
        "labore an vel. Sed an nemore minimum accusata. Sint inermis "
        "tacimates est ex, ad movet iracundia mei, delicata iracundia "
        "laboramus ei eos. Illud principes complectitur te nec, ius alienum "
        "insolens ea, cu quo oratio omnesque.</p>"
        "</div></body></html>");
    return http_response;
  }
  if (request.GetURL().path() == "/csp-sandbox-article") {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    // `sandbox` without `allow-same-origin` commits the document with a unique
    // opaque origin even though its URL matches `distill_url_`. `allow-scripts`
    // models real sandboxed article pages whose main-world scripts still run
    // while opaque (isolated-world distillation scripts also run regardless via
    // `kExecuteScriptWhenScriptsDisabled`).
    http_response->AddCustomHeader("Content-Security-Policy",
                                   "sandbox allow-scripts");
    http_response->set_content(
        "<!doctype html><html><head><title>Test Page Title</title></head>"
        "<body><div>"
        "<p>Lorem ipsum dolor sit amet, at alia aliquip vel. Quas inani "
        "labore an vel. Sed an nemore minimum accusata. Sint inermis "
        "tacimates est ex, ad movet iracundia mei, delicata iracundia "
        "laboramus ei eos. Illud principes complectitur te nec, ius alienum "
        "insolens ea, cu quo oratio omnesque.</p>"
        "<p>Lorem ipsum dolor sit amet, at alia aliquip vel. Quas inani "
        "labore an vel. Sed an nemore minimum accusata. Sint inermis "
        "tacimates est ex, ad movet iracundia mei, delicata iracundia "
        "laboramus ei eos. Illud principes complectitur te nec, ius alienum "
        "insolens ea, cu quo oratio omnesque.</p>"
        "</div></body></html>");
    return http_response;
  }
  return nullptr;
}

// Helper class to know how far in the loading process the current WebContents
// has come. It will call the callback either after
// DidCommitProvisionalLoadForFrame or DOMContentLoaded is called for the
// main frame, based on the value of |wait_for_document_loaded|.
class WebContentsMainFrameHelper : public content::WebContentsObserver {
 public:
  WebContentsMainFrameHelper(content::WebContents* web_contents,
                             base::OnceClosure callback,
                             bool wait_for_document_loaded)
      : WebContentsObserver(web_contents),
        callback_(std::move(callback)),
        wait_for_document_loaded_(wait_for_document_loaded) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (wait_for_document_loaded_ || !callback_) {
      return;
    }
    if (navigation_handle->HasCommitted() &&
        navigation_handle->IsInMainFrame()) {
      std::move(callback_).Run();
    }
  }

  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    if (wait_for_document_loaded_ && callback_) {
      if (!render_frame_host->GetParent()) {
        std::move(callback_).Run();
      }
    }
  }

 private:
  base::OnceClosure callback_;
  bool wait_for_document_loaded_;
};

}  // namespace

namespace web_distiller {

const char* kSimpleArticlePath = "/simple_article.html";
const char* kVideoArticlePath = "/video_article.html";
const char* kShadowDomArticlePath = "/shadow_dom_article.html";

class DistillerPageWebContentsTest : public ContentBrowserTest {
 public:
  DistillerPageWebContentsTest() = default;

  // ContentBrowserTest:
  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    content::ShellContentBrowserClient::Get()
        ->set_create_throttles_for_navigation_callback(
            base::BindRepeating(&web_distiller::DistillerPageWebContents::
                                    MaybeCreateAndAddNavigationThrottle));
    AddComponentsResources();
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleTestRequests));
    SetUpTestServer(embedded_test_server());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void DistillPage(base::OnceClosure quit_closure, const std::string& url) {
    distiller_page_->DistillPage(
        embedded_test_server()->GetURL(url), web_distiller::DistillerOptions(),
        base::BindOnce(
            &DistillerPageWebContentsTest::OnPageDistillationFinished,
            base::Unretained(this), std::move(quit_closure)));
  }

  void OnPageDistillationFinished(
      base::OnceClosure quit_closure,
      std::unique_ptr<DistilledPageProto> distiller_result,
      DistillationParseResult /*result*/) {
    distiller_result_ = std::move(distiller_result);
    std::move(quit_closure).Run();
  }

 protected:
  void RunUseCurrentWebContentsTest(const std::string& url,
                                    bool expect_new_web_contents,
                                    bool wait_for_document_loaded);

  raw_ptr<DistillerPageWebContents, DanglingUntriaged> distiller_page_;
  std::unique_ptr<DistilledPageProto> distiller_result_;
};

// Use this class to be able to leak the WebContents, which is needed for when
// the current WebContents is used for distillation.
class TestDistillerPageWebContents : public DistillerPageWebContents {
 public:
  TestDistillerPageWebContents(
      content::BrowserContext* browser_context,
      const gfx::Size& render_view_size,
      std::unique_ptr<SourcePageHandleWebContents> optional_web_contents_handle,
      bool expect_new_web_contents)
      : DistillerPageWebContents(browser_context,
                                 render_view_size,
                                 std::move(optional_web_contents_handle)),
        expect_new_web_contents_(expect_new_web_contents),
        new_web_contents_created_(false) {}

  void CreateNewWebContents(const GURL& url) override {
    EXPECT_TRUE(expect_new_web_contents_);
    new_web_contents_created_ = true;
    DistillerPageWebContents::CreateNewWebContents(url);
    EXPECT_TRUE(GetUserDataForTesting());
  }

  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    DistillerPageWebContents::DOMContentLoaded(render_frame_host);
    MaybeInterceptJavaScriptExecution();
  }

  void SetNavigateDuringJavaScriptUrl(const GURL& url) {
    navigate_during_js_url_ = url;
  }

  bool new_web_contents_created() { return new_web_contents_created_; }

 protected:
  void DistillPageImpl(const GURL& url, const std::string& script) override {
    DistillerPageWebContents::DistillPageImpl(url, script);
    MaybeInterceptJavaScriptExecution();
  }

 private:
  void MaybeInterceptJavaScriptExecution() {
    if (!navigate_during_js_url_.is_valid() || state_ != EXECUTING_JAVASCRIPT) {
      return;
    }
    GURL nav_url = std::exchange(navigate_during_js_url_, GURL());
    // Replace the in-flight isolated-world callback so a cross-document
    // navigation deterministically commits after ExecuteJavaScript() has
    // started (and stopped observing) but before OnWebContentsDistillationDone
    // consumes the result.
    weak_factory_.InvalidateWeakPtrs();
    const GURL page_url = TargetRenderFrameHost().GetLastCommittedURL();
    const base::TimeTicks javascript_start = base::TimeTicks::Now();
    RunIsolatedJavaScript(
        &TargetRenderFrameHost(), script_,
        base::BindOnce(
            &TestDistillerPageWebContents::NavigateBeforeDeliveringResult,
            test_weak_factory_.GetWeakPtr(), nav_url, page_url,
            javascript_start));
  }

  void NavigateBeforeDeliveringResult(const GURL& nav_url,
                                      const GURL& page_url,
                                      base::TimeTicks javascript_start,
                                      base::Value value) {
    content::WebContents* web_contents = source_page_handle_->web_contents();
    post_js_nav_helper_ = std::make_unique<WebContentsMainFrameHelper>(
        web_contents,
        base::BindOnce(&DistillerPageWebContents::OnWebContentsDistillationDone,
                       weak_factory_.GetWeakPtr(), page_url, javascript_start,
                       std::move(value)),
        /*wait_for_document_loaded=*/false);
    web_contents->GetController().LoadURL(
        nav_url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  }

  bool expect_new_web_contents_;
  bool new_web_contents_created_;
  GURL navigate_during_js_url_;
  std::unique_ptr<WebContentsMainFrameHelper> post_js_nav_helper_;
  base::WeakPtrFactory<TestDistillerPageWebContents> test_weak_factory_{this};
};

#if BUILDFLAG(IS_WIN)
#define MAYBE_BasicDistillationWorks DISABLED_BasicDistillationWorks
#else
#define MAYBE_BasicDistillationWorks BasicDistillationWorks
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_BasicDistillationWorks) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  EXPECT_EQ("Test Page Title", distiller_result_->title());
  EXPECT_THAT(distiller_result_->html(), HasSubstr("Lorem ipsum"));
  EXPECT_THAT(distiller_result_->html(),
              Not(HasSubstr("questionable content")));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_HandlesRelativeLinks DISABLED_HandlesRelativeLinks
#else
#define MAYBE_HandlesRelativeLinks HandlesRelativeLinks
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_HandlesRelativeLinks) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  // A relative link should've been updated.
  EXPECT_THAT(distiller_result_->html(),
              ContainsRegex("href=\"http://127.0.0.1:.*/relativelink.html\""));
  EXPECT_THAT(distiller_result_->html(),
              HasSubstr("href=\"http://www.google.com/absolutelink.html\""));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_HandlesRelativeImages DISABLED_HandlesRelativeImages
#else
#define MAYBE_HandlesRelativeImages HandlesRelativeImages
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_HandlesRelativeImages) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  // A relative link should've been updated.
  EXPECT_THAT(distiller_result_->html(),
              ContainsRegex("src=\"http://127.0.0.1:.*/relativeimage.png\""));
  EXPECT_THAT(distiller_result_->html(),
              HasSubstr("src=\"http://www.google.com/absoluteimage.png\""));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_HandlesRelativeVideos DISABLED_HandlesRelativeVideos
#else
#define MAYBE_HandlesRelativeVideos HandlesRelativeVideos
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_HandlesRelativeVideos) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kVideoArticlePath);
  run_loop.Run();

  // A relative source should've been updated.
  EXPECT_THAT(distiller_result_->html(),
              ContainsRegex("src=\"http://127.0.0.1:.*/relative_video.webm\""));
  EXPECT_THAT(distiller_result_->html(),
              HasSubstr("src=\"http://www.google.com/absolute_video.ogg\""));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_VisibilityDetection DISABLED_VisibilityDetection
#else
#define MAYBE_VisibilityDetection VisibilityDetection
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_VisibilityDetection) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  // visble_style.html and invisible_style.html only differ by the visibility
  // internal stylesheet.

  {
    base::RunLoop run_loop;
    DistillPage(run_loop.QuitClosure(), "/visible_style.html");
    run_loop.Run();
    EXPECT_THAT(distiller_result_->html(), HasSubstr("Lorem ipsum"));
  }

  {
    base::RunLoop run_loop;
    DistillPage(run_loop.QuitClosure(), "/invisible_style.html");
    run_loop.Run();
    EXPECT_THAT(distiller_result_->html(), Not(HasSubstr("Lorem ipsum")));
  }
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesShadowDOM) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kShadowDomArticlePath);
  run_loop.Run();

  const std::string& html = distiller_result_->html();

  EXPECT_EQ("Shadow DOM Test Page Title", distiller_result_->title());

  // The article body lives in an open shadow root, which cloneNode() does not
  // copy. Without flattening, the distiller sees an empty <custom-article> and
  // produces nothing at all.
  EXPECT_THAT(html,
              HasSubstr("The city botanical garden was established in the late "
                        "nineteenth"));

  // Light DOM slotted into the shadow tree. Note this text alone does not
  // prove much -- it survives a plain deep clone too -- but it would be lost
  // by a flattening clone that descends into the shadow root without
  // resolving <slot> assignments.
  EXPECT_THAT(html, HasSubstr("The central glass conservatory and palm house"));

  // A <slot> with nothing assigned renders its fallback content.
  EXPECT_THAT(
      html,
      HasSubstr("Seasonal exhibitions and educational tours are offered"));

  // Content re-projected through a nested shadow root's default slot.
  EXPECT_THAT(
      html,
      HasSubstr("The newly renovated alpine pavilion houses a wide variety"));

  // A <slot> outside any shadow tree is an ordinary element that renders its
  // children; it has no assigned nodes.
  EXPECT_THAT(
      html, HasSubstr("The conservatory's research library contains extensive "
                      "historical"));

  // "continue-read-break" contains the substring "ad-break". Before the token
  // was anchored, unlikelyCandidates matched it and _grabArticle deleted this
  // whole paragraph.
  EXPECT_THAT(
      html, HasSubstr("Horticultural specialists and groundskeepers maintain"));

  // The anchored token must still match a real ad break.
  EXPECT_THAT(html,
              Not(HasSubstr("Commercial sponsor banner and advertisement")));

  // Light DOM that is not assigned to any slot is not rendered, so it must not
  // appear in the distilled output.
  EXPECT_THAT(html, Not(HasSubstr("Unslotted sidebar navigation links")));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_DistillerContentsIgnoreRedirects \
  DISABLED_DistillerContentsIgnoreRedirects
#else
#define MAYBE_DistillerContentsIgnoreRedirects DistillerContentsIgnoreRedirects
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_DistillerContentsIgnoreRedirects) {
  GURL article_url(embedded_test_server()->GetURL(kSimpleArticlePath));
  std::string redirect_path = "/server-redirect?" + article_url.spec();

  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), redirect_path);
  run_loop.Run();

  // If redirects were allowed, simple_article would be distilled and we would
  // have a title. Since redirects are blocked by the throttle, distillation
  // fails and the title is empty.
  EXPECT_EQ("", distiller_result_->title());
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       DistillerContentsIgnoreClientRedirects) {
  GURL article_url(embedded_test_server()->GetURL(kSimpleArticlePath));
  std::string redirect_path = "/client-redirect-js?" + article_url.spec();

  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), redirect_path);
  run_loop.Run();

  // Client-side redirects in the hidden distillation WebContents must be
  // blocked so cross-site pages cannot be loaded and distilled.
  EXPECT_EQ("", distiller_result_->title());
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       DistillerContentsKeepOriginalPageOnPostDclRedirect) {
  GURL dest_url(embedded_test_server()->GetURL(kSimpleArticlePath));
  std::string redirect_path = "/post-dcl-redirect?" + dest_url.spec();

  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), redirect_path);
  run_loop.Run();

  EXPECT_EQ("Original Article Title", distiller_result_->title());
  EXPECT_THAT(distiller_result_->html(),
              HasSubstr("Original article paragraph text"));
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       DistillerContentsAllowSameDocumentNavigation) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/same-document-nav?utm_source=test");
  run_loop.Run();

  EXPECT_EQ("Test Page Title", distiller_result_->title());
  EXPECT_THAT(distiller_result_->html(), HasSubstr("Lorem ipsum"));
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       DistillerContentsAllowSandboxedPage) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/csp-sandbox-article");
  run_loop.Run();

  EXPECT_EQ("Test Page Title", distiller_result_->title());
  EXPECT_THAT(distiller_result_->html(), HasSubstr("Lorem ipsum"));
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_UsingCurrentWebContentsWrongUrl \
  DISABLED_UsingCurrentWebContentsWrongUrl
#else
#define MAYBE_UsingCurrentWebContentsWrongUrl UsingCurrentWebContentsWrongUrl
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_UsingCurrentWebContentsWrongUrl) {
  std::string url("/bogus");
  bool expect_new_web_contents = false;
  bool wait_for_document_loaded = true;
  RunUseCurrentWebContentsTest(url, expect_new_web_contents,
                               wait_for_document_loaded);
  EXPECT_EQ("", distiller_result_->title());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_UsingCurrentWebContentsNotFinishedLoadingYet \
  DISABLED_UsingCurrentWebContentsNotFinishedLoadingYet
#else
#define MAYBE_UsingCurrentWebContentsNotFinishedLoadingYet \
  UsingCurrentWebContentsNotFinishedLoadingYet
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_UsingCurrentWebContentsNotFinishedLoadingYet) {
  std::string url(kSimpleArticlePath);
  bool expect_new_web_contents = false;
  bool wait_for_document_loaded = false;
  RunUseCurrentWebContentsTest(url, expect_new_web_contents,
                               wait_for_document_loaded);
  EXPECT_EQ("Test Page Title", distiller_result_->title());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_UsingCurrentWebContentsReadyForDistillation \
  DISABLED_UsingCurrentWebContentsReadyForDistillation
#else
#define MAYBE_UsingCurrentWebContentsReadyForDistillation \
  UsingCurrentWebContentsReadyForDistillation
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_UsingCurrentWebContentsReadyForDistillation) {
  std::string url(kSimpleArticlePath);
  bool expect_new_web_contents = false;
  bool wait_for_document_loaded = true;
  RunUseCurrentWebContentsTest(url, expect_new_web_contents,
                               wait_for_document_loaded);
  EXPECT_EQ("Test Page Title", distiller_result_->title());
}

void DistillerPageWebContentsTest::RunUseCurrentWebContentsTest(
    const std::string& url,
    bool expect_new_web_contents,
    bool wait_for_document_loaded) {
  content::WebContents* current_web_contents = shell()->web_contents();
  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(current_web_contents,
                                               url_loaded_runner.QuitClosure(),
                                               wait_for_document_loaded);
  current_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(url), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  url_loaded_runner.Run();

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(current_web_contents, false));

  TestDistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::move(source_page_handle), expect_new_web_contents);
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  // Sanity check of distillation process.
  EXPECT_EQ(expect_new_web_contents, distiller_page.new_web_contents_created());
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsNavigatesBeforeReady) {
  content::WebContents* current_web_contents = shell()->web_contents();
  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(
      current_web_contents, url_loaded_runner.QuitClosure(),
      /*wait_for_document_loaded=*/false);
  current_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL("/hung-dom"), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  url_loaded_runner.Run();

  auto source_page_handle = std::make_unique<SourcePageHandleWebContents>(
      current_web_contents, /*owned=*/false);
  TestDistillerPageWebContents distiller_page(
      current_web_contents->GetBrowserContext(),
      current_web_contents->GetContainerBounds().size(),
      std::move(source_page_handle), /*expect_new_web_contents=*/false);
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/hung-dom");

  // Navigate the observed WebContents to another URL before DOMContentLoaded
  // fires for the original page. Distillation must fail rather than distilling
  // the new URL.
  current_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(kSimpleArticlePath), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  run_loop.Run();

  EXPECT_FALSE(distiller_page.new_web_contents_created());
  EXPECT_EQ("", distiller_result_->title());
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsNavigatesDuringJavaScript) {
  content::WebContents* current_web_contents = shell()->web_contents();
  auto source_page_handle = std::make_unique<SourcePageHandleWebContents>(
      current_web_contents, /*owned=*/false);
  TestDistillerPageWebContents distiller_page(
      current_web_contents->GetBrowserContext(),
      current_web_contents->GetContainerBounds().size(),
      std::move(source_page_handle), /*expect_new_web_contents=*/false);
  distiller_page.SetNavigateDuringJavaScriptUrl(
      embedded_test_server()->GetURL(kVideoArticlePath));
  distiller_page_ = &distiller_page;

  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(
      current_web_contents, url_loaded_runner.QuitClosure(),
      /*wait_for_document_loaded=*/false);
  current_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(kSimpleArticlePath), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  url_loaded_runner.Run();

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  EXPECT_FALSE(distiller_page.new_web_contents_created());
  EXPECT_EQ("", distiller_result_->title());
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsDestroyedDuringJavaScript) {
  content::WebContents::CreateParams create_params(
      shell()->web_contents()->GetBrowserContext());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(
      web_contents.get(), url_loaded_runner.QuitClosure(),
      /*wait_for_document_loaded=*/true);
  web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(kSimpleArticlePath), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  url_loaded_runner.Run();

  auto source_page_handle = std::make_unique<SourcePageHandleWebContents>(
      web_contents.get(), /*owned=*/false);
  TestDistillerPageWebContents distiller_page(
      web_contents->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::move(source_page_handle), /*expect_new_web_contents=*/false);
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  // Destroy the unowned WebContents while the isolated-world distillation
  // script is in flight so the Mojo reply callback is dropped without running.
  web_contents.reset();
  run_loop.Run();

  EXPECT_FALSE(distiller_page.new_web_contents_created());
  EXPECT_EQ("", distiller_result_->title());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_PageDestroyedBeforeFinishDistillation \
  DISABLED_PageDestroyedBeforeFinishDistillation
#else
#define MAYBE_PageDestroyedBeforeFinishDistillation \
  PageDestroyedBeforeFinishDistillation
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_PageDestroyedBeforeFinishDistillation) {
  content::WebContents* current_web_contents = shell()->web_contents();

  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(
      current_web_contents, url_loaded_runner.QuitClosure(), true);
  current_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(kSimpleArticlePath), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  url_loaded_runner.Run();

  std::unique_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(current_web_contents, false));

  TestDistillerPageWebContents* distiller_page(new TestDistillerPageWebContents(
      current_web_contents->GetBrowserContext(),
      current_web_contents->GetContainerBounds().size(),
      std::move(source_page_handle), false));
  distiller_page_ = distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);

  // It can not crash the loop when returning the result.
  delete distiller_page_;

  // Make sure the test ends when it does not crash.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       TestNoContentDoesNotCrash) {
  const std::string no_content =
      l10n_util::GetStringUTF8(IDS_WEB_DISTILLER_VIEWER_NO_DATA_CONTENT);

  {  // Test zero pages.
    std::unique_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    std::string js = viewer::GetUnsafeArticleContentJs(article_proto.get());
    EXPECT_THAT(js, HasSubstr(no_content));
  }

  {  // Test empty content.
    std::unique_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    (*(article_proto->add_pages())).set_html("");
    std::string js = viewer::GetUnsafeArticleContentJs(article_proto.get());
    EXPECT_THAT(js, HasSubstr(no_content));
  }
}

}  // namespace web_distiller
