// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

PasswordsClientUIDelegate* PasswordsClientUIDelegateFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return ManagePasswordsUIController::FromWebContents(web_contents);
}
