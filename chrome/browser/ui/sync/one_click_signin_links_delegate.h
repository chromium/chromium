// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_H_

#include "base/macros.h"

// Handles link clicks in the OneClickSigninDialogView.
class OneClickSigninLinksDelegate {
 public:
  virtual ~OneClickSigninLinksDelegate() {}
  virtual void OnLearnMoreLinkClicked(bool is_dialog) = 0;

 protected:
  OneClickSigninLinksDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OneClickSigninLinksDelegate);
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_H_
