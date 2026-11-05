// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/browser/script_tools_ready_waiter.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace page_content_annotations {

namespace {

using Result = ScriptToolsReadyWaiter::Result;

class ScriptToolsReadyWaiterBrowserTest : public content::ContentBrowserTest {
 public:
  ScriptToolsReadyWaiterBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kWebMCP, blink::features::kWebMCPWaitForToolsUntil},
        {});
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    // 127.0.0.1 is a secure context, as `document.modelContext` requires.
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(), embedded_test_server()->GetURL("/title1.html")));
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  // Starts a waiter whose callback deletes the waiter before recording the
  // result, exercising the self-delete contract.
  void StartWaiter(bool include_ad_frames) {
    waiter_ = std::make_unique<ScriptToolsReadyWaiter>(
        web_contents(), base::BindLambdaForTesting([&](Result r) {
          waiter_.reset();
          result_.SetValue(r);
        }),
        include_ad_frames);
  }

  // Adds an iframe with `id`, loading `src` if it isn't empty, and returns its
  // RenderFrameHost.
  content::RenderFrameHost* AddChildFrame(const std::string& src,
                                          const std::string& id,
                                          size_t child_index) {
    EXPECT_TRUE(content::ExecJs(web_contents(), content::JsReplace(R"(
        new Promise(resolve => {
          const iframe = document.createElement('iframe');
          iframe.id = $1;
          if ($2) {
            iframe.src = $2;
            iframe.onload = resolve;
          }
          document.body.appendChild(iframe);
          if (!$2) {
            resolve();
          }
        }))",
                                                                   id, src)));
    return content::ChildFrameAt(web_contents(), child_index);
  }

  // Puts `frame`'s document in the tools not ready state until
  // `window.resolveToolsReady()` is called in it.
  [[nodiscard]] bool MarkScriptToolsNotReady(
      const content::ToRenderFrameHost& frame) {
    content::RenderFrameHost* rfh = frame.render_frame_host();
    return content::ExecJs(rfh, R"(
        window.toolsReady = new Promise(resolve => {
          window.resolveToolsReady = resolve;
        });
        document.modelContext.waitForToolsUntil(window.toolsReady);
    )") && base::test::RunUntil([&]() {
             return rfh->GetScriptToolsReadyState() ==
                    content::RenderFrameHost::ScriptToolsReadyState::kWaiting;
           });
  }

 protected:
  std::unique_ptr<ScriptToolsReadyWaiter> waiter_;
  base::test::TestFuture<Result> result_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ScriptToolsReadyWaiterBrowserTest, AlreadyReady) {
  EXPECT_TRUE(ScriptToolsReadyWaiter::AreScriptToolsReady(
      *web_contents(), /*include_ad_frames=*/true));

  StartWaiter(/*include_ad_frames=*/true);
  // The callback is run asynchronously even when the tools are already ready.
  EXPECT_FALSE(result_.IsReady());
  EXPECT_EQ(result_.Get(), Result::kReady);
}

// Ready once every frame is ready or the frames that aren't are removed.
IN_PROC_BROWSER_TEST_F(ScriptToolsReadyWaiterBrowserTest,
                       ReadyOnceEveryFrameIs) {
  const std::string child_url =
      embedded_test_server()->GetURL("/title1.html").spec();
  content::RenderFrameHost* child1 =
      AddChildFrame(child_url, "child1", /*child_index=*/0);
  content::RenderFrameHost* child2 =
      AddChildFrame(child_url, "child2", /*child_index=*/1);
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);
  child1->UpdateToAdFrame();
  ASSERT_TRUE(MarkScriptToolsNotReady(web_contents()));
  ASSERT_TRUE(MarkScriptToolsNotReady(child1));
  ASSERT_TRUE(MarkScriptToolsNotReady(child2));
  StartWaiter(/*include_ad_frames=*/true);

  // Removing `child2` leaves the main frame and `child1` not ready.
  content::RenderFrameDeletedObserver child2_deleted(child2);
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('child2').remove()"));
  child2_deleted.WaitUntilDeleted();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result_.IsReady());

  // Resolving the main frame still leaves `child1` (an ad frame) not ready.
  ASSERT_TRUE(content::ExecJs(web_contents(), "window.resolveToolsReady()"));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return web_contents()->GetPrimaryMainFrame()->GetScriptToolsReadyState() ==
           content::RenderFrameHost::ScriptToolsReadyState::kReady;
  }));
  EXPECT_FALSE(ScriptToolsReadyWaiter::AreScriptToolsReady(
      *web_contents(), /*include_ad_frames=*/true));
  EXPECT_TRUE(ScriptToolsReadyWaiter::AreScriptToolsReady(
      *web_contents(), /*include_ad_frames=*/false));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result_.IsReady());

  // Resolving `child1` makes every remaining frame ready.
  ASSERT_TRUE(content::ExecJs(child1, "window.resolveToolsReady()"));
  EXPECT_EQ(result_.Get(), Result::kReady);
}

// Navigating a frame whose tools aren't ready picks up the new document's
// ready state.
IN_PROC_BROWSER_TEST_F(ScriptToolsReadyWaiterBrowserTest,
                       SubframeNavigatesWhileWaiting) {
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");
  content::RenderFrameHost* child =
      AddChildFrame(url1.spec(), "child", /*child_index=*/0);
  ASSERT_TRUE(child);
  ASSERT_TRUE(MarkScriptToolsNotReady(child));
  StartWaiter(/*include_ad_frames=*/true);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result_.IsReady());

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(), "child", url2));
  EXPECT_EQ(result_.Get(), Result::kReady);
}

// A frame whose renderer is gone doesn't hold up the page.
IN_PROC_BROWSER_TEST_F(ScriptToolsReadyWaiterBrowserTest,
                       ReadyOnceRendererIsGone) {
  ASSERT_TRUE(MarkScriptToolsNotReady(web_contents()));
  StartWaiter(/*include_ad_frames=*/true);

  content::ScopedAllowRendererCrashes allow_renderer_crashes(web_contents());
  content::RenderProcessHost* process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_EQ(result_.Get(), Result::kReady);
}

IN_PROC_BROWSER_TEST_F(ScriptToolsReadyWaiterBrowserTest, PrimaryPageChanged) {
  ASSERT_TRUE(MarkScriptToolsNotReady(web_contents()));
  StartWaiter(/*include_ad_frames=*/true);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/title2.html")));
  EXPECT_EQ(result_.Get(), Result::kPrimaryPageChanged);
}

}  // namespace

}  // namespace page_content_annotations
