// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.url.GURL;

import java.util.List;

/**
 * This component supports accounts selection for WebID. It shows a list of
 * accounts to the user which can select one of them or dismiss it.
 */
public interface AccountSelectionComponent {
    /**
     * This delegate is called when the AccountSelection component is interacted with (e.g.
     * dismissed or a suggestion was selected).
     */
    interface Delegate {
        /**
         * Called when the user select one of the accounts shown in the
         * AccountSelectionComponent.
         */
        void onAccountSelected(GURL idpConfigUrl, Account account);

        /**
         * Called when the user dismisses the AccountSelectionComponent. Not called if a suggestion
         * was selected.
         */
        void onDismissed(@IdentityRequestDialogDismissReason int dismissReason);
    }

    /**
     * Displays the given accounts in a new bottom sheet.
     * @param topFrameEtldPlusOne The {@link String} for the relying party's top frame.
     * @param iframeEtldPlusOne The {@link String} for the relying party's iframe.
     * @param idpEtldPlusOne The {@link String} for the identity provider.
     * @param accounts A list of {@link Account}s that will be displayed.
     * @param idpMetadata Metadata related to identity provider.
     * @param clientMetadata Metadata related to relying party.
     * @param isAutoReauthn A {@link boolean} that represents whether this is an auto re-authn flow.
     * @param rpContext is a {@link String} representing the desired text to be used in the title of
     *         the FedCM prompt: "signin", "continue", etc.

     */
    void showAccounts(String topFrameEtldPlusOne, String iframeEtldPlusOne, String idpEtldPlusOne,
            List<Account> accounts, IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata, boolean isAutoReauthn, String rpContext);

    /**
     * Closes the outstanding bottom sheet.
     */
    void close();

    /**
     * Gets the sheet's title.
     */
    String getTitle();
    /**
     * Gets the sheet's subtitle, if any, or null..
     */
    String getSubtitle();
}
