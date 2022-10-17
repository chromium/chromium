// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_modal_delegate.h"

#include "content/public/browser/web_contents.h"

namespace ash {

InlineLoginHandlerModalDelegate::InlineLoginHandlerModalDelegate(
    web_modal::WebContentsModalDialogHost* host)
    : host_(host) {}

InlineLoginHandlerModalDelegate::~InlineLoginHandlerModalDelegate() = default;

web_modal::WebContentsModalDialogHost*
InlineLoginHandlerModalDelegate::GetWebContentsModalDialogHost() {
  return host_;
}

}  // namespace ash
