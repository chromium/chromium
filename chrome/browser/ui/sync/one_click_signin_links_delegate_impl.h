// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/sync/one_click_signin_links_delegate.h"

class Browser;

class OneClickSigninLinksDelegateImpl : public OneClickSigninLinksDelegate {
 public:
  // |browser| must outlive the delegate.
  explicit OneClickSigninLinksDelegateImpl(Browser* browser);
  ~OneClickSigninLinksDelegateImpl() override;

 private:
  // OneClickSigninLinksDelegate:
  void OnLearnMoreLinkClicked(bool is_dialog) override;

  // Browser in which the links should be opened.
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninLinksDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_SYNC_ONE_CLICK_SIGNIN_LINKS_DELEGATE_IMPL_H_
