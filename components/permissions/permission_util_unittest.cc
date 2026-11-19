// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_util.h"

#include <memory>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace permissions {

// Due to dependency issues, `PermissionUtil::GetLastCommittedOriginAsURL()` is
// duplicated in content/browser/permissions/permission_util.cc. Keep these
// tests in sync with content/browser/permissions/permission_util_unittest.cc.
class PermissionUtilGetLastCommittedOriginAsURLTest
    : public content::RenderViewHostTestHarness {
 protected:
  // Navigates the primary main frame of `web_contents()` to `url`, served with
  // a `Content-Security-Policy: sandbox` header, so that the committed document
  // has an opaque origin whose precursor is the origin of `url`.
  content::RenderFrameHost* NavigateAndCommitSandboxed(const GURL& url) {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->SetResponseHeaders(net::HttpResponseHeaders::TryToCreate(
        "HTTP/1.1 200 OK\nContent-Security-Policy: sandbox\n\n"));
    navigation->Commit();
    return navigation->GetFinalRenderFrameHost();
  }

  content::RenderFrameHost* AppendChildAndNavigate(
      content::RenderFrameHost* parent,
      const GURL& url) {
    content::RenderFrameHost* child =
        content::RenderFrameHostTester::For(parent)->AppendChild("child");
    content::RenderFrameHostTester::For(child)->InitializeRenderFrameIfNeeded();
    return content::NavigationSimulator::NavigateAndCommitFromDocument(url,
                                                                       child);
  }
};

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, MainFrame) {
  NavigateAndCommit(GURL("https://example.com/path?query#ref"));

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(main_rfh()));
}

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, SameOriginSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  content::RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("https://example.com/child"));

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(child));
}

TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, CrossOriginSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  content::RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("https://other.com/child"));

  EXPECT_EQ(GURL("https://other.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(child));
}

// Opaque subframes never inherit their precursor origin.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, OpaqueSubframe) {
  NavigateAndCommit(GURL("https://example.com/"));
  content::RenderFrameHost* child =
      AppendChildAndNavigate(main_rfh(), GURL("data:text/html,child"));
  ASSERT_TRUE(child->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(child));
}

// A sandboxed primary main frame is mapped to its precursor origin.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, SandboxedMainFrame) {
  content::RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// A sandboxed primary main frame keeps using its own precursor origin while a
// navigation to another origin is pending.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameWithPendingNavigation) {
  content::RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  const GURL pending_url("https://other.com/");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      pending_url, web_contents());
  navigation->Start();
  ASSERT_EQ(pending_url, web_contents()->GetVisibleURL());
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// The initial empty document has an opaque origin without a precursor.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest, InitialEmptyDocument) {
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(main_rfh()));
}

// Regression test for https://crbug.com/571195939: the visible URL of a pending
// navigation must not be used as the origin of the still-committed opaque
// initial empty document.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       InitialEmptyDocumentWithPendingNavigation) {
  content::RenderFrameHost* rfh = main_rfh();
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  const GURL pending_url("https://example.com/");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      pending_url, web_contents());
  navigation->Start();
  ASSERT_EQ(pending_url, web_contents()->GetVisibleURL());
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// Regression test for https://crbug.com/576129749. A sandboxed page navigates
// itself to about:blank. The about:blank document inherits the page's opaque
// origin (precursor https://example.com) and its sandbox flags. It is still a
// primary main frame, but its origin was inherited, not derived from its own
// URL, so it must not use the permissions of https://example.com.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameNavigatedToAboutBlank) {
  content::RenderFrameHost* sandboxed_rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(sandboxed_rfh->GetLastCommittedOrigin().opaque());

  content::RenderFrameHost* rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(url::kAboutBlankURL), sandboxed_rfh);
  ASSERT_TRUE(rfh->IsInPrimaryMainFrame());
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());
  ASSERT_EQ(url::SchemeHostPort(GURL("https://example.com/")),
            rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// A same-document navigation keeps the document and its origin. A sandboxed
// page must keep using the permissions of its precursor afterwards.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       SandboxedMainFrameSameDocumentNavigation) {
  content::RenderFrameHost* rfh =
      NavigateAndCommitSandboxed(GURL("https://example.com/path"));
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());

  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/path#fragment"), rfh);
  navigation->CommitSameDocument();
  ASSERT_EQ(rfh, web_contents()->GetPrimaryMainFrame());

  EXPECT_EQ(GURL("https://example.com/"),
            PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

// An opaque primary main frame whose URL matches its precursor origin, but to
// which no sandbox restrictions apply, must not use the precursor's
// permissions.
TEST_F(PermissionUtilGetLastCommittedOriginAsURLTest,
       UnsandboxedOpaqueMainFrameIsNotAttributedToPrecursor) {
  // A failed navigation commits such a document.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/path"), web_contents());
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->CommitErrorPage();
  content::RenderFrameHost* rfh = navigation->GetFinalRenderFrameHost();
  ASSERT_TRUE(rfh->IsInPrimaryMainFrame());
  ASSERT_TRUE(rfh->GetLastCommittedOrigin().opaque());
  ASSERT_EQ(url::SchemeHostPort(GURL("https://example.com/")),
            rfh->GetLastCommittedOrigin().GetTupleOrPrecursorTupleIfOpaque());
  ASSERT_FALSE(rfh->IsSandboxed(network::mojom::WebSandboxFlags::kOrigin));

  EXPECT_EQ(GURL(), PermissionUtil::GetLastCommittedOriginAsURL(rfh));
}

}  // namespace permissions
