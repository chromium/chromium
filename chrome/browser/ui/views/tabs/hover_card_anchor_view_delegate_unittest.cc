// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hover_card_anchor_view_delegate.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockHoverCardAnchorViewDelegateObserver
    : public HoverCardAnchorViewDelegate::Observer {
 public:
  ~MockHoverCardAnchorViewDelegateObserver() override = default;

  MOCK_METHOD(void, OnAnchorViewRemoved, (), (override));
};

class HoverCardAnchorViewDelegateTest : public TestWithBrowserView {
 public:
  HoverCardAnchorViewDelegateTest() = default;

  void SetUp() override {
    TestWithBrowserView::SetUp();
    observer_ = std::make_unique<MockHoverCardAnchorViewDelegateObserver>();
  }

  void TearDown() override {
    if (delegate_) {
      delegate_.reset();
    }
    TestWithBrowserView::TearDown();
  }

  void CreateDelegate(views::View* view) {
    delegate_ =
        std::make_unique<HoverCardAnchorViewDelegate>(observer_.get(), view);
  }

  HoverCardAnchorViewDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<MockHoverCardAnchorViewDelegateObserver> observer_;
  std::unique_ptr<HoverCardAnchorViewDelegate> delegate_;
};

TEST_F(HoverCardAnchorViewDelegateTest, CrashesOnNullInput) {
  EXPECT_DEATH(CreateDelegate(nullptr), "");
}

TEST_F(HoverCardAnchorViewDelegateTest, ProvidesAccessToATab) {
  AddTab(browser_view()->browser(), GURL("http://foo1.com"));
  Tab* const tab = browser_view()->tabstrip()->tab_at(0);
  CreateDelegate(tab);

  EXPECT_TRUE(delegate()->HasValidTargetView(browser_view()->tabstrip()));
  EXPECT_TRUE(delegate()->HasTab());
  EXPECT_EQ(delegate()->GetAsTab(), tab);
  EXPECT_TRUE(delegate()->IsObserving());
}
