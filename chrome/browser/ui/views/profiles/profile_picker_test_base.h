// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_

#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}

namespace views {
class WebView;
class Widget;
}  // namespace views

class GURL;
class ProfilePickerView;

// Mixin adding getters and helper methods to interact with `ProfilePickerView`.
class WithProfilePickerTestHelpers {
 public:
  // Returns the ProfilePickerView that is currently displayed.
  ProfilePickerView* view();

  // Returns the widget associated with the profile picker.
  views::Widget* widget();

  // Returns the internal web view for the profile picker.
  views::WebView* web_view();

  // Forwards to `profiles::testing::WaitForPickerWidgetCreated()`.
  void WaitForPickerWidgetCreated();

  // Waits until `target` WebContents stops loading `url`. If no `target` is
  // provided, it checks for the current `web_contents()` to stop loading `url`.
  // This also works if `web_contents()` changes throughout the waiting as it is
  // technically observing all web contents.
  //
  // DEPRECATED: Ambiguous call, prefer `content::WaitForLoadStop()` when
  // waiting for a specific WebContents instance to finish loading or
  // `profiles::testing::WaitForPickerUrl()` when waiting for a specific URL to
  // be loaded by the profile picker.
  void WaitForLoadStop(const GURL& url, content::WebContents* target);

  // DEPRECATED: Ambiguous call, prefer `profiles::testing::WaitForPickerUrl()`
  // instead.
  void WaitForLoadStop(const GURL& url);

  // Waits until the picker gets closed.
  void WaitForPickerClosed();

  // Waits until the picker gets closed and asserts it reopens immediately.
  void WaitForPickerClosedAndReopenedImmediately();

  // Gets the picker's web contents.
  content::WebContents* web_contents();

  // Gets signin_chrome_sync_dice with appropriate parameters appended:
  // if in dark mode, "color_scheme=dark", and always "flow=promo".
  GURL GetSigninChromeSyncDiceUrl();

  GURL GetChromeReauthURL(const std::string& email);
};

class ProfilePickerTestBase : public InProcessBrowserTest,
                              public WithProfilePickerTestHelpers {};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_
