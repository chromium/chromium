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

        /**
         * Called when the user cancels auto sign in.
         */
        void onAutoSignInCancelled();
    }

    /**
     * Displays the given accounts in a new bottom sheet.
     * @param rpEtldPlusOne The {@link String} for the relying party.
     * @param idpEtldPlusOne The {@link String} for the identity provider.
     * @param accounts A list of {@link Account}s that will be displayed.
     * @param idpMetadata Metadata related to identity provider.
     * @param clientMetadata Metadata related to relying party.
     * @param isAutoSignIn A {@link boolean} that represents whether this is an auto sign in flow.
     */
    void showAccounts(String rpEtldPlusOne, String idpEtldPlusOne, List<Account> accounts,
            IdentityProviderMetadata idpMetadata, ClientIdMetadata clientMetadata,
            boolean isAutoSignIn);

    /**
     * Closes the outstanding bottom sheet.
     */
    void close();
}
