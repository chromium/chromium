// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_features.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
}  // namespace

class TabSharingMultiContentsViewTest
    : public SplitViewInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    SplitViewInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeatures(
        {
            features::kSideBySide,
#if BUILDFLAG(IS_CHROMEOS)
            features::kTabCaptureBlueBorderCrOS,
#endif
        },
        {});
  }

 protected:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  GURL GetTestUrl() { return embedded_test_server()->GetURL("/title1.html"); }

  auto ShareTab(int tab_index) {
    return Do([&, tab_index, this]() {
      content::WebContents* const web_contents =
          tab_strip_model()->GetWebContentsAt(tab_index);
      const content::DesktopMediaID media_id(
          content::DesktopMediaID::TYPE_WEB_CONTENTS,
          content::DesktopMediaID::kNullId,
          content::WebContentsMediaCaptureId(
              web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->GetDeprecatedID(),
              web_contents->GetPrimaryMainFrame()->GetRoutingID()));
      sharing_ui_ = TabSharingUIViews::Create(
          web_contents->GetPrimaryMainFrame()->GetGlobalId(), media_id,
          /*capturer_name=*/u"capturer.com",
          /*app_preferred_current_tab=*/false,
          TabSharingInfoBarDelegate::TabShareType::CAPTURE,
          /*captured_surface_control_active=*/false);
      sharing_ui_->OnStarted(base::DoNothing(), base::DoNothing(), {});
    });
  }

  auto StopSharingTab() {
    return Do([this]() { sharing_ui_.reset(); });
  }

  auto CheckIsCaptureContentsBorderShowing(int contents_container_index,
                                           bool should_show) {
    return CheckView(
        kMultiContentsViewElementId,
        [=](MultiContentsView* multi_contents_view) -> bool {
          ContentsContainerView* const contents_container_view =
              multi_contents_view
                  ->contents_container_views()[contents_container_index];
          views::Widget* const border_widget =
              contents_container_view->capture_contents_border_widget();
          return border_widget ? border_widget->IsVisible() : false;
        },
        should_show);
  }

 private:
  std::unique_ptr<TabSharingUI> sharing_ui_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSharingMultiContentsViewTest,
                       ContentsSharingBorderShows) {
  RunTestSequence(
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1), ShareTab(0),
      WaitForShow(kContentsCaptureBorder),
      CheckIsCaptureContentsBorderShowing(0, true),
      CheckIsCaptureContentsBorderShowing(1, false), StopSharingTab(),
      InAnyContext(WaitForHide(kContentsCaptureBorder)),
      CheckIsCaptureContentsBorderShowing(0, false),
      CheckIsCaptureContentsBorderShowing(1, false));
}

IN_PROC_BROWSER_TEST_F(TabSharingMultiContentsViewTest,
                       ReverseSplitWhileContentsSharing) {
  RunTestSequence(
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1), ShareTab(0),
      WaitForShow(kContentsCaptureBorder),
      CheckIsCaptureContentsBorderShowing(0, true),
      CheckIsCaptureContentsBorderShowing(1, false), Do([this] {
        TabStripModel* const tab_strip_model = browser()->tab_strip_model();
        tab_strip_model->ReverseTabsInSplit(
            tab_strip_model->GetTabAtIndex(0)->GetSplit().value());
      }),
      CheckIsCaptureContentsBorderShowing(0, false),
      CheckIsCaptureContentsBorderShowing(1, true));
}

IN_PROC_BROWSER_TEST_F(TabSharingMultiContentsViewTest,
                       SeparateSplitWhileContentsSharing) {
  RunTestSequence(
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), EnterSplitView(0, 1), ShareTab(0),
      WaitForShow(kContentsCaptureBorder),
      CheckIsCaptureContentsBorderShowing(0, true),
      CheckIsCaptureContentsBorderShowing(1, false), Do([this] {
        TabStripModel* const tab_strip_model = browser()->tab_strip_model();
        tab_strip_model->RemoveSplit(
            tab_strip_model->GetTabAtIndex(0)->GetSplit().value());
      }),
      SelectTab(kTabStripElementId, 0),
      CheckIsCaptureContentsBorderShowing(0, true),
      SelectTab(kTabStripElementId, 1), WaitForHide(kContentsCaptureBorder),
      CheckIsCaptureContentsBorderShowing(0, false));
}

#if BUILDFLAG(IS_CHROMEOS)
class ChromeOsTabSharingTest : public TabSharingMultiContentsViewTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitWithFeatures(
        {}, {
                features::kTabCaptureBlueBorderCrOS,
            });
  }

  TabCaptureContentsBorderHelper* GetTabCaptureContentsBorderHelper(int index) {
    TabStripModel* const tab_strip_model = browser()->tab_strip_model();
    return TabCaptureContentsBorderHelper::FromWebContents(
        tab_strip_model->GetWebContentsAt(index));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeOsTabSharingTest, BorderStaysHidden) {
  RunTestSequence(
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      SelectTab(kTabStripElementId, 0), ShareTab(0),
      EnsureNotPresent(kContentsCaptureBorder), Check([=, this]() {
        return GetTabCaptureContentsBorderHelper(0)->IsTabCapturing();
      }),
      StopSharingTab(), EnsureNotPresent(kContentsCaptureBorder),
      Check([=, this]() {
        return !GetTabCaptureContentsBorderHelper(0)->IsTabCapturing();
      }));
}
#endif  // BUILDFLAG(IS_CHROMEOS)
