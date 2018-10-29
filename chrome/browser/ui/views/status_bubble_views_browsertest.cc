// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_bubble_views.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/status_bubble_views_browsertest_mac.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/widget/widget.h"

class StatusBubbleViewsTest : public InProcessBrowserTest {
 public:
  StatusBubble* GetBubble() { return browser()->window()->GetStatusBubble(); }
  views::Widget* GetWidget() {
    return static_cast<StatusBubbleViews*>(GetBubble())->popup();
  }
};

// Ensure the status bubble does not hide itself on Mac. Doing so can trigger
// unwanted space switches due to rdar://9037452. See https://crbug.com/866760.
IN_PROC_BROWSER_TEST_F(StatusBubbleViewsTest, NeverHideOnMac) {
  StatusBubble* bubble = GetBubble();
  ASSERT_TRUE(bubble);
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);

  // The status bubble has never been shown.
  EXPECT_FALSE(widget->IsVisible());

  // Setting status shows the widget.
  bubble->SetStatus(base::ASCIIToUTF16("test"));
  EXPECT_TRUE(widget->IsVisible());

#if defined(OS_MACOSX)
  // Check alpha on Mac as well. On other platforms it is redundant.
  EXPECT_EQ(1.f, test::GetNativeWindowAlphaValue(widget->GetNativeWindow()));
  bubble->Hide();
  // On Mac, the bubble widget remains visible so it can remain a child window.
  // However, it is fully transparent.
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(0.f, test::GetNativeWindowAlphaValue(widget->GetNativeWindow()));
#else
  bubble->Hide();
  EXPECT_FALSE(widget->IsVisible());
#endif
}
