// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import androidx.annotation.Nullable;

import org.chromium.components.webauthn.CreateConfirmationUiDelegate;
import org.chromium.content_public.browser.WebContents;

/**
 * A factory class to create a {@link CreateConfirmationUiDelegate} using {@link
 * AuthenticatorIncognitoConfirmationBottomsheet}
 */
public class ChromeAuthenticatorConfirmationFactory
        implements CreateConfirmationUiDelegate.Factory {
    @Override
    public @Nullable CreateConfirmationUiDelegate create(WebContents webContents) {
        if (webContents.isIncognito()) {
            return (accept, reject) -> {
                var sheet = new AuthenticatorIncognitoConfirmationBottomsheet(webContents);
                return sheet.show(accept, reject);
            };
        }
        return null;
    }
}
