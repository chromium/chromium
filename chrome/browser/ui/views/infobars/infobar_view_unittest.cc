// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/views/test/ax_event_counter.h"

class TestInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  static InfoBarView* Create(infobars::ContentInfoBarManager* infobar_manager) {
    return static_cast<InfoBarView*>(
        infobar_manager->AddInfoBar(std::make_unique<InfoBarView>(
            std::make_unique<TestInfoBarDelegate>())));
  }

  // infobars::InfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
};

class InfoBarViewTest : public BrowserWithTestWindowTest {
 public:
  InfoBarViewTest() : infobar_container_view_(nullptr) {}

  InfoBarViewTest(const InfoBarViewTest&) = delete;
  InfoBarViewTest& operator=(const InfoBarViewTest&) = delete;

  ~InfoBarViewTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), GURL("about:blank"));
    infobar_container_view_.ChangeInfoBarManager(infobar_manager());
  }

  void TearDown() override {
    DetachContainer();
    BrowserWithTestWindowTest::TearDown();
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
  }

  // Detaches |infobar_container_view_| from infobar_manager(), so that newly-
  // created infobars will not be placed in a container.  This can be used to
  // simulate creating an infobar in a background tab.
  void DetachContainer() {
    infobar_container_view_.ChangeInfoBarManager(nullptr);
  }

 private:
  InfoBarContainerView infobar_container_view_;
};

TEST_F(InfoBarViewTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  TestInfoBarDelegate::Create(infobar_manager());
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}
