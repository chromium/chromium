// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_MAC_H_

#import "chrome/browser/ui/cocoa/tab_contents/chrome_web_contents_view_delegate_mac.h"

class ChromeWebContentsViewFocusHelper;

// A MacViews specific class that extends ChromeWebContentsViewDelegateMac.
class ChromeWebContentsViewDelegateViewsMac
    : public ChromeWebContentsViewDelegateMac {
 public:
  explicit ChromeWebContentsViewDelegateViewsMac(
      content::WebContents* web_contents);
  ~ChromeWebContentsViewDelegateViewsMac() override;

  // ChromeWebContentsViewDelegateMac:
  void StoreFocus() override;
  bool RestoreFocus() override;
  void ResetStoredFocus() override;
  bool Focus() override;
  bool TakeFocus(bool reverse) override;
  void OnPerformDrop(const content::DropData& drop_data,
                     DropCompletionCallback callback) override;

 private:
  content::WebContents* web_contents_;

  ChromeWebContentsViewFocusHelper* GetFocusHelper() const;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewDelegateViewsMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_MAC_H_
