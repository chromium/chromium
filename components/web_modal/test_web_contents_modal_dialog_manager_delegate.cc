// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"

#include "content/public/browser/web_contents.h"

namespace web_modal {

TestWebContentsModalDialogManagerDelegate::
    TestWebContentsModalDialogManagerDelegate()
    : web_contents_visible_(true),
      web_contents_blocked_(false),
      web_contents_modal_dialog_host_(nullptr) {}

void TestWebContentsModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  web_contents_blocked_ = blocked;
}

WebContentsModalDialogHost*
TestWebContentsModalDialogManagerDelegate::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  return web_contents_modal_dialog_host_;
}

bool TestWebContentsModalDialogManagerDelegate::IsWebContentsVisible(
  content::WebContents* web_contents) {
  return web_contents_visible_;
}

void TestWebContentsModalDialogManagerDelegate::OnWebContentsModalDialogShown(
    content::WebContents* web_contents) {
  web_contents_activated_ = true;
}

}  // namespace web_modal
