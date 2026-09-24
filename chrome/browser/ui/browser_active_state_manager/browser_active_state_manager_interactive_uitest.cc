// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

using BrowserActiveStateManagerInteractiveUiTest = InProcessBrowserTest;

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserActiveStateManagerInteractiveUiTest,
                       MultiWindowStartupActivationOrder) {
  GlobalBrowserCollection* global_collection =
      GlobalBrowserCollection::GetInstance();
  ASSERT_NE(global_collection, nullptr);

  // Simulate multi-window startup / session restore where two windows are
  // created and shown synchronously, and then the first window is activated
  // before native widget activation events have been processed.
  BrowserWindowInterface* browser1 = CreateBrowser(GetProfile());
  BrowserWindowInterface* browser2 = CreateBrowser(GetProfile());
  EXPECT_EQ(global_collection->GetLastActiveBrowser(), browser2);
  EXPECT_FALSE(browser1->IsActive());
  EXPECT_TRUE(browser2->IsActive());

  browser1->GetWindow()->Activate();
  EXPECT_EQ(global_collection->GetLastActiveBrowser(), browser1);
  EXPECT_TRUE(browser1->IsActive());
  EXPECT_FALSE(browser2->IsActive());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
