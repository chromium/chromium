// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"

#include <map>
#include <string>

#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

using ::testing::Not;

content::WebContents* GetWebContents(Browser* browser, int tab) {
  return browser->tab_strip_model()->GetWebContentsAt(tab);
}

content::GlobalRenderFrameHostId GetGlobalId(Browser* browser, int tab) {
  auto* const main_frame = GetWebContents(browser, tab)->GetPrimaryMainFrame();
  return main_frame ? main_frame->GetGlobalId()
                    : content::GlobalRenderFrameHostId();
}

infobars::ContentInfoBarManager* GetInfoBarManager(Browser* browser, int tab) {
  return infobars::ContentInfoBarManager::FromWebContents(
      GetWebContents(browser, tab));
}

ConfirmInfoBarDelegate* GetDelegate(Browser* browser, int tab) {
  return static_cast<ConfirmInfoBarDelegate*>(
      GetInfoBarManager(browser, tab)->infobar_at(0)->delegate());
}

std::u16string GetInfobarMessageText(Browser* browser, int tab) {
  return GetDelegate(browser, tab)->GetMessageText();
}

bool HasSecondaryButton(Browser* browser, int tab) {
  return GetDelegate(browser, tab)->GetButtons() &
         ConfirmInfoBarDelegate::InfoBarButton::BUTTON_CANCEL;
}

std::u16string GetSecondaryButtonLabel(Browser* browser, int tab) {
  DCHECK(HasSecondaryButton(browser, tab));  // Test error otherwise.
  return GetDelegate(browser, tab)
      ->GetButtonLabel(ConfirmInfoBarDelegate::InfoBarButton::BUTTON_CANCEL);
}

ui::ImageModel GetSecondaryButtonImage(Browser* browser, int tab) {
  DCHECK(HasSecondaryButton(browser, tab));  // Test error otherwise.
  return GetDelegate(browser, tab)
      ->GetButtonImage(ConfirmInfoBarDelegate::InfoBarButton::BUTTON_CANCEL);
}

bool SecondaryButtonIsEnabled(Browser* browser, int tab) {
  DCHECK(HasSecondaryButton(browser, tab));  // Test error otherwise.
  return GetDelegate(browser, tab)
      ->GetButtonEnabled(ConfirmInfoBarDelegate::InfoBarButton::BUTTON_CANCEL);
}

bool HasTertiaryButton(Browser* browser, int tab) {
  return GetDelegate(browser, tab)->GetButtons() &
         ConfirmInfoBarDelegate::InfoBarButton::BUTTON_EXTRA;
}

std::u16string GetTertiaryButtonLabel(Browser* browser, int tab) {
  DCHECK(HasTertiaryButton(browser, tab));  // Test error otherwise.
  return GetDelegate(browser, tab)
      ->GetButtonLabel(ConfirmInfoBarDelegate::InfoBarButton::BUTTON_EXTRA);
}

std::u16string GetExpectedSwitchToMessage(Browser* browser, int tab) {
  content::RenderFrameHost* const rfh =
      GetWebContents(browser, tab)->GetPrimaryMainFrame();
  return l10n_util::GetStringFUTF16(
      IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
      url_formatter::FormatOriginForSecurityDisplay(
          rfh->GetLastCommittedOrigin(),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

content::DesktopMediaID GetDesktopMediaID(Browser* browser, int tab) {
  content::RenderFrameHost* main_frame =
      GetWebContents(browser, tab)->GetPrimaryMainFrame();
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
      tab, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kMouse));
}

constexpr int kNullTabIndex = -1;
const std::u16string kShareThisTabInsteadMessage = u"Share this tab instead";
const std::u16string kViewTabMessage = u"View tab:";

#if BUILDFLAG(IS_CHROMEOS)
const policy::DlpContentRestrictionSet kEmptyRestrictionSet;
const policy::DlpContentRestrictionSet kScreenshareRestrictionSet(
    policy::DlpContentRestriction::kScreenShare,
    policy::DlpRulesManager::Level::kBlock);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class TabSharingUIViewsBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  TabSharingUIViewsBrowserTest()
      : favicons_used_for_switch_to_tab_button_(GetParam()) {
#if BUILDFLAG(IS_CHROMEOS)
    features_.InitAndEnableFeature(features::kTabCaptureBlueBorderCrOS);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    DCHECK_EQ(browser()->tab_strip_model()->count(), 1);
    CreateUniqueFaviconFor(browser()->tab_strip_model()->GetWebContentsAt(0));
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void CreateUiAndStartSharing(Browser* browser,
                               int capturing_tab,
                               int captured_tab) {
    // Explicitly activate the shared tab in testing.
    ActivateTab(browser, captured_tab);

    tab_sharing_ui_ = TabSharingUI::Create(
        GetGlobalId(browser, capturing_tab),
        GetDesktopMediaID(browser, captured_tab), u"example-sharing.com",
        favicons_used_for_switch_to_tab_button_,
        /*app_preferred_current_tab=*/false,
        TabSharingInfoBarDelegate::TabShareType::CAPTURE);

    if (favicons_used_for_switch_to_tab_button_) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* const web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        tab_sharing_ui_views()->SetTabFaviconForTesting(
            web_contents, favicons_.find(web_contents)->second);
      }
    }

    tab_sharing_ui_->OnStarted(
        base::OnceClosure(),
        base::BindRepeating(&TabSharingUIViewsBrowserTest::OnStartSharing,
                            base::Unretained(this)),
        std::vector<content::DesktopMediaID>{});
  }

  struct UiExpectations {
    Browser* browser;
    int capturing_tab;
    int captured_tab;
    size_t infobar_count = 1;
    bool has_border = true;
    int tab_with_disabled_button = kNullTabIndex;
  };

  // Verify that tab sharing infobars are displayed on all tabs, and content
  // border and tab capture indicator are only visible on the shared tab. Pass
  // |kNullTabIndex| for |captured_tab| to indicate the shared tab is
  // not in |browser|.
  void VerifyUi(const UiExpectations& expectations) {
    Browser* const browser = expectations.browser;
    const int capturing_tab = expectations.capturing_tab;
    const int captured_tab = expectations.captured_tab;
    const size_t infobar_count = expectations.infobar_count;
    const bool has_border = expectations.has_border;
    const int tab_with_disabled_button = expectations.tab_with_disabled_button;

    DCHECK((capturing_tab != kNullTabIndex && captured_tab != kNullTabIndex) ||
           (capturing_tab == kNullTabIndex && captured_tab == kNullTabIndex));

    views::Widget* contents_border = GetContentsBorder(browser);
    EXPECT_EQ(has_border, contents_border != nullptr);
    auto capture_indicator = GetCaptureIndicator();
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      // All tabs have |infobar_count| tab sharing infobars.
      infobars::ContentInfoBarManager* infobar_manager =
          GetInfoBarManager(browser, i);
      EXPECT_EQ(infobar_count, infobar_manager->infobar_count());
      for (size_t j = 0; j < infobar_count; ++j) {
        EXPECT_EQ(infobars::InfoBarDelegate::TAB_SHARING_INFOBAR_DELEGATE,
                  infobar_manager->infobar_at(j)->delegate()->GetIdentifier());
      }

      // Content border is only visible on the shared tab.
      if (has_border) {
        ActivateTab(browser, i);
        EXPECT_EQ(i == captured_tab, contents_border->IsVisible());
      }

      // Tab capture indicator is only displayed on the shared tab.
      EXPECT_EQ(i == captured_tab,
                capture_indicator->IsBeingMirrored(GetWebContents(browser, i)));

      if (i == capturing_tab && i == captured_tab) {
        // Self-capture.
        EXPECT_FALSE(HasSecondaryButton(browser, i));
      } else if (i == capturing_tab) {
        // Capturing-tab's infobar.
        ASSERT_TRUE(HasSecondaryButton(browser, i));
        EXPECT_EQ(GetSecondaryButtonLabel(browser, i),
                  GetExpectedSwitchToMessage(browser, captured_tab));
        EXPECT_EQ(GetSecondaryButtonImage(browser, i),
                  GetFaviconAssociatedWith(browser, captured_tab));
      } else if (i == captured_tab) {
        // Captured-tab's infobar.
        ASSERT_TRUE(HasSecondaryButton(browser, i));
        EXPECT_EQ(GetSecondaryButtonLabel(browser, i),
                  GetExpectedSwitchToMessage(browser, capturing_tab));
        EXPECT_EQ(GetSecondaryButtonImage(browser, i),
                  GetFaviconAssociatedWith(browser, capturing_tab));
      } else if (infobar_manager->infobar_count() > 0) {
        // Any other infobar.
        ASSERT_TRUE(HasSecondaryButton(browser, i));
        EXPECT_EQ(GetSecondaryButtonLabel(browser, i),
                  kShareThisTabInsteadMessage);
        EXPECT_EQ(GetSecondaryButtonImage(browser, i), ui::ImageModel());
        EXPECT_EQ(SecondaryButtonIsEnabled(browser, i),
                  i != tab_with_disabled_button)
            << "Tab: " << i;
      }
    }
  }

  void AddTabs(Browser* browser, int tab_count) {
    for (int i = 0; i < tab_count; ++i) {
      const int next_index = browser->tab_strip_model()->count();
      ASSERT_TRUE(AddTabAtIndexToBrowser(browser, next_index,
                                         GURL(chrome::kChromeUINewTabURL),
                                         ui::PAGE_TRANSITION_LINK, true));
      CreateUniqueFaviconFor(
          browser->tab_strip_model()->GetWebContentsAt(next_index));
    }
  }

  void CreateUniqueFaviconFor(content::WebContents* web_contents) {
    // The URL produces here is only intended to produce a unique favicon.
    // Note that GenerateMonogramFavicon() uses the first letter in the domain
    // given to it for the monogram, meaning these URLs are all going to
    // produce distinct favicons.
    DCHECK_LE(next_unique_char_, 'z');
    const ui::ImageModel favicon = ui::ImageModel::FromImage(
        gfx::Image::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
            GURL("https://" + std::string(1, next_unique_char_++) + ".com"),
            gfx::kFaviconSize, gfx::kFaviconSize)));

    for (const auto& it : favicons_) {
      ASSERT_NE(favicon, it.second);
    }

    favicons_[web_contents] = favicon;
  }

  ui::ImageModel GetFaviconAssociatedWith(Browser* browser, int tab) {
    if (!favicons_used_for_switch_to_tab_button_) {
      return ui::ImageModel();
    }
    content::WebContents* const web_contents =
        browser->tab_strip_model()->GetWebContentsAt(tab);
    return favicons_.find(web_contents)->second;
  }

  void UpdateTabFavicon(Browser* browser, int tab) {
    if (!favicons_used_for_switch_to_tab_button_) {
      return;
    }

    CreateUniqueFaviconFor(browser->tab_strip_model()->GetWebContentsAt(tab));

    content::WebContents* const web_contents =
        browser->tab_strip_model()->GetWebContentsAt(tab);
    tab_sharing_ui_views()->SetTabFaviconForTesting(
        web_contents, favicons_.find(web_contents)->second);

    // Simulate waiting until the next periodic update.
    tab_sharing_ui_views()->FaviconPeriodicUpdate(1);
  }

  TabSharingUIViews* tab_sharing_ui_views() {
    return static_cast<TabSharingUIViews*>(tab_sharing_ui_.get());
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  void ApplyDlpForAllUsers() {
    TabSharingUIViews::ApplyDlpForAllUsersForTesting();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  void OnStartSharing(const content::DesktopMediaID& media_id) {
    tab_sharing_ui_->OnStarted(
        base::OnceClosure(),
        base::BindRepeating(&TabSharingUIViewsBrowserTest::OnStartSharing,
                            base::Unretained(this)),
        std::vector<content::DesktopMediaID>{});
  }

#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList features_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  const bool favicons_used_for_switch_to_tab_button_;

  std::unique_ptr<TabSharingUI> tab_sharing_ui_;

  std::map<content::WebContents*, ui::ImageModel> favicons_;
  char next_unique_char_ = 'a';  // Derive https://x.com from x.
};

INSTANTIATE_TEST_SUITE_P(All, TabSharingUIViewsBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, StartSharing) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  // Test that before sharing there are no infobars, content border or tab
  // capture indicator.
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 0,
                          .has_border = false});

  // Create UI and start sharing the tab at index 1.
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);

  // Test that infobars were created, and contents border and tab capture
  // indicator are displayed on the shared tab.
  VerifyUi(UiExpectations{
      .browser = browser(), .capturing_tab = 0, .captured_tab = 1});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, SwitchSharedTab) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);

  // Share a different tab.
  // When switching tabs, a new UI is created, and the old one destroyed.
  ActivateTab(browser(), 2);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/2);

  // Test that the UI has been updated.
  VerifyUi(UiExpectations{
      .browser = browser(), .capturing_tab = 0, .captured_tab = 2});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest,
                       ChangeCapturingTabFavicon) {
  constexpr int kCapturingTab = 0;
  constexpr int kCapturedTab = 1;

  // Set up a screen-capture session.
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kCapturingTab,
                          .captured_tab = kCapturedTab});  // Sanity.

  // Simulate changing the tab favicon to a unique new favicon, then waiting
  // until the change is picked up by the next periodic update.
  UpdateTabFavicon(browser(), kCapturingTab);
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kCapturingTab,
                          .captured_tab = kCapturedTab});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, ChangeCapturedTabFavicon) {
  constexpr int kCapturingTab = 0;
  constexpr int kCapturedTab = 1;

  // Set up a screen-capture session.
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kCapturingTab,
                          .captured_tab = kCapturedTab});  // Sanity.

  // Simulate changing the tab favicon to a unique new favicon, then waiting
  // until the change is picked up by the next periodic update.
  UpdateTabFavicon(browser(), kCapturedTab);
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kCapturingTab,
                          .captured_tab = kCapturedTab});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, ChangeOtherTabFavicon) {
  constexpr int kCapturingTab = 0;
  constexpr int kCapturedTab = 1;
  constexpr int kOtherTab = 2;

  // Set up a screen-capture session.
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kCapturingTab,
                          .captured_tab = kCapturedTab});  // Sanity.

  // Simulate changing the tab favicon to a unique new favicon, then waiting
  // until the change is picked up by the next periodic update.
  UpdateTabFavicon(browser(), kOtherTab);
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kCapturingTab,
                          .captured_tab = kCapturedTab});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, StopSharing) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);

  tab_sharing_ui_views()->StopSharing();

  // Test that the infobars have been removed, and the contents border and tab
  // capture indicator are no longer visible.
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 0});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, CloseTab) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);

  // Close a tab different than the shared one and wait until it's actually
  // closed, then test that the UI has not changed.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContentsDestroyedWatcher tab_2_destroyed_watcher(
      tab_strip_model->GetWebContentsAt(2));
  tab_strip_model->CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  tab_2_destroyed_watcher.Wait();
  VerifyUi(UiExpectations{
      .browser = browser(), .capturing_tab = 0, .captured_tab = 1});

  // Close the shared tab and wait until it's actually closed, then verify that
  // sharing is stopped, i.e. the UI is removed.
  content::WebContentsDestroyedWatcher tab_1_destroyed_watcher(
      tab_strip_model->GetWebContentsAt(1));
  tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  tab_1_destroyed_watcher.Wait();
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 0});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest,
                       BorderWidgetShouldCloseWhenBrowserCloses) {
  Browser* new_browser = CreateBrowser(browser()->profile());
  AddTabs(new_browser, 2);
  ASSERT_EQ(new_browser->tab_strip_model()->count(), 3);
  CreateUniqueFaviconFor(new_browser->tab_strip_model()->GetWebContentsAt(0));
  CreateUiAndStartSharing(new_browser, /*capturing_tab=*/0, /*captured_tab=*/1);

  // Share a different tab.
  // When switching tabs, a new UI is created, and the old one destroyed.
  ActivateTab(new_browser, 2);
  CreateUiAndStartSharing(new_browser, /*capturing_tab=*/0, /*captured_tab=*/2);

  // Test that the UI has been updated.
  VerifyUi(UiExpectations{
      .browser = new_browser, .capturing_tab = 0, .captured_tab = 2});

  auto contents_border_weakptr = GetContentsBorder(new_browser)->GetWeakPtr();
  CloseBrowserSynchronously(new_browser);
  EXPECT_FALSE(contents_border_weakptr);
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest,
                       CloseTabInIncognitoBrowser) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  // Start sharing a tab in an incognito browser.
  Browser* incognito_browser = CreateIncognitoBrowser();
  DCHECK_EQ(incognito_browser->tab_strip_model()->count(), 1);
  CreateUniqueFaviconFor(
      incognito_browser->tab_strip_model()->GetWebContentsAt(0));

  AddTabs(incognito_browser, 3);
  ASSERT_EQ(incognito_browser->tab_strip_model()->count(), 4);
  CreateUiAndStartSharing(incognito_browser, /*capturing_tab=*/0,
                          /*captured_tab=*/1);
  VerifyUi(UiExpectations{
      .browser = incognito_browser, .capturing_tab = 0, .captured_tab = 1});
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 1,
                          .has_border = false});

  // Close a tab different than the shared one and test that the UI has not
  // changed.
  TabStripModel* tab_strip_model = incognito_browser->tab_strip_model();
  tab_strip_model->CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  VerifyUi(UiExpectations{
      .browser = incognito_browser, .capturing_tab = 0, .captured_tab = 1});
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 1,
                          .has_border = false});

  // Close the shared tab in the incognito browser and test that the UI is
  // removed.
  incognito_browser->tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_NONE);
  VerifyUi(UiExpectations{.browser = incognito_browser,
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 0});
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 0,
                          .has_border = false});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, KillTab) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/1, /*captured_tab=*/2);

  // Kill a tab different than the shared one.
  content::WebContents* web_contents = GetWebContents(browser(), 0);
  content::RenderProcessHost* process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(content::RESULT_CODE_KILLED);
  crash_observer.Wait();

  // Verify that the sad tab does not have an infobar.
  infobars::ContentInfoBarManager* infobar_manager =
      GetInfoBarManager(browser(), 0);
  EXPECT_EQ(0u, infobar_manager->infobar_count());

  // Stop sharing should not result in a crash.
  tab_sharing_ui_views()->StopSharing();
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest, KillSharedTab) {
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);

  // Kill the shared tab.
  content::WebContents* shared_tab_web_contents = GetWebContents(browser(), 1);
  content::RenderProcessHost* shared_tab_process =
      shared_tab_web_contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher shared_tab_crash_observer(
      shared_tab_process,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  shared_tab_process->Shutdown(content::RESULT_CODE_KILLED);
  shared_tab_crash_observer.Wait();

  // Verify that killing the shared tab stopped sharing.
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = kNullTabIndex,
                          .captured_tab = kNullTabIndex,
                          .infobar_count = 0});
}

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest,
                       InfobarLabelUpdatedOnNavigation) {
  AddTabs(browser(), 1);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  constexpr int kCapturingTab = 0;
  constexpr int kCapturedTab = 1;

  CreateUiAndStartSharing(browser(), kCapturingTab, kCapturedTab);
  ASSERT_THAT(base::UTF16ToUTF8(GetInfobarMessageText(browser(), 0)),
              ::testing::HasSubstr(chrome::kChromeUINewTabPageHost));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
  EXPECT_THAT(
      base::UTF16ToUTF8(GetInfobarMessageText(browser(), kCapturingTab)),
      ::testing::HasSubstr(chrome::kChromeUIVersionHost));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  EXPECT_THAT(
      base::UTF16ToUTF8(GetInfobarMessageText(browser(), kCapturingTab)),
      ::testing::HasSubstr("about:blank"));
}

#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(TabSharingUIViewsBrowserTest,
                       SharingWithDlpAndNavigation) {
  // DLP setup
  ApplyDlpForAllUsers();
  policy::DlpContentTabHelper::ScopedIgnoreDlpRulesManager
      ignore_dlp_rules_manager =
          policy::DlpContentTabHelper::IgnoreDlpRulesManagerForTesting();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL kUrlRestricted =
      embedded_test_server()->GetURL("restricted.com", "/title1.html");
  GURL kUrlUnrestricted =
      embedded_test_server()->GetURL("unrestricted.com", "/title1.html");

  policy::DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrlRestricted, kScreenshareRestrictionSet);
  policy::DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrlUnrestricted, kEmptyRestrictionSet);

  // Start actual test
  AddTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  // Create UI and start sharing the tab at index 1.
  CreateUiAndStartSharing(browser(), /*capturing_tab=*/0, /*captured_tab=*/1);

  // Test that infobars were created, and contents border and tab capture
  // indicator are displayed on the shared tab.
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = 0,
                          .captured_tab = 1,
                          .infobar_count = 1,
                          .has_border = true,
                          .tab_with_disabled_button = kNullTabIndex});

  constexpr int kRestrictedTab = 2;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(kRestrictedTab);
  // Navigate to restricted URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents, kUrlRestricted));

  // Test that button on tab 2 is now disabled.
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = 0,
                          .captured_tab = 1,
                          .infobar_count = 1,
                          .has_border = true,
                          .tab_with_disabled_button = kRestrictedTab});

  // Navigate to unrestricted URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents, kUrlUnrestricted));

  // Verify that button on tab 2 is re-enabled.
  VerifyUi(UiExpectations{.browser = browser(),
                          .capturing_tab = 0,
                          .captured_tab = 1,
                          .infobar_count = 1,
                          .has_border = true,
                          .tab_with_disabled_button = kNullTabIndex});
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class MultipleTabSharingUIViewsBrowserTest : public InProcessBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  MultipleTabSharingUIViewsBrowserTest() {
    features_.InitAndEnableFeature(features::kTabCaptureBlueBorderCrOS);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  void CreateUIsAndStartSharing(Browser* browser,
                                int capturing_tab,
                                int captured_tab_first,
                                int captured_tab_last) {
    for (int captured_tab = captured_tab_first;
         captured_tab <= captured_tab_last; ++captured_tab) {
      DCHECK_NE(captured_tab, capturing_tab);
      ActivateTab(browser, captured_tab);
      tab_sharing_ui_views_.push_back(TabSharingUI::Create(
          GetGlobalId(browser, capturing_tab),
          GetDesktopMediaID(browser, captured_tab), u"example-sharing.com",
          /*favicons_used_for_switch_to_tab_button=*/false,
          /*app_preferred_current_tab=*/false,
          TabSharingInfoBarDelegate::TabShareType::CAPTURE));
      tab_sharing_ui_views_[tab_sharing_ui_views_.size() - 1]->OnStarted(
          base::OnceClosure(), content::MediaStreamUI::SourceCallback(),
          std::vector<content::DesktopMediaID>{});
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
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList features_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::vector<std::unique_ptr<TabSharingUI>> tab_sharing_ui_views_;
};

IN_PROC_BROWSER_TEST_F(MultipleTabSharingUIViewsBrowserTest, VerifyUi) {
  AddTabs(browser(), 3);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 4);
  CreateUIsAndStartSharing(browser(), /*capturing_tab=*/0,
                           /*captured_tab_first=*/1, /*captured_tab_last=*/3);

  // Check that all tabs have 3 infobars corresponding to the 3 sharing
  // sessions.
  int tab_count = browser()->tab_strip_model()->count();
  for (int i = 0; i < tab_count; ++i)
    EXPECT_EQ(3u, GetInfoBarManager(browser(), i)->infobar_count());

  // Check that all shared tabs display a tab capture indicator.
  auto capture_indicator = GetCaptureIndicator();
  for (int i = 1; i < tab_count; ++i)
    ASSERT_TRUE(
        capture_indicator->IsBeingMirrored(GetWebContents(browser(), i)));

  views::Widget* contents_border = GetContentsBorder(browser());
  // The capturing tab, which is not itself being captured, does not have
  // the contents-border.
  ActivateTab(browser(), 0);
  EXPECT_FALSE(contents_border->IsVisible());
  // All other tabs are being captured, and therefore have a visible
  // contents-borders whenever they themselves (the tabs) are visible.
  for (int i = 1; i < tab_count; ++i) {
    ActivateTab(browser(), i);
    ASSERT_TRUE(contents_border->IsVisible());
  }
}

IN_PROC_BROWSER_TEST_F(MultipleTabSharingUIViewsBrowserTest, StopSharing) {
  AddTabs(browser(), 3);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 4);
  CreateUIsAndStartSharing(browser(), /*capturing_tab=*/0,
                           /*captured_tab_first=*/1, /*captured_tab_last=*/3);

  // Stop sharing tabs one by one and check that infobars are removed as well.
  size_t shared_tab_count = 3;
  while (shared_tab_count) {
    tab_sharing_ui_views(--shared_tab_count)->StopSharing();
    for (int j = 0; j < browser()->tab_strip_model()->count(); ++j)
      ASSERT_EQ(shared_tab_count,
                GetInfoBarManager(browser(), j)->infobar_count());
  }
}

IN_PROC_BROWSER_TEST_F(MultipleTabSharingUIViewsBrowserTest, CloseTabs) {
  AddTabs(browser(), 3);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 4);
  CreateUIsAndStartSharing(browser(), /*capturing_tab=*/0,
                           /*captured_tab_first=*/1, /*captured_tab_last=*/3);

  // Close shared tabs one by one and check that infobars are removed as well.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  while (tab_strip_model->count() > 1) {
    tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
    for (int i = 0; i < tab_strip_model->count(); ++i)
      ASSERT_EQ(tab_strip_model->count() - 1u,
                GetInfoBarManager(browser(), i)->infobar_count());
  }
}

class TabSharingUIViewsPreferCurrentTabBrowserTest
    : public InProcessBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  TabSharingUIViewsPreferCurrentTabBrowserTest() {
    features_.InitAndEnableFeature(features::kTabCaptureBlueBorderCrOS);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  ~TabSharingUIViewsPreferCurrentTabBrowserTest() override = default;

  void ManualSetUp(int captured_tab) {
    auto source_change_cb = base::BindRepeating(
        &TabSharingUIViewsPreferCurrentTabBrowserTest::SourceChange,
        base::Unretained(this));

    AddTabs(browser(), 1);  // Starts at 1, so we're up to 2.

    ActivateTab(browser(), kTab0);
    tab_sharing_ui_views_ = TabSharingUI::Create(
        GetGlobalId(browser(), kTab0),
        GetDesktopMediaID(browser(), captured_tab), u"example-sharing.com",
        /*favicons_used_for_switch_to_tab_button=*/false,
        /*app_preferred_current_tab=*/true,
        TabSharingInfoBarDelegate::TabShareType::CAPTURE);
    tab_sharing_ui_views_->OnStarted(base::OnceClosure(), source_change_cb,
                                     std::vector<content::DesktopMediaID>{});
  }

  void AddTabs(Browser* browser, int tab_count) {
    for (int i = 0; i < tab_count; ++i)
      AddBlankTabAndShow(browser);
  }

  void SourceChange(const content::DesktopMediaID& media_id) {}

 protected:
  const int kTab0 = 0;
  const int kTab1 = 1;

#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList features_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<TabSharingUI> tab_sharing_ui_views_;
};

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsPreferCurrentTabBrowserTest,
                       VerifyUiWhenSelfCapturing) {
  ManualSetUp(/*captured_tab=*/kTab0);

  // The tab which is capturing itself: [Stop]
  EXPECT_FALSE(HasSecondaryButton(browser(), kTab0));
  EXPECT_FALSE(HasTertiaryButton(browser(), kTab0));

  // Any other tab: [Stop] [Share this tab instead]
  EXPECT_TRUE(HasSecondaryButton(browser(), kTab1));
  EXPECT_EQ(GetSecondaryButtonLabel(browser(), kTab1),
            kShareThisTabInsteadMessage);
  EXPECT_FALSE(HasTertiaryButton(browser(), kTab1));
}

IN_PROC_BROWSER_TEST_F(TabSharingUIViewsPreferCurrentTabBrowserTest,
                       VerifyUiWhenCapturingAnotherTab) {
  ManualSetUp(/*captured_tab=*/kTab1);

  // The capturing tab: [Stop] [Share this tab instead] [View tab: ...]
  EXPECT_TRUE(HasSecondaryButton(browser(), kTab0));
  EXPECT_EQ(GetSecondaryButtonLabel(browser(), kTab0),
            kShareThisTabInsteadMessage);
  EXPECT_TRUE(HasTertiaryButton(browser(), kTab0));
  EXPECT_TRUE(base::StartsWith(GetTertiaryButtonLabel(browser(), kTab0),
                               kViewTabMessage));

  // The capturing tab: [Stop] [View tab: ...]
  EXPECT_TRUE(HasSecondaryButton(browser(), kTab1));
  EXPECT_TRUE(base::StartsWith(GetSecondaryButtonLabel(browser(), kTab1),
                               kViewTabMessage));
  EXPECT_FALSE(HasTertiaryButton(browser(), kTab1));
}
