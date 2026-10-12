// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_util.h"

#include <memory>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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

}  // namespace permissions
