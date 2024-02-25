// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webauthn;

import org.chromium.components.webauthn.AuthenticatorFactory;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.components.webauthn.WebauthnModeProvider.WebauthnMode;
import org.chromium.content_public.browser.RenderFrameHost;

public class ChromeAuthenticatorFactory extends AuthenticatorFactory {
    public ChromeAuthenticatorFactory(RenderFrameHost renderFrameHost) {
        super(renderFrameHost, new ChromeAuthenticatorConfirmationFactory());
        WebauthnModeProvider.getInstance().setWebauthnMode(WebauthnMode.CHROME);
    }
}
