// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_BUBBLE_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_BUBBLE_H_

class AuthenticatorRequestDialogModel;

namespace content {
class WebContents;
}

void ShowAuthenticatorRequestBubble(content::WebContents* web_contents,
                                    AuthenticatorRequestDialogModel* model);

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_BUBBLE_H_
