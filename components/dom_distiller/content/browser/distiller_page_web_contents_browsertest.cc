// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test/test_util.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/viewer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::ContentBrowserTest;
using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

namespace {

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
    if (wait_for_document_loaded_ || !callback_)
      return;
    if (navigation_handle->HasCommitted() && navigation_handle->IsInMainFrame())
      std::move(callback_).Run();
  }

  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    if (wait_for_document_loaded_ && callback_) {
      if (!render_frame_host->GetParent())
        std::move(callback_).Run();
    }
  }

 private:
  base::OnceClosure callback_;
  bool wait_for_document_loaded_;
};

}  // namespace

namespace dom_distiller {

const char* kSimpleArticlePath = "/simple_article.html";
const char* kVideoArticlePath = "/video_article.html";

class DistillerPageWebContentsTest : public ContentBrowserTest {
 public:
  // ContentBrowserTest:
  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    AddComponentsResources();
    SetUpTestServer(embedded_test_server());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void DistillPage(base::OnceClosure quit_closure, const std::string& url) {
    distiller_page_->DistillPage(
        embedded_test_server()->GetURL(url),
        dom_distiller::proto::DomDistillerOptions(),
        base::BindOnce(
            &DistillerPageWebContentsTest::OnPageDistillationFinished,
            base::Unretained(this), std::move(quit_closure)));
  }

  void OnPageDistillationFinished(
      base::OnceClosure quit_closure,
      std::unique_ptr<proto::DomDistillerResult> distiller_result,
      bool distillation_successful) {
    distiller_result_ = std::move(distiller_result);
    std::move(quit_closure).Run();
  }

 protected:
  void RunUseCurrentWebContentsTest(const std::string& url,
                                    bool expect_new_web_contents,
                                    bool wait_for_document_loaded);

  DistillerPageWebContents* distiller_page_;
  std::unique_ptr<proto::DomDistillerResult> distiller_result_;
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
    ASSERT_EQ(true, expect_new_web_contents_);
    new_web_contents_created_ = true;
    DistillerPageWebContents::CreateNewWebContents(url);
  }

  bool new_web_contents_created() { return new_web_contents_created_; }

 private:
  bool expect_new_web_contents_;
  bool new_web_contents_created_;
};

#if defined(OS_WIN)
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
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              HasSubstr("Lorem ipsum"));
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              Not(HasSubstr("questionable content")));
  EXPECT_EQ("", distiller_result_->pagination_info().next_page());
  EXPECT_EQ("", distiller_result_->pagination_info().prev_page());
}

#if defined(OS_WIN)
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
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              ContainsRegex("href=\"http://127.0.0.1:.*/relativelink.html\""));
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              HasSubstr("href=\"http://www.google.com/absolutelink.html\""));
}

#if defined(OS_WIN)
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
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              ContainsRegex("src=\"http://127.0.0.1:.*/relativeimage.png\""));
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              HasSubstr("src=\"http://www.google.com/absoluteimage.png\""));
}

#if defined(OS_WIN)
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

  // A relative source/track should've been updated.
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              ContainsRegex("src=\"http://127.0.0.1:.*/relative_video.webm\""));
  EXPECT_THAT(
      distiller_result_->distilled_content().html(),
      ContainsRegex("src=\"http://127.0.0.1:.*/relative_track_en.vtt\""));
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              HasSubstr("src=\"http://www.google.com/absolute_video.ogg\""));
  EXPECT_THAT(distiller_result_->distilled_content().html(),
              HasSubstr("src=\"http://www.google.com/absolute_track_fr.vtt\""));
}

#if defined(OS_WIN)
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
    EXPECT_THAT(distiller_result_->distilled_content().html(),
                HasSubstr("Lorem ipsum"));
  }

  {
    base::RunLoop run_loop;
    DistillPage(run_loop.QuitClosure(), "/invisible_style.html");
    run_loop.Run();
    EXPECT_THAT(distiller_result_->distilled_content().html(),
                Not(HasSubstr("Lorem ipsum")));
  }
}

#if defined(OS_WIN)
#define MAYBE_UsingCurrentWebContentsWrongUrl \
  DISABLED_UsingCurrentWebContentsWrongUrl
#else
#define MAYBE_UsingCurrentWebContentsWrongUrl UsingCurrentWebContentsWrongUrl
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       MAYBE_UsingCurrentWebContentsWrongUrl) {
  std::string url("/bogus");
  bool expect_new_web_contents = true;
  bool wait_for_document_loaded = true;
  RunUseCurrentWebContentsTest(url, expect_new_web_contents,
                               wait_for_document_loaded);
}

#if defined(OS_WIN)
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
}

#if defined(OS_WIN)
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
  EXPECT_EQ("Test Page Title", distiller_result_->title());
}

#if defined(OS_WIN)
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
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(2));

  run_loop.Run();
}

#if defined(OS_WIN)
#define MAYBE_MarkupInfo DISABLED_MarkupInfo
#else
#define MAYBE_MarkupInfo MarkupInfo
#endif
IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, MAYBE_MarkupInfo) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      std::unique_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/markup_article.html");
  run_loop.Run();

  EXPECT_THAT(distiller_result_->distilled_content().html(),
              HasSubstr("Lorem ipsum"));
  EXPECT_EQ("Marked-up Markup Test Page Title", distiller_result_->title());

  const proto::MarkupInfo markup_info = distiller_result_->markup_info();
  EXPECT_EQ("Marked-up Markup Test Page Title", markup_info.title());
  EXPECT_EQ("Article", markup_info.type());
  EXPECT_EQ("http://test/markup.html", markup_info.url());
  EXPECT_EQ("This page tests Markup Info.", markup_info.description());
  EXPECT_EQ("Whoever Published", markup_info.publisher());
  EXPECT_EQ("Copyright 2000-2014 Whoever Copyrighted", markup_info.copyright());
  EXPECT_EQ("Whoever Authored", markup_info.author());

  const proto::MarkupArticle markup_article = markup_info.article();
  EXPECT_EQ("Whatever Section", markup_article.section());
  EXPECT_EQ("July 23, 2014", markup_article.published_time());
  EXPECT_EQ("2014-07-23T23:59", markup_article.modified_time());
  EXPECT_EQ("", markup_article.expiration_time());
  ASSERT_EQ(1, markup_article.authors_size());
  EXPECT_EQ("Whoever Authored", markup_article.authors(0));

  ASSERT_EQ(2, markup_info.images_size());

  const proto::MarkupImage markup_image1 = markup_info.images(0);
  EXPECT_EQ("http://test/markup1.jpeg", markup_image1.url());
  EXPECT_EQ("https://test/markup1.jpeg", markup_image1.secure_url());
  EXPECT_EQ("jpeg", markup_image1.type());
  EXPECT_EQ("", markup_image1.caption());
  EXPECT_EQ(600, markup_image1.width());
  EXPECT_EQ(400, markup_image1.height());

  const proto::MarkupImage markup_image2 = markup_info.images(1);
  EXPECT_EQ("http://test/markup2.gif", markup_image2.url());
  EXPECT_EQ("https://test/markup2.gif", markup_image2.secure_url());
  EXPECT_EQ("gif", markup_image2.type());
  EXPECT_EQ("", markup_image2.caption());
  EXPECT_EQ(1000, markup_image2.width());
  EXPECT_EQ(600, markup_image2.height());
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       TestNoContentDoesNotCrash) {
  const std::string no_content =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);

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

}  // namespace dom_distiller
