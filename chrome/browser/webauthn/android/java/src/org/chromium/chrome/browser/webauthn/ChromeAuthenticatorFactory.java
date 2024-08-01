// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webauthn;

import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.webauthn.AuthenticatorFactory;
import org.chromium.components.webauthn.WebauthnMode;
import org.chromium.components.webauthn.WebauthnModeProvider;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContentsStatics;

import java.util.Optional;

public class ChromeAuthenticatorFactory extends AuthenticatorFactory {
    public ChromeAuthenticatorFactory(RenderFrameHost renderFrameHost) {
        super(renderFrameHost, new ChromeAuthenticatorConfirmationFactory());
        @AndroidAutofillAvailabilityStatus
        int autofillStatus =
                Optional.ofNullable(WebContentsStatics.fromRenderFrameHost(renderFrameHost))
                        .map(Profile::fromWebContents)
                        .map(UserPrefs::get)
                        .map(AutofillClientProviderUtils::getAndroidAutofillFrameworkAvailability)
                        .orElse(AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
        WebauthnModeProvider.getInstance()
                .setGlobalWebauthnMode(
                        autofillStatus == AndroidAutofillAvailabilityStatus.AVAILABLE
                                ? WebauthnMode.CHROME_3PP_ENABLED
                                : WebauthnMode.CHROME);
    }
}
