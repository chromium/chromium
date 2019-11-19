// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_links_delegate_impl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"

OneClickSigninLinksDelegateImpl::OneClickSigninLinksDelegateImpl(
    Browser* browser)
    : browser_(browser) {}

OneClickSigninLinksDelegateImpl::~OneClickSigninLinksDelegateImpl() {}

void OneClickSigninLinksDelegateImpl::OnLearnMoreLinkClicked(bool is_dialog) {
  NavigateParams params(browser_, GURL(chrome::kChromeSyncLearnMoreURL),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = is_dialog ? WindowOpenDisposition::NEW_WINDOW
                                 : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}
