// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_LINK_CAPTURING_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_LINK_CAPTURING_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace views {
class Button;
}  // namespace views

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;

namespace web_app {

// These test functions work only with the new intent picker UX and requires the
// `kPwaNavigationCapturing` flag to be set on Windows, Mac and Linux. On
// ChromeOS, this will work by default. Without these flags set on their
// respective platforms, the tests will CHECK fail.

// This test function handles the the case where intent picker migration is
// enabled to use the PageActionView of the intent picker from the
// IntentChipButton.
views::Button* GetIntentPickerButton(BrowserWindowInterface* browser);

IntentPickerBubbleView* intent_picker_bubble();

testing::AssertionResult AwaitIntentPickerTabHelperIconUpdateComplete(
    content::WebContents* web_contents);

testing::AssertionResult WaitForIntentPickerToShow(
    BrowserWindowInterface* browser);

testing::AssertionResult ClickIntentPickerChip(BrowserWindowInterface* browser);

testing::AssertionResult ClickIntentPickerAndWaitForBubble(
    BrowserWindowInterface* browser);

views::Button* GetIntentPickerButtonAtIndex(size_t index);


}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_LINK_CAPTURING_TEST_UTILS_H_
