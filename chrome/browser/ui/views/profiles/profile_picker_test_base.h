// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}

namespace views {
class View;
class WebView;
class Widget;
}  // namespace views

class GURL;

class ProfilePickerTestBase : public InProcessBrowserTest {
 public:
  ProfilePickerTestBase();
  ~ProfilePickerTestBase() override;

  // Returns the ProfilePickerView that is currently displayed.
  views::View* view();

  // Returns the widget associated with the profile picker.
  views::Widget* widget();

  // Returns the internal web view for the profile picker.
  views::WebView* web_view();

  // Waits until a relayout of the main view has been performed. This implies
  // the appropriate web_contents() is attached to the layout.
  void WaitForLayoutWithToolbar();
  void WaitForLayoutWithoutToolbar();

  // Waits until the web contents does the first non-empty paint for `url`.
  void WaitForFirstPaint(content::WebContents* contents, const GURL& url);

  // Waits until the picker gets closed.
  void WaitForPickerClosed();

  // Gets the picker's web contents.
  content::WebContents* web_contents();

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_TEST_BASE_H_
