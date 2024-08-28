// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_LINK_CAPTURING_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_LINK_CAPTURING_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace views {
class Button;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

class Browser;

namespace web_app {

// These test functions work only with the new intent picker UX and requires the
// following flags to be enabled: features::kPwaNavigationCapturing on
// Windows, Mac and Linux. apps::features::kLinkCapturingUiUpdate on CrOS.
// Without these flags set on their respective platforms, the tests will CHECK
// fail.
IntentChipButton* GetIntentPickerIcon(Browser* browser);

IntentPickerBubbleView* intent_picker_bubble();

testing::AssertionResult AwaitIntentPickerTabHelperIconUpdateComplete(
    content::WebContents* web_contents);

testing::AssertionResult WaitForIntentPickerToShow(Browser* browser);

testing::AssertionResult ClickIntentPickerChip(Browser* browser);

testing::AssertionResult ClickIntentPickerAndWaitForBubble(Browser* browser);

views::Button* GetIntentPickerButtonAtIndex(size_t index);

// Testing utility to wait for the IntentChipButton to be visible. The correct
// usage for this class is: apps::IntentChipVisibilityObserver
// visibility_observer(intent_chip); <Do something to make the chip visible>
// visibility_observer.WaitForChipToBeVisible();
class IntentChipVisibilityObserver : public OmniboxChipButton::Observer {
 public:
  explicit IntentChipVisibilityObserver(IntentChipButton* intent_chip);
  ~IntentChipVisibilityObserver() override;

  IntentChipVisibilityObserver(const IntentChipVisibilityObserver&) = delete;
  IntentChipVisibilityObserver& operator=(const IntentChipVisibilityObserver&) =
      delete;

  void WaitForChipToBeVisible();

 private:
  void OnChipVisibilityChanged(bool is_visible) override;
  base::ScopedObservation<IntentChipButton, OmniboxChipButton::Observer>
      observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_LINK_CAPTURING_TEST_UTILS_H_
