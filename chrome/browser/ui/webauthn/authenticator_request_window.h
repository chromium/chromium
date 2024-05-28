// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_WINDOW_H_
#define CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_WINDOW_H_

namespace content {
class WebContents;
}

class GURL;

struct AuthenticatorRequestDialogModel;

// Open a dialog window to render a step of the WebAuthn UI.
void ShowAuthenticatorRequestWindow(content::WebContents* web_contents,
                                    AuthenticatorRequestDialogModel* model);

// Returns true if the specified `url` can be served by the WebAuthn UI.
bool IsAuthenticatorRequestWindowUrl(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBAUTHN_AUTHENTICATOR_REQUEST_WINDOW_H_
