// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_H_

#include "base/memory/scoped_refptr.h"

struct AuthenticatorRequestDialogModel;

namespace content {
class WebContents;
}

// Creates and shows the dialog for a given WebContents.
// |model| must outlive the dialog.
void ShowAuthenticatorRequestDialog(
    content::WebContents* web_contents,
    scoped_refptr<AuthenticatorRequestDialogModel> model);

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_H_
