// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_FOCUS_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_FOCUS_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}

namespace views {
class FocusManager;
class Widget;
class View;
}

// A chrome specific helper class that handles focus management.
class ChromeWebContentsViewFocusHelper
    : public content::WebContentsUserData<ChromeWebContentsViewFocusHelper> {
 public:
  ChromeWebContentsViewFocusHelper(const ChromeWebContentsViewFocusHelper&) =
      delete;
  ChromeWebContentsViewFocusHelper& operator=(
      const ChromeWebContentsViewFocusHelper&) = delete;

  void StoreFocus();
  bool RestoreFocus();
  void ResetStoredFocus();
  bool Focus();
  bool TakeFocus(bool reverse);
  // Returns the View that will be focused if RestoreFocus() is called.
  views::View* GetStoredFocus();

 private:
  explicit ChromeWebContentsViewFocusHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromeWebContentsViewFocusHelper>;
  gfx::NativeView GetActiveNativeView();
  views::Widget* GetTopLevelWidget();
  views::FocusManager* GetFocusManager();

  // Used to store the last focused view.
  views::ViewTracker last_focused_view_tracker_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_FOCUS_HELPER_H_
