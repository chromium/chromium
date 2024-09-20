// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import org.chromium.blink.mojom.RpContext;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content.webid.IdentityRequestDialogLinkType;
import org.chromium.content_public.browser.WebContents;
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

        /** Called when the user clicks on the button to sign in to the IDP. */
        void onLoginToIdP(GURL idpConfigUrl, GURL idpLoginUrl);

        /** Called when the user clicks on the more details button in an error dialog. */
        void onMoreDetails();

        /** Called on the opener when a modal dialog that it opened has been closed. */
        void onModalDialogClosed();

        /** Called when the accounts UI is displayed. */
        void onAccountsDisplayed();

        /**
         * Gets the WebContents used by this delegate.
         *
         * <p>For use by code running in the CCT popup.
         */
        WebContents getWebContents();

        /** Called to associate the popup with the delegate. */
        void setPopupComponent(AccountSelectionComponent popupComponent);
    }

    /**
     * Displays the given accounts in a new bottom sheet.
     *
     * @param rpEtldPlusOne The {@link String} for the relying party.
     * @param idpEtldPlusOne The {@link String} for the identity provider.
     * @param accounts A list of {@link Account}s that will be displayed.
     * @param idpData The information about the identity provider.
     * @param isAutoReauthn A {@link boolean} that represents whether this is an auto re-authn flow.
     * @param newAccounts The newly logged in accounts.
     */
    void showAccounts(
            String rpEtldPlusOne,
            String idpEtldPlusOne,
            List<Account> accounts,
            IdentityProviderData idpData,
            boolean isAutoReauthn,
            List<Account> newAccounts);

    /**
     * Displays a dialog telling the user that they can sign in to an IDP for the purpose of
     * federated login when the IDP sign-in status is signin but no accounts are received from the
     * fetch.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param idpMetadata is the metadata of the IDP.
     * @param rpContext is an enum representing the desired text to be used in the title of the
     *     FedCM prompt: "signin", "continue", etc.
     */
    void showFailureDialog(
            String rpForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext);

    /**
     * Displays a dialog telling the user that an error has occurred in their attempt to sign-in to
     * a website with an IDP.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param idpMetadata is the metadata of the IDP.
     * @param rpContext is an enum representing the desired text to be used in the title of the
     *     FedCM prompt: "signin", "continue", etc.
     * @param IdentityCredentialTokenError is contains the error code and url to display in the
     *     FedCM prompt.
     */
    void showErrorDialog(
            String rpForDisplay,
            String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext,
            IdentityCredentialTokenError error);

    /**
     * Displays a dialog with a spinner to indicate that contents are loading.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param rpContext is an enum representing the desired text to be used in the title of the
     *     FedCM prompt: "signin", "continue", etc.
     */
    void showLoadingDialog(
            String rpForDisplay, String idpForDisplay, @RpContext.EnumType int rpContext);

    /**
     * Closes the outstanding bottom sheet or the popup, depending on what this object corresponds
     * to.
     */
    void close();

    /** Gets the sheet's title. */
    String getTitle();

    /** Gets the sheet's subtitle, if any, or null.. */
    String getSubtitle();

    /** Show the given URL in a popup window. */
    void showUrl(@IdentityRequestDialogLinkType int linkType, GURL url);

    /**
     * Shows a custom tab with the given url. Returns the WebContents of the new dialog.
     *
     * @param url The URL to be loaded in the dialog.
     */
    WebContents showModalDialog(GURL url);

    /** Closes a modal dialog, if one is opened. */
    void closeModalDialog();

    /** Gets notified about the modal dialog that it opened being closed. */
    void onModalDialogClosed();

    /** Gets the WebContents for this object. */
    WebContents getWebContents();

    /**
     * Gets the WebContents for the RP associated with this object.
     *
     * <p>This is intended to be called on the custom tab object to return the WebContents for the
     * RP, i.e. the WebContents for the object that showModalDialog was called on.
     */
    WebContents getRpWebContents();

    /** Called to associate the popup with the delegate. */
    void setPopupComponent(AccountSelectionComponent popupComponent);
}
