// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/overscroll_pref_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using OverscrollPrefManagerTest = InProcessBrowserTest;

// Tests that changing the pref immediately enables or disables overscroll.
IN_PROC_BROWSER_TEST_F(OverscrollPrefManagerTest, PrefChange) {
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(
      local_state->GetBoolean(prefs::kOverscrollHistoryNavigationEnabled));
  ASSERT_TRUE(browser()->CanOverscrollContent());

  local_state->SetBoolean(prefs::kOverscrollHistoryNavigationEnabled, false);
  EXPECT_FALSE(browser()->CanOverscrollContent());

  local_state->SetBoolean(prefs::kOverscrollHistoryNavigationEnabled, true);
  EXPECT_TRUE(browser()->CanOverscrollContent());
}
