// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/unique_widget_ptr.h"

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
  InfoBarViewTest() {}

  InfoBarViewTest(const InfoBarViewTest&) = delete;
  InfoBarViewTest& operator=(const InfoBarViewTest&) = delete;

  ~InfoBarViewTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), GURL("about:blank"));
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(50, 50, 650, 650);
    widget_->Init(std::move(params));
    auto* root = widget_->GetRootView();

    // Accessible events will not fire until View is added to a RootView's tree.
    auto* added_container =
        root->AddChildView(std::make_unique<InfoBarContainerView>(nullptr));

    infobar_container_view_ = added_container;
    infobar_container_view_->ChangeInfoBarManager(infobar_manager());

    widget_->Show();
  }

  void TearDown() override {
    DetachContainer();
    infobar_container_view_ = nullptr;
    widget_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetWebContentsAt(0));
  }

  views::Widget::InitParams CreateParams(
      views::Widget::InitParams::Ownership ownership,
      views::Widget::InitParams::Type type) {
    views::Widget::InitParams params(ownership, type);
    params.context = GetContext();
    return params;
  }

  InfoBarContainerView* info_bar_container_view() {
    return infobar_container_view_;
  }

  // Detaches |infobar_container_view_| from infobar_manager(), so that newly-
  // created infobars will not be placed in a container.  This can be used to
  // simulate creating an infobar in a background tab.
  void DetachContainer() {
    infobar_container_view_->ChangeInfoBarManager(nullptr);
  }

 private:
  raw_ptr<InfoBarContainerView> infobar_container_view_ = nullptr;
  views::UniqueWidgetPtr widget_;
};

TEST_F(InfoBarViewTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  TestInfoBarDelegate::Create(infobar_manager());
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

TEST_F(InfoBarViewTest, AccessibleProperties) {
  // InfoBarView accessible properties test.
  InfoBarView* view = TestInfoBarDelegate::Create(infobar_manager());
  ui::AXNodeData data;

  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kAlertDialog, data.role);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR),
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));

  // InfoBarContainerView accessible properties test.
  auto* container = info_bar_container_view();

  data = ui::AXNodeData();
  container->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kGroup, data.role);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR_CONTAINER),
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}
