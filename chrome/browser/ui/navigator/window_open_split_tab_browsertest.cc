// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "url/gurl.h"

namespace {

// The click modifier for opening a link in a new tab.
#if BUILDFLAG(IS_MAC)
constexpr int kNewTabModifier = blink::WebInputEvent::kMetaKey;
#else
constexpr int kNewTabModifier = blink::WebInputEvent::kControlKey;
#endif

}  // namespace

// Tests for the "split", "splitposition", and "splitsize" window.open()
// features, which request that the new content be opened in a split tab
// alongside the opener.
class WindowOpenSplitTabBrowserTest : public InProcessBrowserTest {
 public:
  WindowOpenSplitTabBrowserTest() {
    // The base::Feature is enabled by default, but the Blink feature only
    // follows it when it is overridden. kSplitViewHorizontal is enabled
    // explicitly, since the top/bottom tests expect a stacked split.
    feature_list_.InitWithFeatures(
        {blink::features::kWindowOpenSplitTab, tabs::kSplitViewHorizontal}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/title1.html")));
  }

 protected:
  TabStripModel* tab_strip_model() { return browser()->GetTabStripModel(); }

  content::WebContents* opener_contents() {
    return tab_strip_model()->GetWebContentsAt(0);
  }

  // Calls window.open() from `opener` with the given `features`, stores the
  // result in `window.w` in `opener`, and returns the new WebContents.
  content::WebContents* OpenWindow(content::WebContents* opener,
                                   const std::string& features) {
    content::WebContentsAddedObserver observer;
    EXPECT_TRUE(content::ExecJs(
        opener, content::JsReplace(
                    "window.w = window.open($1, '_blank', $2);",
                    embedded_test_server()->GetURL("/title2.html"), features)));
    content::WebContents* new_contents = observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));
    return new_contents;
  }

  // Clicks in `opener` with the given `modifiers`, where a click handler calls
  // window.open() with the given `features`, and returns the new WebContents.
  content::WebContents* ClickToOpenWindow(content::WebContents* opener,
                                          int modifiers,
                                          const std::string& features) {
    EXPECT_TRUE(content::ExecJs(
        opener, content::JsReplace(
                    "document.addEventListener('click', () => {"
                    "  window.open($1, '_blank', $2);"
                    "});",
                    embedded_test_server()->GetURL("/title2.html"), features)));
    content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(opener);
    content::WebContentsAddedObserver observer;
    content::SimulateMouseClick(opener, modifiers,
                                blink::WebMouseEvent::Button::kLeft);
    content::WebContents* new_contents = observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));
    return new_contents;
  }

  // Creates an element of `element_type` ("a", "area", or "form") in `opener`
  // targeting "/title2.html" with the given `window_features` and `rel`,
  // activates it (via click() or submit()), and returns the new WebContents.
  content::WebContents* OpenViaElement(content::WebContents* opener,
                                       const std::string& element_type,
                                       const std::string& window_features,
                                       const std::string& rel = "") {
    content::WebContentsAddedObserver observer;
    EXPECT_TRUE(content::ExecJs(
        opener,
        content::JsReplace(
            "const el = document.createElement($1);"
            "if ($1 === 'form') { el.action = $2; } else { el.href = $2; }"
            "el.windowFeatures = $3;"
            "if ($4) { el.rel = $4; }"
            "document.body.appendChild(el);"
            "if ($1 === 'form') { el.submit(); } else { el.click(); }",
            element_type, embedded_test_server()->GetURL("/title2.html"),
            window_features, rel)));
    content::WebContents* new_contents = observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));
    return new_contents;
  }

  // Returns the split data for `contents`, or nullptr if it is not split.
  split_tabs::SplitTabData* GetSplitData(content::WebContents* contents) {
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(contents);
    if (!tab || !tab->IsSplit()) {
      return nullptr;
    }
    return tab_strip_model()->GetSplitData(tab->GetSplit().value());
  }

  // Expects `opener` and `new_contents` to be split together, with the given
  // order, layout and ratio.
  void ExpectSplit(content::WebContents* opener,
                   content::WebContents* new_contents,
                   bool new_tab_first,
                   split_tabs::SplitTabLayout layout,
                   double first_tab_ratio) {
    split_tabs::SplitTabData* split_data = GetSplitData(new_contents);
    ASSERT_TRUE(split_data);
    EXPECT_EQ(split_data, GetSplitData(opener));
    std::vector<tabs::TabInterface*> tabs = split_data->ListTabs();
    ASSERT_EQ(2u, tabs.size());
    tabs::TabInterface* opener_tab =
        tabs::TabInterface::MaybeGetFromContents(opener);
    tabs::TabInterface* new_tab =
        tabs::TabInterface::MaybeGetFromContents(new_contents);
    EXPECT_EQ(new_tab_first ? new_tab : opener_tab, tabs[0]);
    EXPECT_EQ(new_tab_first ? opener_tab : new_tab, tabs[1]);
    EXPECT_EQ(layout, split_data->visual_data()->split_layout());
    EXPECT_DOUBLE_EQ(first_tab_ratio, split_data->visual_data()->split_ratio());
    // The new tab is focused.
    EXPECT_EQ(new_contents, tab_strip_model()->GetActiveWebContents());
  }

  // Expects `new_contents` to be an ordinary tab in the same window.
  void ExpectNewTab(content::WebContents* new_contents) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model()->GetIndexOfWebContents(new_contents));
    EXPECT_FALSE(GetSplitData(new_contents));
  }

  // Expects `new_contents` to be in a popup window.
  void ExpectPopup(content::WebContents* new_contents) {
    EXPECT_EQ(TabStripModel::kNoTab,
              tab_strip_model()->GetIndexOfWebContents(new_contents));
    tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(new_contents);
    ASSERT_TRUE(tab);
    EXPECT_EQ(BrowserWindowInterface::TYPE_POPUP,
              tab->GetBrowserWindowInterface()->GetType());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, Split) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents = OpenWindow(opener, "split");
  EXPECT_EQ(2, tab_strip_model()->count());
  // The default is the new content on the right, taking half of the split.
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);

  // The opener relationship is preserved.
  EXPECT_EQ(true, content::EvalJs(opener, "window.w !== null"));
  EXPECT_EQ(true, content::EvalJs(new_contents, "window.opener !== null"));
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, PositionLeft) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=left,splitsize=30");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kSideBySide, 0.3);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, PositionRight) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=right,splitsize=30");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.7);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, PositionTop) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=top,splitsize=40");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kStacked, 0.4);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, PositionBottom) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=bottom,splitsize=40");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kStacked, 0.6);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, SizeClampedSmall) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=left,splitsize=1");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kSideBySide, 0.1);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, SizeClampedLarge) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=left,splitsize=99");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kSideBySide, 0.9);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, SplitWinsOverPopup) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,popup,width=400,height=300");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, NoOpener) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents = OpenWindow(opener, "split,noopener");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);
  EXPECT_EQ(true, content::EvalJs(opener, "window.w === null"));
  EXPECT_EQ(true, content::EvalJs(new_contents, "window.opener === null"));
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, CloseEndsSplit) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents = OpenWindow(opener, "split");
  ASSERT_TRUE(GetSplitData(opener));

  content::WebContentsDestroyedWatcher destroyed_watcher(new_contents);
  ASSERT_TRUE(content::ExecJs(opener, "window.w.close();"));
  destroyed_watcher.Wait();

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(GetSplitData(opener));
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, SelfCloseEndsSplit) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents = OpenWindow(opener, "split");
  ASSERT_TRUE(GetSplitData(opener));

  content::WebContentsDestroyedWatcher destroyed_watcher(new_contents);
  ASSERT_TRUE(content::ExecJs(new_contents, "window.close();"));
  destroyed_watcher.Wait();

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(GetSplitData(opener));
}

// A request from an already-split tab falls back to a new tab, and does not
// replace the other side of the split.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, AlreadySplit) {
  content::WebContents* opener = opener_contents();
  content::WebContents* first_split = OpenWindow(opener, "split");
  ASSERT_TRUE(GetSplitData(opener));
  tab_strip_model()->ActivateTab(
      tabs::TabInterface::MaybeGetFromContents(opener));

  content::WebContents* new_contents = OpenWindow(opener, "split");
  EXPECT_EQ(3, tab_strip_model()->count());
  ExpectNewTab(new_contents);
  // The existing split is unchanged.
  EXPECT_EQ(GetSplitData(opener), GetSplitData(first_split));
}

// When the split can't be created and "popup" was also requested, the
// fallback is a popup.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, AlreadySplitWithPopup) {
  content::WebContents* opener = opener_contents();
  OpenWindow(opener, "split");
  ASSERT_TRUE(GetSplitData(opener));
  tab_strip_model()->ActivateTab(
      tabs::TabInterface::MaybeGetFromContents(opener));

  content::WebContents* new_contents = OpenWindow(opener, "split,popup");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectPopup(new_contents);
}

// A request from a tab that isn't active falls back to a new tab.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, OpenerNotActive) {
  content::WebContents* opener = opener_contents();
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title3.html"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(2, tab_strip_model()->count());
  ASSERT_NE(opener, tab_strip_model()->GetActiveWebContents());

  content::WebContents* new_contents = OpenWindow(opener, "split");
  EXPECT_EQ(3, tab_strip_model()->count());
  ExpectNewTab(new_contents);
  EXPECT_FALSE(GetSplitData(opener));
}

// The user's click modifiers take precedence over a split request, and are
// handled as they would be without it. Ctrl-click (cmd-click on Mac) opens a
// background tab.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, NewTabModifierClick) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      ClickToOpenWindow(opener, kNewTabModifier, "split");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectNewTab(new_contents);
  EXPECT_EQ(opener, tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(GetSplitData(opener));
}

// Ctrl+shift-click (cmd+shift-click on Mac) opens a foreground tab. This
// results in the same disposition as the fallback for a split request.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest,
                       NewForegroundTabModifierClick) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents = ClickToOpenWindow(
      opener, kNewTabModifier | blink::WebInputEvent::kShiftKey, "split");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectNewTab(new_contents);
  EXPECT_EQ(new_contents, tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(GetSplitData(opener));
}

// Shift-click opens a new window, which is a popup if "popup" was also
// requested. This results in the same disposition as the fallback for a split
// request.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest,
                       NewWindowModifierClickWithPopup) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      ClickToOpenWindow(opener, blink::WebInputEvent::kShiftKey, "split,popup");
  EXPECT_EQ(1, tab_strip_model()->count());
  ExpectPopup(new_contents);
  EXPECT_FALSE(GetSplitData(opener));
}

// The split view click modifier creates a split with the default position and
// size, ignoring the hints.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, SplitViewModifierClick) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      ClickToOpenWindow(opener, kNewTabModifier | blink::WebInputEvent::kAltKey,
                        "split,splitposition=left,splitsize=30");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);
}

class WindowOpenSplitTabNoStackedBrowserTest
    : public WindowOpenSplitTabBrowserTest {
 public:
  WindowOpenSplitTabNoStackedBrowserTest() {
    feature_list_.InitAndDisableFeature(tabs::kSplitViewHorizontal);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Without stacked split support, top/bottom fall back to left/right.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabNoStackedBrowserTest, PositionTop) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=top,splitsize=40");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kSideBySide, 0.4);
}

IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabNoStackedBrowserTest, PositionBottom) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenWindow(opener, "split,splitposition=bottom,splitsize=40");
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.6);
}

class WindowOpenSplitTabDisabledBrowserTest : public InProcessBrowserTest {
 public:
  WindowOpenSplitTabDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(blink::features::kWindowOpenSplitTab);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When the feature is disabled, "split" is an unrecognized feature, so the
// legacy popup behavior applies. Disabling the base::Feature also disables the
// Blink feature, so the renderer does not send split requests.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabDisabledBrowserTest, Split) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* opener =
      browser()->GetTabStripModel()->GetActiveWebContents();

  content::WebContentsAddedObserver observer;
  ASSERT_TRUE(content::ExecJs(
      opener,
      content::JsReplace("window.open($1, '_blank', 'split');",
                         embedded_test_server()->GetURL("/title2.html"))));
  content::WebContents* new_contents = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(new_contents);
  ASSERT_TRUE(tab);
  EXPECT_EQ(BrowserWindowInterface::TYPE_POPUP,
            tab->GetBrowserWindowInterface()->GetType());
}

// Clicking an `<a windowfeatures="split">` without a `target` attribute
// implies `target="_blank"` (and `noopener`) and creates a split tab.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest,
                       AnchorWindowFeaturesSplitImpliesTargetBlank) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenViaElement(opener, "a", "split, splitposition=left, splitsize=30");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kSideBySide, 0.3);

  // Implied target="_blank" implies noopener by default.
  EXPECT_EQ(true, content::EvalJs(new_contents, "window.opener === null"));
}

// `<a windowfeatures="split" rel="opener">` preserves `window.opener`.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest,
                       AnchorWindowFeaturesSplitWithRelOpener) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenViaElement(opener, "a", "split", /*rel=*/"opener");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);
  EXPECT_EQ(true, content::EvalJs(new_contents, "window.opener !== null"));
}

// `windowfeatures` is ignored when `target="_self"` navigates an existing
// frame.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest,
                       AnchorWindowFeaturesIgnoredForTargetSelf) {
  content::WebContents* contents = opener_contents();
  GURL target_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver nav_observer(contents);
  ASSERT_TRUE(content::ExecJs(
      contents, content::JsReplace("const a = document.createElement('a');"
                                   "a.href = $1;"
                                   "a.target = '_self';"
                                   "a.windowFeatures = 'split';"
                                   "document.body.appendChild(a);"
                                   "a.click();",
                                   target_url)));
  nav_observer.Wait();

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(GetSplitData(contents));
  EXPECT_EQ(target_url, contents->GetLastCommittedURL());
}

// `<area windowfeatures="split">` creates a split tab.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, AreaWindowFeaturesSplit) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents = OpenViaElement(opener, "area", "split");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectSplit(opener, new_contents, /*new_tab_first=*/false,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);
}

// Submitting a `<form windowfeatures="split">` creates a split tab and implies
// `target="_blank"`.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest, FormWindowFeaturesSplit) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenViaElement(opener, "form", "split, splitposition=left");
  EXPECT_EQ(2, tab_strip_model()->count());
  ExpectSplit(opener, new_contents, /*new_tab_first=*/true,
              split_tabs::SplitTabLayout::kSideBySide, 0.5);
  EXPECT_EQ(true, content::EvalJs(new_contents, "window.opener === null"));
}

// `<a windowfeatures="popup, width=400, height=300">` opens a popup window and
// implies `target="_blank"`.
IN_PROC_BROWSER_TEST_F(WindowOpenSplitTabBrowserTest,
                       AnchorWindowFeaturesPopup) {
  content::WebContents* opener = opener_contents();
  content::WebContents* new_contents =
      OpenViaElement(opener, "a", "popup, width=400, height=300");
  EXPECT_EQ(1, tab_strip_model()->count());
  ExpectPopup(new_contents);
  EXPECT_EQ(true, content::EvalJs(new_contents, "window.opener === null"));
}
