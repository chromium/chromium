// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views_mac.h"

#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewDelegateViewsMac::ChromeWebContentsViewDelegateViewsMac(
    content::WebContents* web_contents)
    : ChromeWebContentsViewDelegateMac(web_contents),
      web_contents_(web_contents) {
  ChromeWebContentsViewFocusHelper::CreateForWebContents(web_contents);
}

ChromeWebContentsViewDelegateViewsMac::
    ~ChromeWebContentsViewDelegateViewsMac() {
}

ChromeWebContentsViewFocusHelper*
ChromeWebContentsViewDelegateViewsMac::GetFocusHelper() const {
  ChromeWebContentsViewFocusHelper* helper =
      ChromeWebContentsViewFocusHelper::FromWebContents(web_contents_);
  DCHECK(helper);
  return helper;
}

void ChromeWebContentsViewDelegateViewsMac::StoreFocus() {
  GetFocusHelper()->StoreFocus();
}

bool ChromeWebContentsViewDelegateViewsMac::RestoreFocus() {
  return GetFocusHelper()->RestoreFocus();
}

void ChromeWebContentsViewDelegateViewsMac::ResetStoredFocus() {
  GetFocusHelper()->ResetStoredFocus();
}

bool ChromeWebContentsViewDelegateViewsMac::Focus() {
  return GetFocusHelper()->Focus();
}

bool ChromeWebContentsViewDelegateViewsMac::TakeFocus(bool reverse) {
  return GetFocusHelper()->TakeFocus(reverse);
}

void ChromeWebContentsViewDelegateViewsMac::OnPerformDrop(
    const content::DropData& drop_data,
    DropCompletionCallback callback) {
  HandleOnPerformDrop(web_contents_, drop_data, std::move(callback));
}

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new ChromeWebContentsViewDelegateViewsMac(web_contents);
}
