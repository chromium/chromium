// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class ExtensionsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ExtensionsBrowserTest() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }
};

////////////////////////////////////////////////////////////////////////////////
// Extension Sidebar Tests

class CrExtensionsSidebarTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/sidebar_test.js",
        base::StringPrintf("runMochaTest('ExtensionSidebarTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsSidebarTest, HrefVerification) {
  RunTestCase("HrefVerification");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsSidebarTest, LayoutAndClickHandlers) {
  RunTestCase("LayoutAndClickHandlers");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsSidebarTest, SetSelected) {
  RunTestCase("SetSelected");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Toolbar Tests

class CrExtensionsToolbarTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/toolbar_test.js",
        base::StringPrintf("runMochaTest('ExtensionToolbarTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, DevModeToggle) {
  RunTestCase("DevModeToggle");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, FailedUpdateFiresLoadError) {
  RunTestCase("FailedUpdateFiresLoadError");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, NarrowModeShowsMenu) {
  RunTestCase("NarrowModeShowsMenu");
}

// TODO(crbug.com/882342) Disabled on other platforms but MacOS due to timeouts.
#if !BUILDFLAG(IS_MAC)
#define MAYBE_ClickHandlers DISABLED_ClickHandlers
#else
#define MAYBE_ClickHandlers ClickHandlers
#endif
IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, MAYBE_ClickHandlers) {
  RunTestCase("ClickHandlers");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(CrExtensionsToolbarTest, KioskMode) {
  RunTestCase("KioskMode");
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Extension Item Tests

class CrExtensionsItemsTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/item_test.js",
        base::StringPrintf("runMochaTest('ExtensionItemTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, NormalState) {
  RunTestCase("ElementVisibilityNormalState");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, DeveloperState) {
  RunTestCase("ElementVisibilityDeveloperState");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, ClickableItems) {
  RunTestCase("ClickableItems");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, FailedReloadFiresLoadError) {
  RunTestCase("FailedReloadFiresLoadError");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, Warnings) {
  RunTestCase("Warnings");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, SourceIndicator) {
  RunTestCase("SourceIndicator");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, EnableToggle) {
  RunTestCase("EnableToggle");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, RemoveButton) {
  RunTestCase("RemoveButton");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, HtmlInName) {
  RunTestCase("HtmlInName");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, RepairButton) {
  RunTestCase("RepairButton");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemsTest, InspectableViewSortOrder) {
  RunTestCase("InspectableViewSortOrder");
}

typedef ExtensionsBrowserTest CrExtensionsActivityLogTest;
IN_PROC_BROWSER_TEST_F(CrExtensionsActivityLogTest, All) {
  RunTest("extensions/activity_log_test.js", "mocha.run()");
}

typedef ExtensionsBrowserTest CrExtensionsActivityLogHistoryTest;
IN_PROC_BROWSER_TEST_F(CrExtensionsActivityLogHistoryTest, All) {
  RunTest("extensions/activity_log_history_test.js", "mocha.run()");
}

typedef ExtensionsBrowserTest CrExtensionsActivityLogHistoryItemTest;
IN_PROC_BROWSER_TEST_F(CrExtensionsActivityLogHistoryItemTest, All) {
  RunTest("extensions/activity_log_history_item_test.js", "mocha.run()");
}

typedef ExtensionsBrowserTest CrExtensionsActivityLogStreamTest;
IN_PROC_BROWSER_TEST_F(CrExtensionsActivityLogStreamTest, All) {
  RunTest("extensions/activity_log_stream_test.js", "mocha.run()");
}

typedef ExtensionsBrowserTest CrExtensionsActivityLogStreamItemTest;
IN_PROC_BROWSER_TEST_F(CrExtensionsActivityLogStreamItemTest, All) {
  RunTest("extensions/activity_log_stream_item_test.js", "mocha.run()");
}

class CrExtensionsDetailViewTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/detail_view_test.js",
        base::StringPrintf("runMochaTest('ExtensionDetailViewTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Layout) {
  RunTestCase("Layout");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, LayoutSource) {
  RunTestCase("LayoutSource");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       SupervisedUserDisableReasons) {
  RunTestCase("SupervisedUserDisableReasons");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, ClickableElements) {
  RunTestCase("ClickableElements");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Indicator) {
  RunTestCase("Indicator");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, Warnings) {
  RunTestCase("Warnings");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       NoSiteAccessWithEnhancedSiteControls) {
  RunTestCase("NoSiteAccessWithEnhancedSiteControls");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, InspectableViewSortOrder) {
  RunTestCase("InspectableViewSortOrder");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest,
                       ShowAccessRequestsInToolbar) {
  RunTestCase("ShowAccessRequestsInToolbar");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsDetailViewTest, SafetyCheckWarning) {
  RunTestCase("SafetyCheckWarning");
}

////////////////////////////////////////////////////////////////////////////////
// Extension Item List Tests

class CrExtensionsItemListTest : public ExtensionsBrowserTest {
 protected:
  void RunTestCase(const std::string& testCase) {
    ExtensionsBrowserTest::RunTest(
        "extensions/item_list_test.js",
        base::StringPrintf("runMochaTest('ExtensionItemListTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, Filtering) {
  RunTestCase("Filtering");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, NoItems) {
  RunTestCase("NoItems");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, NoSearchResults) {
  RunTestCase("NoSearchResults");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, LoadTimeData) {
  RunTestCase("LoadTimeData");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsItemListTest, SafetyCheckPanel) {
  RunTestCase("SafetyCheckPanel");
}
