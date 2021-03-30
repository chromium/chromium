// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/widget.h"

namespace {

content::WebContents* GetWebContents(Browser* browser, int tab) {
  return browser->tab_strip_model()->GetWebContentsAt(tab);
}

InfoBarService* GetInfobarService(Browser* browser, int tab) {
  return InfoBarService::FromWebContents(GetWebContents(browser, tab));
}

std::u16string GetInfobarMessageText(Browser* browser, int tab) {
  return static_cast<ConfirmInfoBarDelegate*>(
             GetInfobarService(browser, tab)->infobar_at(0)->delegate())
      ->GetMessageText();
}

content::DesktopMediaID GetDesktopMediaID(Browser* browser, int tab) {
  content::RenderFrameHost* main_frame =
      GetWebContents(browser, tab)->GetMainFrame();
  return content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID()));
}

views::Widget* GetContentsBorder(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->contents_border_widget();
}

scoped_refptr<MediaStreamCaptureIndicator> GetCaptureIndicator() {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator();
}

void ActivateTab(Browser* browser, int tab) {
  browser->tab_strip_model()->ActivateTabAt(
      tab, {TabStripModel::GestureType::kMouse});
}

constexpr int kNoSharedTabIndex = -1;
}  // namespace

class TabSharingUIViewsBrowserTest : public InProcessBrowserTest {
 public:
  TabSharingUIViewsBrowserTest() {}

  void CreateUiAndStartSharing(Browser* browser, int tab) {
    // Explicitly activate the shared tab in testing.
    ActivateTab(browser, tab);

    tab_sharing_ui_ = TabSharingUI::Create(GetDesktopMediaID(browser, tab),
                                           u"example-sharing.com");
    tab_sharing_ui_->OnStarted(
        base::OnceClosure(),
        base::BindRepeating(&TabSharingUIViewsBrowserTest::OnStartSharing,
                            base::Unretained(this)));
  }

  // Verify that tab sharing infobars are displayed on all tabs, and content
  // border and tab capture indicator are only visible on the shared tab. Pass
  // |kNoSharedTabIndex| for |shared_tab_index| to indicate the shared tab is
  // not in |browser|.
  void VerifyUi(Browser* browser,
                int shared_tab_index,
                size_t infobar_count = 1,
                bool has_border = true) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
    has_border = false;
#endif
    views::Widget* contents_border = GetContentsBorder(browser);
    EXPECT_EQ(has_border, contents_border != nullptr);
    auto capture_indicator = GetCaptureIndicator();
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      // All tabs have |infobar_count| tab sharing infobars.
      InfoBarService* infobar_service = GetInfobarService(browser, i);
      EXPECT_EQ(infobar_count, infobar_service->infobar_count());
      for (size_t j = 0; j < infobar_count; ++j) {
        EXPECT_EQ(infobars::InfoBarDelegate::TAB_SHARING_INFOBAR_DELEGATE,
                  infobar_service->infobar_at(j)->delegate()->GetIdentifier());
      }

      // Content border is only visible on the shared tab.
      if (has_border) {
        ActivateTab(browser, i);
        EXPECT_EQ(i == shared_tab_index, contents_border->IsVisible());
      }

      // Tab capture indicator is only displayed on the shared tab.
      EXPECT_EQ(i == shared_tab_index,
                capture_indicator->IsBeingMirrored(GetWebContents(browser, i)));
    }
  }

  void AddTabs(Browser* browser, int tab_count = 1) {
    for (int i = 0; i < tab_count; ++i) {
      AddTabAtIndexToBrowser(browser, 0, GURL(chrome::kChromeUINewTabURL),
                             ui::PAGE_TRANSITION_LINK, true);
    }
  }

  TabSharingUIViews* tab_sharing_ui_views() {
    return static_cast<TabSharingUIViews*>(tab_sharing_ui_.get());
  }

 private:
  void OnStartSharing(const content::DesktopMediaID& media_id) {
    tab_sharing_ui_->OnStarted(
        base::OnceClosure(),
        base::BindRepeating(&TabSharingUIViewsBrowserTest::OnStartSharing,
                            base::Unretained(this)));
  }

  std::unique_ptr<TabSharingUI> tab_sharing_ui_;
};

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest, StartSharing) {
  AddTabs(browser(), 2);

  // Test that before sharing there are no infobars, content border or tab
  // capture indicator.
  VerifyUi(browser(), kNoSharedTabIndex, 0 /*infobar_count*/,
           false /*has_border*/);

  // Create UI and start sharing the tab at index 1.
  CreateUiAndStartSharing(browser(), 1);

  // Test that infobars were created, and contents border and tab capture
  // indicator are displayed on the shared tab.
  VerifyUi(browser(), 1);
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest, SwitchSharedTab) {
  AddTabs(browser(), 2);
  CreateUiAndStartSharing(browser(), 1);

  // Share a different tab.
  ActivateTab(browser(), 2);
  tab_sharing_ui_views()->StartSharing(
      GetInfobarService(browser(), 2)->infobar_at(0));

  // Test that the UI has been updated.
  VerifyUi(browser(), 2);
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest, StopSharing) {
  AddTabs(browser());
  CreateUiAndStartSharing(browser(), 1);

  tab_sharing_ui_views()->StopSharing();

  // Test that the infobars have been removed, and the contents border and tab
  // capture indicator are no longer visible.
  VerifyUi(browser(), kNoSharedTabIndex, 0 /*infobar_count*/);
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest, CloseTab) {
  AddTabs(browser(), 2);
  CreateUiAndStartSharing(browser(), 1);

  // Close a tab different than the shared one and wait until it's actually
  // closed, then test that the UI has not changed.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContentsDestroyedWatcher tab_2_destroyed_watcher(
      tab_strip_model->GetWebContentsAt(2));
  tab_strip_model->CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  tab_2_destroyed_watcher.Wait();
  VerifyUi(browser(), 1);

  // Close the shared tab and wait until it's actually closed, then verify that
  // sharing is stopped, i.e. the UI is removed.
  content::WebContentsDestroyedWatcher tab_1_destroyed_watcher(
      tab_strip_model->GetWebContentsAt(1));
  tab_strip_model->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  tab_1_destroyed_watcher.Wait();
  VerifyUi(browser(), kNoSharedTabIndex, 0 /*infobar_count*/);
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest,
                       CloseTabInIncognitoBrowser) {
  AddTabs(browser(), 2);

  // Start sharing a tab in an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser();
  AddTabs(incognito_browser, 2);
  CreateUiAndStartSharing(incognito_browser, 1);
  VerifyUi(incognito_browser, 1);
  VerifyUi(browser(), kNoSharedTabIndex, 1 /*infobar_count*/,
           false /*has_border*/);

  // Close a tab different than the shared one and test that the UI has not
  // changed.
  TabStripModel* tab_strip_model = incognito_browser->tab_strip_model();
  tab_strip_model->CloseWebContentsAt(2, TabStripModel::CLOSE_NONE);
  VerifyUi(incognito_browser, 1);
  VerifyUi(browser(), kNoSharedTabIndex, 1, false);

  // Close the shared tab in the incognito browser and test that the UI is
  // removed.
  incognito_browser->tab_strip_model()->CloseWebContentsAt(
      1, TabStripModel::CLOSE_NONE);
  VerifyUi(incognito_browser, kNoSharedTabIndex, 0 /*infobar_count*/);
  VerifyUi(browser(), kNoSharedTabIndex, 0, false);
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest, KillTab) {
  AddTabs(browser(), 2);
  CreateUiAndStartSharing(browser(), 1);

  // Kill a tab different than the shared one.
  content::WebContents* web_contents = GetWebContents(browser(), 0);
  content::RenderProcessHost* process =
      web_contents->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  crash_observer.Wait();

  // Verify that the sad tab does not have an infobar.
  InfoBarService* infobar_service = GetInfobarService(browser(), 0);
  EXPECT_EQ(0u, infobar_service->infobar_count());

  // Stop sharing should not result in a crash.
  tab_sharing_ui_views()->StopSharing();
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest, KillSharedTab) {
  AddTabs(browser(), 2);
  CreateUiAndStartSharing(browser(), 1);

  // Kill the shared tab.
  content::WebContents* shared_tab_web_contents = GetWebContents(browser(), 1);
  content::RenderProcessHost* shared_tab_process =
      shared_tab_web_contents->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher shared_tab_crash_observer(
      shared_tab_process,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  shared_tab_process->Shutdown(content::RESULT_CODE_KILLED);
  shared_tab_crash_observer.Wait();

  // Verify that killing the shared tab stopped sharing.
  VerifyUi(browser(), kNoSharedTabIndex, 0);
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsBrowserTest,
                       InfobarLabelUpdatedOnNavigation) {
  AddTabs(browser());
  CreateUiAndStartSharing(browser(), 0);
  ASSERT_THAT(base::UTF16ToUTF8(GetInfobarMessageText(browser(), 1)),
              ::testing::HasSubstr(chrome::kChromeUINewTabHost));

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIVersionURL));
  EXPECT_THAT(base::UTF16ToUTF8(GetInfobarMessageText(browser(), 1)),
              ::testing::HasSubstr(chrome::kChromeUIVersionHost));

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_THAT(base::UTF16ToUTF8(GetInfobarMessageText(browser(), 1)),
              ::testing::HasSubstr("about:blank"));
}

class MultipleTabSharingUIViewsBrowserTest : public InProcessBrowserTest {
 public:
  MultipleTabSharingUIViewsBrowserTest() {}

  void CreateUIsAndStartSharing(Browser* browser,
                                int tab_index,
                                int tab_count) {
    for (int i = 0; i < tab_count; ++i) {
      int tab = tab_index + i;
      ActivateTab(browser, tab);
      tab_sharing_ui_views_.push_back(TabSharingUI::Create(
          GetDesktopMediaID(browser, tab), u"example-sharing.com"));
      tab_sharing_ui_views_[tab_sharing_ui_views_.size() - 1]->OnStarted(
          base::OnceClosure(), content::MediaStreamUI::SourceCallback());
    }
  }

  TabSharingUIViews* tab_sharing_ui_views(int i) {
    return static_cast<TabSharingUIViews*>(tab_sharing_ui_views_[i].get());
  }

  void AddTabs(Browser* browser, int tab_count) {
    for (int i = 0; i < tab_count; ++i)
      AddBlankTabAndShow(browser);
  }

 private:
  std::vector<std::unique_ptr<TabSharingUI>> tab_sharing_ui_views_;
};

IN_PROC_BROWSER_TEST_F(MultipleTabSharingUIViewsBrowserTest, VerifyUi) {
  AddTabs(browser(), 3);
  CreateUIsAndStartSharing(browser(), 1, 3);

  // Check that all tabs have 3 infobars corresponding to the 3 sharing
  // sessions.
  int tab_count = browser()->tab_strip_model()->count();
  for (int i = 0; i < tab_count; ++i)
    EXPECT_EQ(3u, GetInfobarService(browser(), i)->infobar_count());

  // Check that all shared tabs display a tab capture indicator.
  auto capture_indicator = GetCaptureIndicator();
  for (int i = 1; i < tab_count; ++i)
    EXPECT_TRUE(
        capture_indicator->IsBeingMirrored(GetWebContents(browser(), i)));

  // Check that the border is only displayed on the last shared tab (known
  // limitation https://crbug.com/996631).
  views::Widget* contents_border = GetContentsBorder(browser());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
  EXPECT_EQ(nullptr, contents_border);
#else
  for (int i = 0; i < tab_count; ++i) {
    ActivateTab(browser(), i);
    EXPECT_EQ(i == 3, contents_border->IsVisible());
  }
#endif
}

IN_PROC_BROWSER_TEST_F(MultipleTabSharingUIViewsBrowserTest, StopSharing) {
  AddTabs(browser(), 3);
  CreateUIsAndStartSharing(browser(), 1, 3);

  // Stop sharing tabs one by one and check that infobars are removed as well.
  size_t shared_tab_count = 3;
  while (shared_tab_count) {
    tab_sharing_ui_views(--shared_tab_count)->StopSharing();
    for (int j = 0; j < browser()->tab_strip_model()->count(); ++j)
      EXPECT_EQ(shared_tab_count,
                GetInfobarService(browser(), j)->infobar_count());
  }
}

IN_PROC_BROWSER_TEST_F(MultipleTabSharingUIViewsBrowserTest, CloseTabs) {
  AddTabs(browser(), 3);
  CreateUIsAndStartSharing(browser(), 1, 3);

  // Close shared tabs one by one and check that infobars are removed as well.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  while (tab_strip_model->count() > 1) {
    tab_strip_model->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
    for (int i = 0; i < tab_strip_model->count(); ++i)
      EXPECT_EQ(tab_strip_model->count() - 1u,
                GetInfobarService(browser(), i)->infobar_count());
  }
}
