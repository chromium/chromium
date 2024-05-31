// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/browser/ui/views/shortcuts/shortcut_integration_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace shortcuts {

namespace {

// Identifier for the initially created tab.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInitialTabId);
// Identifier for the tab opened as a result of launching a shortcut.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
// Identifier for the shortcut created in these tests.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewShortcutId);

}  // namespace

using ShortcutIntegrationBrowserTest = ShortcutIntegrationBrowserTestBase;

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationBrowserTest, CreateAndLaunch) {
  const GURL kPageWithIconsUrl =
      embedded_https_test_server().GetURL("/shortcuts/page_icons.html");
  RunTestSequence(
      InstrumentTab(kInitialTabId),
      NavigateWebContents(kInitialTabId, kPageWithIconsUrl),
      InstrumentNextShortcut(kNewShortcutId),
      ShowAndAcceptCreateShortcutDialog(),
      CheckShortcut(kNewShortcutId, IsShortcutForUrl(kPageWithIconsUrl)),
      InstrumentNextTab(kNewTabId), LaunchShortcut(kNewShortcutId),
      WaitForWebContentsNavigation(kNewTabId, kPageWithIconsUrl));
}

IN_PROC_BROWSER_TEST_F(ShortcutIntegrationBrowserTest, CustomTitle) {
  const GURL kPageWithIconsUrl =
      embedded_https_test_server().GetURL("/shortcuts/page_icons.html");
  RunTestSequence(
      InstrumentTab(kInitialTabId),
      NavigateWebContents(kInitialTabId, kPageWithIconsUrl),
      InstrumentNextShortcut(kNewShortcutId),
      ShowCreateShortcutDialogSetTitleAndAccept(u"Hello World!"),
      CheckShortcut(kNewShortcutId, IsShortcutWithTitle(u"Hello World!")));
}

}  // namespace shortcuts
