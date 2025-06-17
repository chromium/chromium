// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"

class TestInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  static InfoBarView* Create(infobars::ContentInfoBarManager* infobar_manager) {
    return infobar_manager->AddInfoBar(
        std::make_unique<InfoBarView>(std::make_unique<TestInfoBarDelegate>()));
  }

  // infobars::InfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
};

class TestInfoBarViewWithLabel : public InfoBarView {
 public:
  TestInfoBarViewWithLabel()
      : InfoBarView(std::make_unique<TestInfoBarDelegate>()) {
    test_label_ = AddContentChildView(CreateLabel(u"Test Label"));
  }

  views::Label* test_label() { return test_label_; }

 private:
  raw_ptr<views::Label> test_label_ = nullptr;
};

class InfoBarViewBrowserTest : public InProcessBrowserTest {
 protected:
  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  InfoBarContainerView* info_bar_container_view() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->infobar_container();
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarViewBrowserTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  TestInfoBarDelegate::Create(infobar_manager());
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

IN_PROC_BROWSER_TEST_F(InfoBarViewBrowserTest, AccessibleProperties) {
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

IN_PROC_BROWSER_TEST_F(InfoBarViewBrowserTest,
                       ThemeAppliesToContentContainerLabels) {
  auto* infobar = infobar_manager()->AddInfoBar(
      std::make_unique<TestInfoBarViewWithLabel>());
  ASSERT_NE(infobar, nullptr);

  const ui::ColorProvider* color_provider = infobar->GetColorProvider();
  ASSERT_TRUE(color_provider);

  const SkColor expected_bg = color_provider->GetColor(kColorInfoBarBackground);
  const SkColor expected_fg = color_provider->GetColor(kColorInfoBarForeground);

  infobar->OnThemeChanged();

  views::Label* label = infobar->test_label();
  ASSERT_NE(label, nullptr);

  EXPECT_EQ(expected_bg, label->GetBackgroundColor());
  EXPECT_EQ(expected_fg, label->GetEnabledColor());
  EXPECT_FALSE(label->GetAutoColorReadabilityEnabled());
}
