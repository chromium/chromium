// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_bubble_view.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class RelaunchRecommendedBubbleViewDialogTest : public DialogBrowserTest {
 public:
  RelaunchRecommendedBubbleViewDialogTest(
      const RelaunchRecommendedBubbleViewDialogTest&) = delete;
  RelaunchRecommendedBubbleViewDialogTest& operator=(
      const RelaunchRecommendedBubbleViewDialogTest&) = delete;

 protected:
  RelaunchRecommendedBubbleViewDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    base::Time detection_time = base::Time::Now() - base::Days(3);
    RelaunchRecommendedBubbleView::ShowBubble(browser(), detection_time,
                                              base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(RelaunchRecommendedBubbleViewDialogTest,
                       InvokeUi_default) {
  set_baseline("5855019");
  ShowAndVerifyUi();
}
