// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/view_utils.h"

namespace {

// Delegate for testing ConfirmInfoBar show link before button.
class CustomLayoutTestConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit CustomLayoutTestConfirmInfoBarDelegate(bool show_link_before_button)
      : show_link_before_button_(show_link_before_button) {}
  ~CustomLayoutTestConfirmInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return infobars::InfoBarDelegate::TEST_INFOBAR;
  }

  std::u16string GetMessageText() const override { return u"Test Message"; }
  std::u16string GetLinkText() const override { return u"Test Link"; }
  int GetButtons() const override { return BUTTON_OK; }

  // Enable the show link before button (to show link before button in infobar).
  bool ShouldShowLinkBeforeButton() const override {
    return show_link_before_button_;
  }

 private:
  const bool show_link_before_button_;
};

}  // namespace

class ConfirmInfobarCustomLayoutBrowserTest : public InProcessBrowserTest {
 protected:
  ConfirmInfobarCustomLayoutBrowserTest() = default;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  infobars::ContentInfoBarManager* GetInfoBarManager() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(web_contents);
    return infobars::ContentInfoBarManager::FromWebContents(web_contents);
  }

  void AddCustomLayoutTestConfirmInfobar(bool show_link_before_button) {
    infobars::ContentInfoBarManager* infobar_manager = GetInfoBarManager();
    ASSERT_TRUE(infobar_manager);
    auto delegate = std::make_unique<CustomLayoutTestConfirmInfoBarDelegate>(
        show_link_before_button);
    infobar_manager->AddInfoBar(
        std::make_unique<ConfirmInfoBar>(std::move(delegate)));
  }

  ConfirmInfoBar* GetActiveConfirmInfoBar() {
    infobars::ContentInfoBarManager* infobar_manager = GetInfoBarManager();
    EXPECT_TRUE(infobar_manager);
    EXPECT_EQ(1u, infobar_manager->infobars().size());
    return static_cast<ConfirmInfoBar*>(infobar_manager->infobars()[0]);
  }
};

IN_PROC_BROWSER_TEST_F(ConfirmInfobarCustomLayoutBrowserTest,
                       ShowsCustomLayoutWhenEnabled) {
  //  Add a ConfirmInfoBar with a delegate that enables show link before button
  //  (link before button).
  AddCustomLayoutTestConfirmInfobar(true);

  ConfirmInfoBar* info_bar = GetActiveConfirmInfoBar();
  ASSERT_TRUE(info_bar);
  // Check the layout.
  const ConfirmInfoBarDelegate* delegate = info_bar->GetDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_TRUE(delegate->ShouldShowLinkBeforeButton());

  views::Label* message_label = nullptr;
  views::Link* test_link = nullptr;
  views::MdTextButton* ok_button = info_bar->ok_button_for_testing();
  ASSERT_TRUE(ok_button);

  // Find the specific label and link views by their text content.
  for (views::View* child : info_bar->children()) {
    if (auto* link_view = views::AsViewClass<views::Link>(child)) {
      if (link_view->GetText() == u"Test Link") {
        test_link = link_view;
      }
    } else if (auto* label_view = views::AsViewClass<views::Label>(child)) {
      if (label_view->GetText() == u"Test Message") {
        message_label = label_view;
      }
    }
  }
  ASSERT_TRUE(message_label);
  ASSERT_TRUE(test_link);
  // Ensure the views have been laid out.
  info_bar->GetWidget()->LayoutRootViewIfNecessary();
  // Check relative horizontal positions: Label then Link then OK_Button.
  EXPECT_EQ(message_label->bounds().right(), test_link->bounds().x());
  EXPECT_LT(test_link->bounds().right(), ok_button->bounds().x());
}

IN_PROC_BROWSER_TEST_F(ConfirmInfobarCustomLayoutBrowserTest,
                       ShowsDefaultLayout) {
  // Add a ConfirmInfoBar with a delegate that uses the default
  // layout (link after buttons).
  AddCustomLayoutTestConfirmInfobar(false);

  ConfirmInfoBar* info_bar = GetActiveConfirmInfoBar();
  ASSERT_TRUE(info_bar);
  // Check the layout.
  const ConfirmInfoBarDelegate* delegate = info_bar->GetDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_FALSE(delegate->ShouldShowLinkBeforeButton());

  views::Label* message_label = nullptr;
  views::Link* test_link = nullptr;
  views::MdTextButton* ok_button = info_bar->ok_button_for_testing();
  ASSERT_TRUE(ok_button);

  for (views::View* child : info_bar->children()) {
    if (auto* link_view = views::AsViewClass<views::Link>(child)) {
      if (link_view->GetText() == u"Test Link") {
        test_link = link_view;
      }
    } else if (auto* label_view = views::AsViewClass<views::Label>(child)) {
      if (label_view->GetText() == u"Test Message") {
        message_label = label_view;
      }
    }
  }
  ASSERT_TRUE(message_label);
  ASSERT_TRUE(test_link);
  // Ensure the views have been laid out.
  info_bar->GetWidget()->LayoutRootViewIfNecessary();
  // Check relative horizontal positions: Label then OK_Button.
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  EXPECT_EQ(message_label->bounds().right() +
                layout_provider->GetDistanceMetric(
                    DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING),
            ok_button->bounds().x());
}
