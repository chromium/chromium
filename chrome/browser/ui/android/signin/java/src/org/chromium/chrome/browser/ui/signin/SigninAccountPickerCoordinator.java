// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/** Responsible of showing the sign-in bottom sheet. */
public class SigninAccountPickerCoordinator implements AccountPickerDelegate {
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final ViewGroup mContainerView;

    private final Delegate mDelegate;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final SigninManager mSigninManager;
    private final @SigninAccessPoint int mSigninAccessPoint;

    private ScrimCoordinator mScrim;
    private BottomSheetObserver mBottomSheetObserver;
    private BottomSheetController mBottomSheetController;
    private AccountPickerBottomSheetCoordinator mAccountPickerBottomSheetCoordinator;

    /** This is a delegate that the embedder needs to implement. */
    public interface Delegate {
        /** Called when the sign-in successfully finishes. */
        void onSignInComplete();

        /** Called when the sign-in is aborted. */
        void onSignInCancel();
    }

    /**
     * Creates an instance of {@link SigninAccountPickerCoordinator} and show the sign-in bottom
     * sheet.
     *
     * @param windowAndroid The window that hosts the sign-in flow.
     * @param activity The {@link Activity} that hosts the sign-in flow.
     * @param containerView The {@link ViewGroup} that should contain the bottom sheet.
     * @param delegate The delegate for this coordinator.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param signinManager The sign-in manager to start the sign-in.
     * @param signinAccessPoint The entry point for the sign-in.
     */
    public SigninAccountPickerCoordinator(
            @NonNull WindowAndroid windowAndroid,
            @NonNull Activity activity,
            @NonNull ViewGroup containerView,
            @NonNull Delegate delegate,
            @NonNull DeviceLockActivityLauncher deviceLockActivityLauncher,
            @NonNull SigninManager signinManager,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mContainerView = containerView;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mSigninManager = signinManager;
        mSigninAccessPoint = signinAccessPoint;

        initAndShowBottomSheet();
    }

    private void initAndShowBottomSheet() {
        ViewGroup sheetContainer = new FrameLayout(mActivity);
        sheetContainer.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mContainerView.addView(sheetContainer);
        mScrim =
                new ScrimCoordinator(
                        mActivity,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {}

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        (ViewGroup) sheetContainer.getParent(),
                        mActivity.getColor(android.R.color.transparent));

        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrim,
                        (sheet) -> {},
                        mActivity.getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0);

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        mBottomSheetController.removeObserver(this);
                        onBottomSheetDismiss(reason);
                    }

                    @Override
                    public void onSheetStateChanged(int newState, @StateChangeReason int reason) {
                        switch (newState) {
                            case SheetState.PEEK:
                            case SheetState.HALF:
                            case SheetState.FULL:
                                break;
                            case SheetState.HIDDEN:
                                mBottomSheetController.removeObserver(this);
                                onBottomSheetDismiss(reason);
                                break;
                        }
                    }
                };

        mBottomSheetController.addObserver(mBottomSheetObserver);
        mAccountPickerBottomSheetCoordinator =
                new AccountPickerBottomSheetCoordinator(
                        mWindowAndroid,
                        mBottomSheetController,
                        this,
                        new AccountPickerBottomSheetStrings() {},
                        mDeviceLockActivityLauncher);
    }

    /** Called when the account picker is destroyed after dismissal. */
    @Override
    public void onAccountPickerDestroy() {
        // The bottom sheet dismissal should already be requested when this method is called.
        // Therefore no further cleaning is needed.
    }

    /**
     * Starts sign-in with the given account, call the delegate's `onSigninComplete` on success or
     * show the error UI if the sign-in is aborted or not allowed.
     *
     * @param accountInfo The account to sign-in with.
     * @param onSignInErrorCallback The error callback that should be called by the WebSigninBridge,
     *     not used in this flow.
     */
    @Override
    public void signIn(
            CoreAccountInfo accountInfo, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        SigninManager.SignInCallback callback =
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        mDelegate.onSignInComplete();
                    }

                    @Override
                    public void onSignInAborted() {
                        // onSignInErrorCallback was meant to be called by the WebSigninBridge which
                        // is not used in this sign-in flow, as we do not need to wait for cookies
                        // to propagate.
                        // Instead of calling AccountPickerBottomSheetMediator.onSigninFailed()
                        // from the signin bridge we directly perform the creation of the "try
                        // again" bottom sheet view:
                        mAccountPickerBottomSheetCoordinator.setTryAgainBottomSheetView();
                    }
                };

        if (mSigninManager.isSigninAllowed()) {
            mSigninManager.signin(accountInfo, mSigninAccessPoint, callback);
        } else {
            makeSigninNotAllowedToast();
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), true);
        }
    }

    /** Returns the bottom sheet entry point. */
    @Override
    public @EntryPoint int getEntryPoint() {
        // TODO(https://crbug.com/1520783): Add and use entry points for the new sign-in flow.
        return EntryPoint.WEB_SIGNIN;
    }

    /**
     * Called by the embedder to dismiss the bottom sheet. This method is different from
     * `onAccountPickerDestroy` since the latter is called by the account picker coordinator, and
     * only after the bottom sheet's dismissal.
     */
    public void destroy() {
        if (mAccountPickerBottomSheetCoordinator != null) {
            mAccountPickerBottomSheetCoordinator.dismiss();
            mAccountPickerBottomSheetCoordinator = null;
        }
    }

    private void makeSigninNotAllowedToast() {
        // TODO(https://crbug.com/1520783): Update the string & UI.
        Toast.makeText(
                        mWindowAndroid.getActivity().get(),
                        R.string.sign_in_to_chrome_disabled_by_user_summary,
                        Toast.LENGTH_SHORT)
                .show();
    }

    private void onBottomSheetDismiss(@StateChangeReason int reason) {
        mAccountPickerBottomSheetCoordinator = null;
        // TODO(https://crbug.com/1520783): Handle different dismiss reasons.
    }
}
