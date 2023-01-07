// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace send_tab_to_self {

class SendTabToSelfToolbarBubbleViewTest : public ChromeViewsTestBase {};

TEST_F(SendTabToSelfToolbarBubbleViewTest, ButtonNavigatesToPage) {
  GURL url("https://www.example.com");
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid");
  SendTabToSelfToolbarBubbleView bubble(
      nullptr, nullptr, entry,
      base::BindLambdaForTesting([&](NavigateParams* params) {
        EXPECT_EQ("https://www.example.com", params->url.spec());
        EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  params->disposition);
        EXPECT_EQ(NavigateParams::SHOW_WINDOW, params->window_action);
      }));
}

}  // namespace send_tab_to_self
