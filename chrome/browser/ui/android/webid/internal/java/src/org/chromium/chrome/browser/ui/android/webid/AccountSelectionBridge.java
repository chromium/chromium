// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content.webid.IdentityRequestDialogLinkType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * This bridge creates and initializes a {@link AccountSelectionComponent} on construction and
 * forwards native calls to it.
 */
class AccountSelectionBridge implements AccountSelectionComponent.Delegate {
    /**
     * The size of the maskable icon's safe zone as a fraction of the icon's edge size as defined
     * in https://www.w3.org/TR/appmanifest/
     */
    public static final float MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO = 0.8f;

    private long mNativeView;
    private final AccountSelectionComponent mAccountSelectionComponent;

    private AccountSelectionBridge(
            long nativeView,
            Tab tab,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            @RpMode.EnumType int rpMode) {
        mNativeView = nativeView;
        mAccountSelectionComponent =
                new AccountSelectionCoordinator(
                        tab, windowAndroid, bottomSheetController, rpMode, this);
    }

    @CalledByNative
    static int getBrandIconMinimumSize(@RpMode.EnumType int rpMode) {
        // Icon needs to be big enough for the smallest screen density (1x).
        Resources resources = ContextUtils.getApplicationContext().getResources();
        // Density < 1.0f on ldpi devices. Adjust density to ensure that
        // {@link getBrandIconMinimumSize()} <= {@link getBrandIconIdealSize()}.
        float density = Math.max(resources.getDisplayMetrics().density, 1.0f);
        return Math.round(getBrandIconIdealSize(rpMode) / density);
    }

    @CalledByNative
    static int getBrandIconIdealSize(@RpMode.EnumType int rpMode) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return Math.round(
                resources.getDimension(
                                rpMode == RpMode.ACTIVE
                                        ? R.dimen.account_selection_button_mode_sheet_icon_size
                                        : R.dimen.account_selection_sheet_icon_size)
                        / MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO);
    }

    @CalledByNative
    private static @Nullable AccountSelectionBridge create(
            long nativeView,
            WebContents webContents,
            WindowAndroid windowAndroid,
            @RpMode.EnumType int rpMode) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        Tab tab = TabUtils.fromWebContents(webContents);
        return new AccountSelectionBridge(
                nativeView, tab, windowAndroid, bottomSheetController, rpMode);
    }

    @CalledByNative
    private void destroy() {
        mAccountSelectionComponent.close();
        mNativeView = 0;
    }

    /**
     * Shows the accounts in a bottom sheet UI allowing user to select one.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param accounts is the list of accounts to be shown.
     * @param idpData is the data of the IDP.
     * @param isAutoReauthn represents whether this is an auto re-authn flow.
     * @param newAccounts represents the newly logged in accounts.
     */
    @CalledByNative
    private void showAccounts(
            @JniType("std::string") String rpForDisplay,
            @JniType("std::string") String idpForDisplay,
            Account[] accounts,
            IdentityProviderData idpData,
            boolean isAutoReauthn,
            Account[] newAccounts) {
        assert accounts != null && accounts.length > 0;
        mAccountSelectionComponent.showAccounts(
                rpForDisplay,
                idpForDisplay,
                Arrays.asList(accounts),
                idpData,
                isAutoReauthn,
                Arrays.asList(newAccounts));
    }

    /**
     * Shows a bottomsheet prompting the user to sign in to an IDP for the purpose of federated
     * login when the IDP sign-in status is signin but no accounts are received from the fetch.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param idpMetadata is the metadata of the IDP.
     * @param rpContext is an enum representing the desired text to be used in the title of the
     *     FedCM prompt: "signin", "continue", etc.
     */
    @CalledByNative
    private void showFailureDialog(
            @JniType("std::string") String rpForDisplay,
            @JniType("std::string") String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext) {
        mAccountSelectionComponent.showFailureDialog(
                rpForDisplay, idpForDisplay, idpMetadata, rpContext);
    }

    /**
     * Shows a bottomsheet detailing the error that has occurred in the user's attempt to sign-in
     * through federated login.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param idpMetadata is the metadata of the IDP.
     * @param rpContext is a {@link String} representing the desired text to be used in the title of
     *     the FedCM prompt: "signin", "continue", etc.
     * @param IdentityCredentialTokenError is contains the error code and url to display in the
     *     FedCM prompt.
     */
    @CalledByNative
    private void showErrorDialog(
            @JniType("std::string") String rpForDisplay,
            @JniType("std::string") String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            @RpContext.EnumType int rpContext,
            IdentityCredentialTokenError error) {
        mAccountSelectionComponent.showErrorDialog(
                rpForDisplay, idpForDisplay, idpMetadata, rpContext, error);
    }

    /**
     * Shows a bottomsheet prompting the user to sign-in to an RP with an IDP with a spinner to
     * indicate that contents are loading.
     *
     * @param rpForDisplay is the formatted RP URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param rpContext is a {@link String} representing the desired text to be used in the title of
     *     the FedCM prompt: "signin", "continue", etc.
     */
    @CalledByNative
    private void showLoadingDialog(
            @JniType("std::string") String rpForDisplay,
            @JniType("std::string") String idpForDisplay,
            @RpContext.EnumType int rpContext) {
        mAccountSelectionComponent.showLoadingDialog(rpForDisplay, idpForDisplay, rpContext);
    }

    @CalledByNative
    private @JniType("std::string") String getTitle() {
        return mAccountSelectionComponent.getTitle();
    }

    @CalledByNative
    private @JniType("std::optional<std::string>") String getSubtitle() {
        return mAccountSelectionComponent.getSubtitle();
    }

    @CalledByNative
    private void showUrl(@IdentityRequestDialogLinkType int linkType, @JniType("GURL") GURL url) {
        mAccountSelectionComponent.showUrl(linkType, url);
    }

    @CalledByNative
    private WebContents showModalDialog(@JniType("GURL") GURL url) {
        return mAccountSelectionComponent.showModalDialog(url);
    }

    @CalledByNative
    private void closeModalDialog() {
        mAccountSelectionComponent.closeModalDialog();
    }

    @CalledByNative
    private WebContents getRpWebContents() {
        return mAccountSelectionComponent.getRpWebContents();
    }

    @Override
    public void onDismissed(@IdentityRequestDialogDismissReason int dismissReason) {
        if (mNativeView != 0) {
            AccountSelectionBridgeJni.get().onDismiss(mNativeView, dismissReason);
        }
    }

    @Override
    public void onAccountSelected(GURL idpConfigUrl, Account account) {
        if (mNativeView != 0) {
            // This call passes the account fields directly as String and GURL parameters as an
            // optimization to avoid needing multiple JNI getters on the Account class on for each
            // field.
            AccountSelectionBridgeJni.get()
                    .onAccountSelected(
                            mNativeView,
                            idpConfigUrl,
                            account.getStringFields(),
                            account.getPictureUrl(),
                            account.isSignIn());
        }
    }

    @Override
    public void onLoginToIdP(GURL idpConfigUrl, GURL idpLoginUrl) {
        if (mNativeView != 0) {
            AccountSelectionBridgeJni.get().onLoginToIdP(mNativeView, idpConfigUrl, idpLoginUrl);
        }
    }

    @Override
    public void onMoreDetails() {
        if (mNativeView != 0) {
            AccountSelectionBridgeJni.get().onMoreDetails(mNativeView);
        }
    }

    @Override
    public void onAccountsDisplayed() {
        if (mNativeView != 0) {
            AccountSelectionBridgeJni.get().onAccountsDisplayed(mNativeView);
        }
    }

    @Override
    public void onModalDialogClosed() {
        mAccountSelectionComponent.onModalDialogClosed();
    }

    @Override
    public WebContents getWebContents() {
        return mAccountSelectionComponent.getWebContents();
    }

    @Override
    public void setPopupComponent(AccountSelectionComponent popupComponent) {
        mAccountSelectionComponent.setPopupComponent(popupComponent);
    }

    @NativeMethods
    interface Natives {
        void onAccountSelected(
                long nativeAccountSelectionViewAndroid,
                @JniType("GURL") GURL idpConfigUrl,
                @JniType("std::vector<std::string>") String[] accountFields,
                @JniType("GURL") GURL accountPictureUrl,
                boolean isSignedIn);

        void onDismiss(
                long nativeAccountSelectionViewAndroid,
                @IdentityRequestDialogDismissReason int dismissReason);

        void onLoginToIdP(
                long nativeAccountSelectionViewAndroid,
                @JniType("GURL") GURL idpConfigUrl,
                @JniType("GURL") GURL idpLoginUrl);

        void onMoreDetails(long nativeAccountSelectionViewAndroid);

        void onAccountsDisplayed(long nativeAccountSelectionViewAndroid);
    }
}
