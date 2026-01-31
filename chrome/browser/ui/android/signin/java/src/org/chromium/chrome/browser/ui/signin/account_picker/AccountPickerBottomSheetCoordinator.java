// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the account picker bottom sheet. */
@NullMarked
public class AccountPickerBottomSheetCoordinator implements SigninBottomSheetUiCoordinator {
    private final AccountPickerBottomSheetView mView;
    private final BottomSheetController mBottomSheetController;
    private final AccountPickerDelegate mAccountPickerDelegate;
    private final AccountPickerDismissalLogger mDismissalLogger;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerCoordinator mAccountPickerCoordinator;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(
                        @SheetState int newState, @StateChangeReason int reason) {
                    super.onSheetStateChanged(newState, reason);
                    if (newState != BottomSheetController.SheetState.HIDDEN) {
                        return;
                    }

                    mDismissalLogger.logBottomSheetDismissal(reason);
                    if (reason != StateChangeReason.INTERACTION_COMPLETE) {
                        mAccountPickerDelegate.onSignInCancel();
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
            IdentityManager identityManager,
            SigninManager signinManager,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @AccountPickerLaunchMode int launchMode,
            boolean isWebSignin,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId selectedAccountId) {
        mBottomSheetController = bottomSheetController;
        mAccountPickerDelegate = accountPickerDelegate;
        mDismissalLogger = new AccountPickerDismissalLogger(signinAccessPoint, isWebSignin);
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.SHOWN, signinAccessPoint);

        mAccountPickerBottomSheetMediator =
                AccountPickerBottomSheetMediator.create(
                        windowAndroid,
                        identityManager,
                        signinManager,
                        accountPickerDelegate,
                        this::dismiss,
                        accountPickerBottomSheetStrings,
                        deviceLockActivityLauncher,
                        launchMode,
                        isWebSignin,
                        signinAccessPoint,
                        selectedAccountId);
        mView =
                new AccountPickerBottomSheetView(
                        assumeNonNull(windowAndroid.getActivity().get()),
                        mAccountPickerBottomSheetMediator);

        mAccountPickerCoordinator =
                new AccountPickerCoordinator(
                        mView.getAccountListView(),
                        mAccountPickerBottomSheetMediator,
                        identityManager,
                        R.layout.account_picker_bottom_sheet_row,
                        R.layout.account_picker_bottom_sheet_new_account_row);

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

    /** Implements {@link SigninBottomSheetUiCoordinator}. */
    @Override
    @MainThread
    public void dismiss() {
        // The observer calls destroy() after the sheet is hidden.
        mBottomSheetController.hideContent(mView, true, StateChangeReason.INTERACTION_COMPLETE);
    }

    /**
     * Implements {@link SigninBottomSheetUiCoordinator} Called when an account is added on the
     * device. Will sign the account in and may trigger the bottom sheet and the flow dismissal in
     * this case. Should be called only by the new sign-in flow.
     */
    @Override
    public void onAccountAdded(String accountEmail) {
        mAccountPickerBottomSheetMediator.onAccountAdded(accountEmail);
    }

    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
