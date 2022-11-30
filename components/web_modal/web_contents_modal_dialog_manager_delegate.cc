// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

#include <string.h>

namespace web_modal {

void WebContentsModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents, bool blocked) {
}

WebContentsModalDialogHost*
    WebContentsModalDialogManagerDelegate::GetWebContentsModalDialogHost() {
  return nullptr;
}

bool WebContentsModalDialogManagerDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return true;
}

WebContentsModalDialogManagerDelegate::~WebContentsModalDialogManagerDelegate(
) {}

}  // namespace web_modal
