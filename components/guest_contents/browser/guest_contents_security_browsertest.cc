// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "components/guest_contents/browser/guest_contents_host_impl.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "content/public/browser/unowned_inner_web_contents_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class GuestContentsSecurityBrowsertest : public content::ContentBrowserTest {
 public:
  GuestContentsSecurityBrowsertest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAttachUnownedInnerWebContents);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_https_test_server().SetCertHostnames(
        {"outer.com", "inner.com", "attacker.com", "a.com", "b.com"});
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data/guest_contents");

    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override { inner_webcontents_.reset(); }

  content::WebContents* GetInnerWebContents() {
    if (!inner_webcontents_) {
      SetUpEmbeddedWebContents();
    }
    return inner_webcontents_.get();
  }

  content::WebContents* GetOuterWebContents() {
    return GetInnerWebContents()->GetOuterWebContents();
  }

 private:
  // Helper function to set up embedded web contents for tests.
  void SetUpEmbeddedWebContents() {
    const GURL outer_url(
        embedded_https_test_server().GetURL("outer.com", "/iframe.html"));
    const GURL inner_url(
        embedded_https_test_server().GetURL("inner.com", "/inner.html"));

    // Navigate to iframe.html
    ASSERT_TRUE(NavigateToURL(shell(), outer_url));
    content::WebContents* main_web_contents = shell()->web_contents();
    ASSERT_TRUE(main_web_contents);

    // Wait for the page to load and iframe to be ready.
    EXPECT_TRUE(content::WaitForLoadStop(main_web_contents));

    // Setup inner WebContents and navigate it to inner.html.
    content::WebContents::CreateParams inner_params(
        shell()->web_contents()->GetBrowserContext());
    inner_webcontents_ = content::WebContents::Create(inner_params);
    content::NavigationController::LoadURLParams load_params(inner_url);
    inner_webcontents_->GetController().LoadURLWithParams(load_params);

    // Wait for guest content to load.
    EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents_.get()));

    guest_contents::GuestContentsHandle* guest_handle =
        guest_contents::GuestContentsHandle::CreateForWebContents(
            inner_webcontents_.get());
    ASSERT_TRUE(guest_handle);

    mojo::Remote<guest_contents::mojom::GuestContentsHost> guest_host_remote;
    guest_contents::GuestContentsHostImpl::Create(
        main_web_contents, guest_host_remote.BindNewPipeAndPassReceiver());

    // Get the iframe's frame token and guest_id for the attach call.
    blink::LocalFrameToken iframe_token =
        content::ChildFrameAt(main_web_contents, 0)->GetFrameToken();
    guest_contents::GuestId guest_id = guest_handle->id();

    // Perform the attach.
    base::RunLoop attach_loop;
    guest_host_remote->Attach(iframe_token, guest_id,
                              base::BindLambdaForTesting([&](bool result) {
                                EXPECT_TRUE(result);
                                attach_loop.Quit();
                              }));
    attach_loop.Run();

    // Verify that the connection is established.
    EXPECT_EQ(main_web_contents, inner_webcontents_->GetOuterWebContents());
  }

  std::unique_ptr<content::WebContents> inner_webcontents_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that parent history is not affected by embedded navigation.
// The history.length should be independent between inner and outer
// webcontents.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       HistoryLengthIndependent) {
  content::WebContents* outer_webcontents = GetOuterWebContents();
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_FALSE(outer_webcontents->GetOuterWebContents());
  EXPECT_TRUE(outer_webcontents);
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.history.length"));

  // Navigate the inner contents another cross origin URL and verify outer
  // remains 1.
  GURL url = embedded_https_test_server().GetURL("b.com", "/simple.html");
  EXPECT_TRUE(NavigateToURL(inner_webcontents, url));
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.history.length"));
  EXPECT_EQ(2, EvalJs(inner_webcontents, "window.history.length"));
}

// Test that the frame tree isolation between inner and outer webcontents.
// The outer webcontents is able to see the GuestContents but not the other
// way around.
// NOTE: This is a known security issue.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest, WindowFramesLength) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_EQ(0, EvalJs(inner_webcontents, "window.frames.length"));
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.frames.length"));
}

// Test whether the parent window counts the embedded content as a frame.
// NOTE: This is a known security issue.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowLengthIndependent) {
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.length"));
}

// Test that the embedded content acts as top level. window.top in the
// embedded content should equal window (itself), not the actual parent's
// top-level window.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest, WindowTopIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(EvalJs(inner_webcontents, "window.top === window").ExtractBool());
}

// Test that the embedded content acts as top level.
// window.opener should be null since the embedded content should not
// have access to the parent that "opened" it.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowOpenerIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(
      EvalJs(inner_webcontents, "window.opener === null").ExtractBool());
}

// Test that the embedded content acts as top level.
// window.parent should equal window (itself) since there should be
// no accessible parent window from the embedded content's perspective.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowParentIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(
      EvalJs(inner_webcontents, "window.parent === window").ExtractBool());
}

// Test that the embedded content acts as top level.
// window.frameElement should be null since the embedded content should
// not appear to be contained within a frame element.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowFrameElementIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(
      EvalJs(inner_webcontents, "window.frameElement === null").ExtractBool());
}

// Test that inner webcontents cannot target outer webcontents
// _parent and _top should all target the inner webcontents itself.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowOpenTargetingIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // Store current URLs to verify navigation targets.
  GURL outer_url = outer_webcontents->GetLastCommittedURL();

  // Test _parent targeting from inner webcontents.
  GURL test_url =
      embedded_https_test_server().GetURL("b.com", "/defaultresponse");
  EXPECT_TRUE(
      ExecJs(inner_webcontents,
             content::JsReplace("window.open($1, '_parent')", test_url)));

  // Verify inner webcontents navigated, outer did not.
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));
  EXPECT_EQ(inner_webcontents->GetLastCommittedURL().host(), test_url.host());
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), outer_url);

  // Test _top also just navigates inner.
  test_url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
  EXPECT_TRUE(ExecJs(inner_webcontents,
                     content::JsReplace("window.open($1, '_top')", test_url)));

  // Verify inner webcontents navigated, outer did not.
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));
  EXPECT_EQ(inner_webcontents->GetLastCommittedURL().host(), test_url.host());
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), outer_url);
}

// Test that cross-context window references are not useful.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowOpenReferenceIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // Part 1. outer makes a window, it's not accessible in inner.
  EXPECT_TRUE(ExecJs(outer_webcontents,
                     "window.testWindow = window.open('about:blank')"));
  EXPECT_TRUE(EvalJs(outer_webcontents, "window.hasOwnProperty('testWindow')")
                  .ExtractBool());
  EXPECT_FALSE(EvalJs(inner_webcontents, "window.hasOwnProperty('testWindow')")
                   .ExtractBool());

  // Part 2. inner makes a window, it's not accessible in outer.
  EXPECT_TRUE(ExecJs(inner_webcontents,
                     "window.innerWindow = window.open('about:blank')"));
  EXPECT_FALSE(EvalJs(outer_webcontents, "window.hasOwnProperty('innerWindow')")
                   .ExtractBool());
}

// Array accessor on the outer window is able to access inner window.
// NOTE: This is a known security issue.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       WindowIndexedAccessor) {
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_FALSE(
      EvalJs(outer_webcontents, "window[0] === undefined").ExtractBool());
}

// Test postMessage from outer to inner. The expectation is that communication
// in either direction is not allowed. That holds true here in content_shell.
// However, this test fails in a webium environment. See crbug.com/452082277
// for more information.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       OuterToInnerPostMessage) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // 1. Prepare the inner to receive postMessage and mark receipt.
  EXPECT_TRUE(ExecJs(inner_webcontents,
                     "window.addEventListener('message', (event) => { "
                     "window.postMessageReceived = true; "
                     "});"));

  // 2. PostMessage from outer.
  EXPECT_TRUE(ExecJs(outer_webcontents,
                     "var iframe = document.querySelector('#iframe');"));
  EXPECT_TRUE(ExecJs(outer_webcontents,
                     "iframe.contentWindow.postMessage('test', '*');"));

  // 3. Verify inner did not receive the postMessage.
  EXPECT_FALSE(
      EvalJs(inner_webcontents, "window.hasOwnProperty('postMessageReceived')")
          .ExtractBool());
}

// Test PostMessage to '*' from outer does not affect inner.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       OuterToInnerStarPostMessage) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // 1. Prepare the inner to receive postMessage and mark receipt.
  EXPECT_TRUE(ExecJs(inner_webcontents,
                     "window.addEventListener('message', (event) => { "
                     "window.postMessageReceived = true; "
                     "});"));

  // 2. PostMessage from outer.
  EXPECT_TRUE(ExecJs(outer_webcontents, "window.postMessage('test', '*');"));

  // 3. Verify inner did not receive the postMessage.
  EXPECT_FALSE(
      EvalJs(inner_webcontents, "window.hasOwnProperty('postMessageReceived')")
          .ExtractBool());
}

// Test PostMessage to '*' from inner does not affect outer.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       InnerToOuterStarPostMessage) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // 1. Prepare the outer to receive postMessage and mark receipt.
  EXPECT_TRUE(ExecJs(outer_webcontents,
                     "window.addEventListener('message', (event) => { "
                     "window.postMessageReceived = true; "
                     "});"));

  // 2. PostMessage from inner.
  EXPECT_TRUE(ExecJs(inner_webcontents, "window.postMessage('test', '*');"));

  // 3. Verify outer did not receive the postMessage.
  EXPECT_FALSE(
      EvalJs(outer_webcontents, "window.hasOwnProperty('postMessageReceived')")
          .ExtractBool());
}

// Test that the outer has no perception into nested iframes inside the guest
// contents.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest, NestedIframeInGuest) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // Navigate to same-origin as in non-guest-contents scenarios, reach down
  // into a nested iframe is possible. The outer is navigated to the outer.com
  // host.
  GURL url = embedded_https_test_server().GetURL("outer.com",
                                                 "/inner_with_iframe.html");
  EXPECT_TRUE(NavigateToURL(inner_webcontents, url));
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));

  // The embedder should be able to see the guest contents via window.frames.
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.frames.length"));
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.length"));

  // window[0] should point at the top iframe. We should not be able to access
  // "window" or "frame" inside of it. Attempting to do so throws a
  // SecurityError.
  EXPECT_TRUE(EvalJs(outer_webcontents,
                     "try { window[0].window[0] } catch (e) { e.name === "
                     "'SecurityError' }")
                  .ExtractBool());
  EXPECT_TRUE(EvalJs(outer_webcontents,
                     "try { window[0].frames[0] } catch (e) { e.name === "
                     "'SecurityError' }")
                  .ExtractBool());

  // window[0] refers to the guest contents, [1] should not refer to anything.
  EXPECT_TRUE(
      EvalJs(outer_webcontents, "window[1] === undefined").ExtractBool());
}

// Test HTMLIFrameElement's contentWindow and contentDocument accessibility.
// NOTE: This is a known security issue.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest, HTMLIFrameElement) {
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_TRUE(ExecJs(outer_webcontents,
                     "var iframe = document.querySelector('#iframe');"));
  EXPECT_FALSE(
      EvalJs(outer_webcontents, "iframe.contentWindow === null").ExtractBool());
  EXPECT_TRUE(EvalJs(outer_webcontents, "iframe.contentDocument === null")
                  .ExtractBool());
}

// Test that a link with target="_parent" or target="_top" from the inner
// webcontents does not navigate the outer webcontents.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       NavigateParentWithLink) {
  content::WebContents* outer_webcontents = GetOuterWebContents();
  content::WebContents* inner_webcontents = GetInnerWebContents();

  // Store the outer's current URL to verify it doesn't change.
  GURL outer_url = outer_webcontents->GetLastCommittedURL();
  GURL inner_url = inner_webcontents->GetLastCommittedURL();

  // Test 1: Create a link with target="_parent" and click it.
  GURL target_url =
      embedded_https_test_server().GetURL("attacker.com", "/simple.html");
  EXPECT_TRUE(
      ExecJs(inner_webcontents,
             content::JsReplace("const link = document.createElement('a');"
                                "link.href = $1;"
                                "link.target = '_parent';"
                                "link.id = 'testLink';"
                                "link.textContent = 'Click me';"
                                "document.body.appendChild(link);"
                                "link.click();",
                                target_url)));
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));

  // Verify the inner webcontents navigated to the target URL.
  EXPECT_EQ(inner_webcontents->GetLastCommittedURL(), target_url);

  // Verify the outer webcontents did NOT navigate.
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), outer_url);

  // Navigate inner back to original URL for next test.
  EXPECT_TRUE(NavigateToURL(inner_webcontents, inner_url));

  // Test 2: Create a link with target="_top" and click it.
  EXPECT_TRUE(
      ExecJs(inner_webcontents,
             content::JsReplace("const link2 = document.createElement('a');"
                                "link2.href = $1;"
                                "link2.target = '_top';"
                                "link2.textContent = 'Click me too';"
                                "document.body.appendChild(link2);"
                                "link2.click();",
                                target_url)));
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));

  // Verify the inner webcontents navigated to the target URL.
  EXPECT_EQ(inner_webcontents->GetLastCommittedURL(), target_url);

  // Verify the outer webcontents still did NOT navigate.
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), outer_url);
}

// Test that document.referrer in the inner webcontents does not leak the
// embedder's URL.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       DocumentReferrerIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  // The inner webcontents should not have a referrer since it acts as a
  // top-level browsing context, not as an embedded frame.
  EXPECT_EQ("", EvalJs(inner_webcontents, "document.referrer"));
}

// Navigating the outer should always navigate the outer.
IN_PROC_BROWSER_TEST_F(GuestContentsSecurityBrowsertest,
                       NavigateOuterWithWindowLocation) {
  content::WebContents* outer_webcontents = GetOuterWebContents();

  GURL url = embedded_https_test_server().GetURL("outer.com", "/simple.html");
  EXPECT_TRUE(ExecJs(outer_webcontents,
                     content::JsReplace("window.location.href = $1", url)));
  EXPECT_TRUE(content::WaitForLoadStop(outer_webcontents));
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), url);
}
