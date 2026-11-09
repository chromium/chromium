// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/window_creation.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace waap {

using WaapWindowCreationBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WaapWindowCreationBrowserTest,
                       CreateBrowserWindowAndRecordCreationSource) {
  BrowserWindowInterface* new_browser =
      CreateBrowserWindowAndRecordCreationSource(
          BrowserWindowCreateParams(*browser()->GetProfile(),
                                    /*from_user_gesture=*/false),
          NewWindowCreationSource::kNewEmptyWindow);

  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(BrowserWindowInterface::TYPE_NORMAL, new_browser->GetType());
  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  EXPECT_EQ(NewWindowCreationSource::kNewEmptyWindow,
            manager->GetCreationSourceForTesting());
}

}  // namespace waap
