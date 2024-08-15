// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_interface.h"

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

class TabInterfaceBrowserTest : public PlatformBrowserTest {
 public:
  tabs::TabInterface* GetActiveTabInterface() {
    return chrome_test_utils::GetActiveTabInterface(this);
  }
};

IN_PROC_BROWSER_TEST_F(TabInterfaceBrowserTest, ActiveTabIsInForeground) {
  auto* active_tab = GetActiveTabInterface();
  ASSERT_TRUE(active_tab);
  EXPECT_TRUE(active_tab->IsInForeground());
}
