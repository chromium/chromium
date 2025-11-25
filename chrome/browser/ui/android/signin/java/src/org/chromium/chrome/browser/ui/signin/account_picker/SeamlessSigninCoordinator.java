// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the seamless sign-in flow. This class handles the UI logic where the user signs in
 * directly from a promo. The bottom sheet is not shown by default for seamless sign-in; it will
 * only be displayed in cases of sign-in errors, or for management notices.
 */
@NullMarked
public class SeamlessSigninCoordinator {

    private final Activity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;

    private @Nullable AccountPickerBottomSheetView mView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    // TODO(crbug.com/437038737): log AccountConsistencyPromoAction dismissed type,
                    // refactor and reuse AccountPickerBottomSheetCoordinator.mBottomSheetObserver
                    SeamlessSigninCoordinator.this.destroy();
                }
            };

    /**
     * Constructs the SeamlessSigninCoordinator.
     *
     * @param windowAndroid The current activity window.
     * @param activity The {@link Activity} that hosts the sign-in flow.
     * @param identityManager The IdentityManager for the current profile.
     * @param signinManager The sign-in manager to start the sign-in.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param accountPickerDelegate The delegate for account picker actions.
     * @param accountPickerBottomSheetStrings The strings for the account picker bottom sheet.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param signinAccessPoint The entry point for the sign-in flow.
     * @param selectedAccountId The account to be signed in seamlessly.
     */
    @MainThread
    public SeamlessSigninCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            IdentityManager identityManager,
            SigninManager signinManager,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @SigninAccessPoint int signinAccessPoint,
            CoreAccountId selectedAccountId) {
        mActivity = activity;
        mBottomSheetController = bottomSheetController;

        mAccountPickerBottomSheetMediator =
                AccountPickerBottomSheetMediator.createForSeamlessSignin(
                        windowAndroid,
                        identityManager,
                        signinManager,
                        accountPickerDelegate,
                        this::requestDisplayBottomSheet,
                        this::dismissBottomSheet,
                        accountPickerBottomSheetStrings,
                        deviceLockActivityLauncher,
                        signinAccessPoint,
                        selectedAccountId);

        mAccountPickerBottomSheetMediator.launchDeviceLockIfNeededAndSignIn();
    }

    @MainThread
    public void destroy() {
        mAccountPickerBottomSheetMediator.destroy();
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    /**
     * Displays the bottom sheet to present a seamless sign-in error or managed account
     * confirmation.
     */
    @MainThread
    public void requestDisplayBottomSheet() {
        if (mView == null) {
            // Bottom sheet initialized lazily, in most cases no bottom sheet will be shown
            mView = new AccountPickerBottomSheetView(mActivity, mAccountPickerBottomSheetMediator);
            PropertyModelChangeProcessor.create(
                    mAccountPickerBottomSheetMediator.getModel(),
                    mView,
                    AccountPickerBottomSheetViewBinder::bind);

            mBottomSheetController.addObserver(mBottomSheetObserver);
            mBottomSheetController.requestShowContent(mView, true);

            // TODO(crbug.com/437038737): log AccountConsistencyPromoAction.SHOWN histogram
        }
    }

    /** Dismiss the bottom sheet, if shown. */
    @MainThread
    public void dismissBottomSheet() {
        if (mView != null) {
            // TODO(crbug.com/437038737): log AccountConsistencyPromoAction.DISMISSED_BUTTON
            // histogram
            mBottomSheetController.hideContent(mView, true);
        }
    }

    @Nullable View getBottomSheetViewForTesting() {
        return mView == null ? null : mView.getContentView();
    }
}
