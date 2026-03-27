// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_registry.h"

#include "chrome/browser/ui/side_panel/test/android/side_panel_android_browser_test_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class SidePanelRegistryAndroidBrowserTest
    : public SidePanelAndroidBrowserTestBase {
 protected:
  using SidePanelAndroidBrowserTestBase::GetActiveTab;
  using SidePanelAndroidBrowserTestBase::GetBrowserWindow;
};

IN_PROC_BROWSER_TEST_F(SidePanelRegistryAndroidBrowserTest,
                       From_DifferentRegistriesForWindowAndTab) {
  SidePanelRegistry* window_scoped_registry =
      SidePanelRegistry::From(GetBrowserWindow());
  SidePanelRegistry* tab_scoped_registry =
      SidePanelRegistry::From(GetActiveTab());

  EXPECT_NE(nullptr, window_scoped_registry);
  EXPECT_NE(nullptr, tab_scoped_registry);
  EXPECT_NE(window_scoped_registry, tab_scoped_registry);
}
