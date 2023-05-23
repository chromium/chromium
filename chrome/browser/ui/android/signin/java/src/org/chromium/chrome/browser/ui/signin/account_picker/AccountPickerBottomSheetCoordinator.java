// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinator of the account picker bottom sheet used in web signin flow.
 */
public class AccountPickerBottomSheetCoordinator {
    /** The scenarios which can trigger the account picker bottom sheet. */
    @IntDef({
            EntryPoint.WEB_SIGNIN,
            EntryPoint.SEND_TAB_TO_SELF,
            EntryPoint.FEED_ACTION,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EntryPoint {
        // The user navigated to a website requiring a signed-in Google Account.
        int WEB_SIGNIN = 0;
        // The user attempted to use the send-tab-to-self feature while being signed out.
        int SEND_TAB_TO_SELF = 1;
        // The user attempted to use the p13n actions on back of a feed card while signed out.
        int FEED_ACTION = 2;
    }

    private final AccountPickerBottomSheetView mView;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerCoordinator mAccountPickerCoordinator;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
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
     * Constructs the AccountPickerBottomSheetCoordinator and shows the
     * bottom sheet on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(AccountConsistencyPromoAction.SHOWN);

        mAccountPickerBottomSheetMediator = new AccountPickerBottomSheetMediator(windowAndroid,
                accountPickerDelegate, this::onDismissButtonClicked,
                accountPickerBottomSheetStrings, deviceLockActivityLauncher);
        mView = new AccountPickerBottomSheetView(
                windowAndroid.getActivity().get(), mAccountPickerBottomSheetMediator);
        mAccountPickerCoordinator = new AccountPickerCoordinator(
                mView.getAccountListView(), mAccountPickerBottomSheetMediator);

        mBottomSheetController = bottomSheetController;
        PropertyModelChangeProcessor.create(mAccountPickerBottomSheetMediator.getModel(), mView,
                AccountPickerBottomSheetViewBinder::bind);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetController.requestShowContent(mView, true);
    }

    /**
     * Releases the resources used by AccountPickerBottomSheetCoordinator.
     */
    @MainThread
    private void destroy() {
        mAccountPickerCoordinator.destroy();
        mAccountPickerBottomSheetMediator.destroy();

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @MainThread
    private void onDismissButtonClicked() {
        logMetricAndIncrementActiveDismissalCountIfWebSignin(
                AccountConsistencyPromoAction.DISMISSED_BUTTON);
        mBottomSheetController.hideContent(mView, true);
    }

    @MainThread
    private void logMetricAndIncrementActiveDismissalCountIfWebSignin(
            @AccountConsistencyPromoAction int promoAction) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction);
        if (mAccountPickerBottomSheetMediator.isEntryPointWebSignin()) {
            SigninPreferencesManager.getInstance()
                    .incrementWebSigninAccountPickerActiveDismissalCount();
        }
    }

    @VisibleForTesting
    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }

    public void setTryAgainBottomSheetView() {
        mAccountPickerBottomSheetMediator.setTryAgainBottomSheetView();
    }
}
