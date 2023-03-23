// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
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

    private AccountSelectionBridge(long nativeView, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController) {
        mNativeView = nativeView;
        mAccountSelectionComponent = new AccountSelectionCoordinator(
                windowAndroid.getContext().get(), bottomSheetController, this);
    }

    @CalledByNative
    static int getBrandIconMinimumSize() {
        // Icon needs to be big enough for the smallest screen density (1x).
        Resources resources = ContextUtils.getApplicationContext().getResources();
        // Density < 1.0f on ldpi devices. Adjust density to ensure that
        // {@link getBrandIconMinimumSize()} <= {@link getBrandIconIdealSize()}.
        float density = Math.max(resources.getDisplayMetrics().density, 1.0f);
        return Math.round(getBrandIconIdealSize() / density);
    }

    @CalledByNative
    static int getBrandIconIdealSize() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return Math.round(resources.getDimension(R.dimen.account_selection_sheet_icon_size)
                / MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO);
    }

    @CalledByNative
    private static @Nullable AccountSelectionBridge create(
            long nativeView, WindowAndroid windowAndroid) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) return null;
        return new AccountSelectionBridge(nativeView, windowAndroid, bottomSheetController);
    }

    @CalledByNative
    private void destroy() {
        mAccountSelectionComponent.close();
        mNativeView = 0;
    }

    /* Shows the accounts in a bottom sheet UI allowing user to select one.
     *
     * @param topFrameForDisplay is the formatted RP top frame URL to display in the FedCM prompt.
     * @param iframeForDisplay is the formatted RP iframe URL to display in the FedCM prompt.
     * @param idpForDisplay is the formatted IDP URL to display in the FedCM prompt.
     * @param accounts is the list of accounts to be shown.
     * @param isAutoReauthn represents whether this is an auto re-authn flow.
     */
    @CalledByNative
    private void showAccounts(String topFrameForDisplay, String iframeForDisplay,
            String idpForDisplay, Account[] accounts, IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientIdMetadata, boolean isAutoReauthn) {
        assert accounts != null && accounts.length > 0;
        mAccountSelectionComponent.showAccounts(topFrameForDisplay, iframeForDisplay, idpForDisplay,
                Arrays.asList(accounts), idpMetadata, clientIdMetadata, isAutoReauthn);
    }

    @CalledByNative
    private String getTitle() {
        return mAccountSelectionComponent.getTitle();
    }

    @CalledByNative
    private String getSubtitle() {
        return mAccountSelectionComponent.getSubtitle();
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
            AccountSelectionBridgeJni.get().onAccountSelected(mNativeView, idpConfigUrl,
                    account.getStringFields(), account.getPictureUrl(), account.isSignIn());
        }
    }

    @NativeMethods
    interface Natives {
        void onAccountSelected(long nativeAccountSelectionViewAndroid, GURL idpConfigUrl,
                String[] accountFields, GURL accountPictureUrl, boolean isSignedIn);
        void onDismiss(long nativeAccountSelectionViewAndroid,
                @IdentityRequestDialogDismissReason int dismissReason);
    }
}
