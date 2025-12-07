// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

using InteractionTestUtilBrowserUiTest = InteractiveBrowserTest;

// This test checks that we can attach to a WebUI that is embedded in a tab.
// Note: This test used to fail on the Win bot (crbug.com/1376747).
IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserUiTest,
                       CompareScreenshot_TabWebUI) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDownloadsPageElementId);

  // Set the browser view to a consistent size.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetWidget()->SetSize({600, 400});

  RunTestSequence(
      InstrumentTab(kDownloadsPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),
      WaitForWebContentsNavigation(kDownloadsPageElementId,
                                   GURL("chrome://downloads")),
      // This adds a callback that calls
      // InteractionTestUtilBrowser::CompareScreenshot().
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(kDownloadsPageElementId, /*screenshot_name=*/std::string(),
                 /*baseline_cl=*/"5406828"));
}
