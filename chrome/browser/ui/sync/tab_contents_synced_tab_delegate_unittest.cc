// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestSyncedTabDelegate : public TabContentsSyncedTabDelegate {
 public:
  explicit TestSyncedTabDelegate(content::WebContents* web_contents) {
    SetWebContents(web_contents);
  }

  ~TestSyncedTabDelegate() override {}

  SessionID GetWindowId() const override { return SessionID::InvalidValue(); }
  SessionID GetSessionId() const override { return SessionID::InvalidValue(); }
  bool IsPlaceholderTab() const override { return false; }
};

class TabContentsSyncedTabDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TabContentsSyncedTabDelegateTest() : ChromeRenderViewHostTestHarness() {}
  ~TabContentsSyncedTabDelegateTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    NavigateAndCommit(GURL("about:blank"));
  }
};

TEST_F(TabContentsSyncedTabDelegateTest, InvalidEntryIndexReturnsDefault) {
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  TestSyncedTabDelegate delegate(web_contents.get());

  // -1 and 2 are invalid indices because there's only one navigation
  // recorded(the initial one to "about:blank")
  EXPECT_EQ(delegate.GetFaviconURLAtIndex(-1), GURL());
  EXPECT_EQ(delegate.GetFaviconURLAtIndex(2), GURL());

  EXPECT_TRUE(
      PageTransitionCoreTypeIs(delegate.GetTransitionAtIndex(-1),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(delegate.GetTransitionAtIndex(2),
                               ui::PageTransition::PAGE_TRANSITION_LINK));
}

}  // namespace
