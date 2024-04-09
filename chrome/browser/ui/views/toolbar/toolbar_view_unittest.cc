// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class ToolbarViewUnitTest : public TestWithBrowserView {
 public:
  ToolbarButton* GetForwardButton() {
    return browser_view()->toolbar()->forward_button();
  }
};

TEST_F(ToolbarViewUnitTest, ForwardButtonVisibility) {
  // Forward button should be visible by default.
  EXPECT_TRUE(GetForwardButton()->GetVisible());

  browser_view()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kShowForwardButton, false);
  EXPECT_FALSE(GetForwardButton()->GetVisible());
}
