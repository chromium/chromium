// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/infobars/core/infobar.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

const base::string16 kSharedTabName = base::UTF8ToUTF16("example.com");
const base::string16 kAppName = base::UTF8ToUTF16("sharing.com");

class MockTabSharingUIViews : public TabSharingUI {
 public:
  MockTabSharingUIViews() {}
  MOCK_METHOD1(StartSharing, void(infobars::InfoBar* infobar));
  MOCK_METHOD0(StopSharing, void());

  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override {
    return 0;
  }
};

}  // namespace

class TabSharingInfoBarDelegateTest : public BrowserWithTestWindowTest {
 public:
  TabSharingInfoBarDelegateTest() {}

  infobars::InfoBar* CreateInfobar(base::string16 shared_tab_name,
                                   base::string16 app_name,
                                   bool is_sharing_allowed,
                                   int tab_index = 0) {
    return TabSharingInfoBarDelegate::Create(
        InfoBarService::FromWebContents(
            browser()->tab_strip_model()->GetWebContentsAt(tab_index)),
        shared_tab_name, app_name, is_sharing_allowed, tab_sharing_mock_ui());
  }

  ConfirmInfoBarDelegate* CreateDelegate(base::string16 shared_tab_name,
                                         base::string16 app_name,
                                         bool is_sharing_allowed,
                                         int tab_index = 0) {
    infobars::InfoBar* infobar =
        CreateInfobar(shared_tab_name, app_name, is_sharing_allowed, tab_index);
    return static_cast<ConfirmInfoBarDelegate*>(infobar->delegate());
  }

  MockTabSharingUIViews* tab_sharing_mock_ui() { return &mock_ui; }

 private:
  MockTabSharingUIViews mock_ui;
};

TEST_F(TabSharingInfoBarDelegateTest, StartSharingOnCancel) {
  AddTab(browser(), GURL("about:blank"));
  infobars::InfoBar* infobar = CreateInfobar(kSharedTabName, kAppName, true);
  ConfirmInfoBarDelegate* delegate =
      static_cast<ConfirmInfoBarDelegate*>(infobar->delegate());
  EXPECT_CALL(*tab_sharing_mock_ui(), StartSharing(infobar)).Times(1);
  EXPECT_FALSE(delegate->Cancel());
}

TEST_F(TabSharingInfoBarDelegateTest, StopSharingOnAccept) {
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* delegate =
      CreateDelegate(kSharedTabName, kAppName, true);
  EXPECT_CALL(*tab_sharing_mock_ui(), StopSharing).Times(1);
  EXPECT_FALSE(delegate->Accept());
}

// Test that the infobar on the shared tab has the correct layout:
// "|icon| Sharing this tab to |app| [Stop]"
TEST_F(TabSharingInfoBarDelegateTest, InfobarOnSharedTab) {
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* delegate =
      CreateDelegate(base::string16(), kAppName, true);
  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, kAppName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON));
  EXPECT_FALSE(delegate->IsCloseable());
}

// Test that the infobar on another not share tab has the correct layout:
// "|icon| Sharing |shared_tab| to |app| [Stop] [Share this tab instead]"
TEST_F(TabSharingInfoBarDelegateTest, InfobarOnNotSharedTab) {
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* delegate =
      CreateDelegate(kSharedTabName, kAppName, true);
  EXPECT_STREQ(delegate->GetVectorIcon().name,
               vector_icons::kScreenShareIcon.name);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringFUTF16(
                IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                kSharedTabName, kAppName));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK |
                                        ConfirmInfoBarDelegate::BUTTON_CANCEL);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON));
  EXPECT_FALSE(delegate->IsCloseable());
}

// Test that when sharing is not allowed, the infobar only has one button (the
// "Stop" button) on both shared and not shared tabs.
TEST_F(TabSharingInfoBarDelegateTest, InfobarWhenSharingNotAllowed) {
  // Create infobar for shared tab.
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* delegate_shared_tab = CreateDelegate(
      base::string16(), kAppName, false /* is_sharing_allowed */, 0);
  EXPECT_EQ(delegate_shared_tab->GetButtons(),
            ConfirmInfoBarDelegate::BUTTON_OK);

  // Create infobar for another not shared tab.
  AddTab(browser(), GURL("about:blank"));
  ConfirmInfoBarDelegate* delegate = CreateDelegate(
      kSharedTabName, kAppName, false /* is_sharing_allowed */, 1);
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
}

// Test that multiple infobars can be created on the same tab.
TEST_F(TabSharingInfoBarDelegateTest, MultipleInfobarsOnSameTab) {
  AddTab(browser(), GURL("about:blank"));
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_EQ(infobar_service->infobar_count(), 0u);
  CreateInfobar(kSharedTabName, kAppName, true);
  EXPECT_EQ(infobar_service->infobar_count(), 1u);
  CreateInfobar(kSharedTabName, kAppName, true);
  EXPECT_EQ(infobar_service->infobar_count(), 2u);
}

TEST_F(TabSharingInfoBarDelegateTest, InfobarNotDismissedOnNavigation) {
  AddTab(browser(), GURL("http://foo"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  CreateInfobar(kSharedTabName, kAppName, true);
  EXPECT_EQ(infobar_service->infobar_count(), 1u);
  content::NavigationController* controller = &web_contents->GetController();
  NavigateAndCommit(controller, GURL("http://bar"));
  EXPECT_EQ(infobar_service->infobar_count(), 1u);
}
