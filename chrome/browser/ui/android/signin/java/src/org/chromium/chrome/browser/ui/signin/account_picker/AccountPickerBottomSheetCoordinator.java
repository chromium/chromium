// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the account picker bottom sheet. */
public class AccountPickerBottomSheetCoordinator {
    private final AccountPickerBottomSheetView mView;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerCoordinator mAccountPickerCoordinator;
    private final BottomSheetController mBottomSheetController;
    // TODO(crbug.com/328747528): The web sign-in specific logic should be moved out of the bottom
    // sheet MVC.
    private final boolean mIsWebSignin;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(
                        @SheetState int newState, @StateChangeReason int reason) {
                    super.onSheetStateChanged(newState, reason);
                    if (newState != BottomSheetController.SheetState.HIDDEN) {
                        return;
                    }

                    if (reason == StateChangeReason.SWIPE) {
                        logMetricAndIncrementActiveDismissalCountIfWebSignin(
                                AccountConsistencyPromoAction.DISMISSED_SWIPE_DOWN);
                    } else if (reason == StateChangeReason.BACK_PRESS) {
                        logMetricAndIncrementActiveDismissalCountIfWebSignin(
                                AccountConsistencyPromoAction.DISMISSED_BACK);
                    } else if (reason == StateChangeReason.TAP_SCRIM) {
                        logMetricAndIncrementActiveDismissalCountIfWebSignin(
                                AccountConsistencyPromoAction.DISMISSED_SCRIM);
                    }

                    AccountPickerBottomSheetCoordinator.this.destroy();
                }
            };

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the bottom sheet on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @AccountPickerLaunchMode int launchMode,
            boolean isWebSignin,
            @SigninAccessPoint int signinAccessPoint) {
        mIsWebSignin = isWebSignin;
        mSigninAccessPoint = signinAccessPoint;
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.SHOWN, mSigninAccessPoint);

        mAccountPickerBottomSheetMediator =
                new AccountPickerBottomSheetMediator(
                        windowAndroid,
                        accountPickerDelegate,
                        this::dismiss,
                        accountPickerBottomSheetStrings,
                        deviceLockActivityLauncher,
                        launchMode,
                        isWebSignin,
                        signinAccessPoint);
        mView =
                new AccountPickerBottomSheetView(
                        windowAndroid.getActivity().get(), mAccountPickerBottomSheetMediator);

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            mAccountPickerCoordinator =
                    new AccountPickerCoordinator(
                            mView.getAccountListView(),
                            mAccountPickerBottomSheetMediator,
                            R.layout.account_picker_bottom_sheet_row,
                            R.layout.account_picker_bottom_sheet_new_account_row);
        } else {
            mAccountPickerCoordinator =
                    new AccountPickerCoordinator(
                            mView.getAccountListView(),
                            mAccountPickerBottomSheetMediator,
                            R.layout.account_picker_row,
                            R.layout.account_picker_new_account_row);
        }

        mBottomSheetController = bottomSheetController;
        PropertyModelChangeProcessor.create(
                mAccountPickerBottomSheetMediator.getModel(),
                mView,
                AccountPickerBottomSheetViewBinder::bind);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(mView, true);
    }

    /** Releases the resources used by AccountPickerBottomSheetCoordinator. */
    @MainThread
    private void destroy() {
        mAccountPickerCoordinator.destroy();
        mAccountPickerBottomSheetMediator.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @MainThread
    public void dismiss() {
        logMetricAndIncrementActiveDismissalCountIfWebSignin(
                AccountConsistencyPromoAction.DISMISSED_BUTTON);
        mBottomSheetController.hideContent(mView, true);
    }

    @MainThread
    private void logMetricAndIncrementActiveDismissalCountIfWebSignin(
            @AccountConsistencyPromoAction int promoAction) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction, mSigninAccessPoint);
        if (mIsWebSignin) {
            SigninPreferencesManager.getInstance()
                    .incrementWebSigninAccountPickerActiveDismissalCount();
        }
    }

    /**
     * Called when an account is added on the device. Will sign the account in and may trigger the
     * bottom sheet and the flow dismissal in this case. Should be called only by the new sign-in
     * flow.
     */
    public void onAccountAdded(@NonNull String accountEmail) {
        assert SigninUtils.shouldShowNewSigninFlow();
        mAccountPickerBottomSheetMediator.onAccountAdded(accountEmail);
    }

    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
