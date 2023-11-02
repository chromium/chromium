// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_H_

// Handles link clicks in the OneClickSigninDialogView.
class OneClickSigninLinksDelegate {
 public:
  OneClickSigninLinksDelegate(const OneClickSigninLinksDelegate&) = delete;
  OneClickSigninLinksDelegate& operator=(const OneClickSigninLinksDelegate&) =
      delete;

  virtual ~OneClickSigninLinksDelegate() {}
  virtual void OnLearnMoreLinkClicked(bool is_dialog) = 0;

 protected:
  OneClickSigninLinksDelegate() {}
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_H_
