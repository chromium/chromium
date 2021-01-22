// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/views/test/ax_event_counter.h"

class TestInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  static InfoBarView* Create(InfoBarService* infobar_service) {
    return static_cast<InfoBarView*>(
        infobar_service->AddInfoBar(std::make_unique<InfoBarView>(
            std::make_unique<TestInfoBarDelegate>())));
  }

  // infobars::InfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
};

class InfoBarViewTest : public BrowserWithTestWindowTest {
 public:
  InfoBarViewTest() : infobar_container_view_(nullptr) {}
  ~InfoBarViewTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), GURL("about:blank"));
    infobar_container_view_.ChangeInfoBarManager(infobar_service());
  }

  void TearDown() override {
    DetachContainer();
    BrowserWithTestWindowTest::TearDown();
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
  }

  // Detaches |infobar_container_view_| from infobar_service(), so that newly-
  // created infobars will not be placed in a container.  This can be used to
  // simulate creating an infobar in a background tab.
  void DetachContainer() {
    infobar_container_view_.ChangeInfoBarManager(nullptr);
  }

 private:
  InfoBarContainerView infobar_container_view_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarViewTest);
};

TEST_F(InfoBarViewTest, GetDrawSeparator) {
  // Add multiple infobars.  The top infobar should not draw a separator; the
  // others should.
  for (int i = 0; i < 3; ++i) {
    InfoBarView* infobar = TestInfoBarDelegate::Create(infobar_service());
    ASSERT_TRUE(infobar);
    EXPECT_EQ(i > 0, infobar->GetDrawSeparator());
  }
}

// Regression test for crbug.com/834728 .
TEST_F(InfoBarViewTest, LayoutOnHiddenInfoBar) {
  // Calling Layout() on an infobar inside a container should not crash.
  InfoBarView* infobar = TestInfoBarDelegate::Create(infobar_service());
  ASSERT_TRUE(infobar);
  infobar->Layout();
  // Neither should calling it on an infobar not in a container.
  DetachContainer();
  infobar->Layout();
}

TEST_F(InfoBarViewTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  TestInfoBarDelegate::Create(infobar_service());
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}
