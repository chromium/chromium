// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.activity.ComponentActivity;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetMediator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/** Responsible of showing the sign-in bottom sheet. */
public class SigninAccountPickerCoordinator implements AccountPickerDelegate {
    private final WindowAndroid mWindowAndroid;
    private final ComponentActivity mActivity;
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

        /** Called when the bottom sheet is dismissed without completing sign-in. */
        void onSignInCancel();
    }

    /**
     * Creates an instance of {@link SigninAccountPickerCoordinator} and show the sign-in bottom
     * sheet.
     *
     * @param windowAndroid The window that hosts the sign-in flow.
     * @param activity The {@link ComponentActivity} that hosts the sign-in flow.
     * @param containerView The {@link ViewGroup} that should contain the bottom sheet.
     * @param delegate The delegate for this coordinator.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param signinManager The sign-in manager to start the sign-in.
     * @param signinAccessPoint The entry point for the sign-in.
     */
    public SigninAccountPickerCoordinator(
            @NonNull WindowAndroid windowAndroid,
            @NonNull ComponentActivity activity,
            @NonNull ViewGroup containerView,
            @NonNull Delegate delegate,
            @NonNull DeviceLockActivityLauncher deviceLockActivityLauncher,
            @NonNull SigninManager signinManager,
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @AccountPickerLaunchMode int accountPickerLaunchMode,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mContainerView = containerView;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mSigninManager = signinManager;
        mSigninAccessPoint = signinAccessPoint;

        initAndShowBottomSheet(bottomSheetStrings, accountPickerLaunchMode);
    }

    private void initAndShowBottomSheet(
            AccountPickerBottomSheetStrings bottomSheetStrings,
            @AccountPickerLaunchMode int accountPickerLaunchMode) {
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
                };

        mBottomSheetController.addObserver(mBottomSheetObserver);
        BackPressHandler bottomSheetBackPressHandler =
                mBottomSheetController.getBottomSheetBackPressHandler();
        BackPressHelper.create(
                mActivity,
                mActivity.getOnBackPressedDispatcher(),
                bottomSheetBackPressHandler,
                SecondaryActivity.SIGNIN_AND_HISTORY_OPT_IN);

        mAccountPickerBottomSheetCoordinator =
                new AccountPickerBottomSheetCoordinator(
                        mWindowAndroid,
                        mBottomSheetController,
                        this,
                        bottomSheetStrings,
                        mDeviceLockActivityLauncher,
                        accountPickerLaunchMode,
                        mSigninAccessPoint == SigninAccessPoint.WEB_SIGNIN,
                        mSigninAccessPoint);
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
    public void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator) {
        SigninManager.SignInCallback callback =
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        mBottomSheetController.hideContent(
                                mBottomSheetController.getCurrentSheetContent(),
                                true,
                                StateChangeReason.INTERACTION_COMPLETE);
                        mDelegate.onSignInComplete();
                    }

                    @Override
                    public void onSignInAborted() {
                        mediator.switchToTryAgainView();
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

    @Override
    public void isAccountManaged(CoreAccountInfo accountInfo, Callback<Boolean> callback) {
        mSigninManager.isAccountManaged(accountInfo, callback);
    }

    @Override
    public void setUserAcceptedAccountManagement(boolean confirmed) {
        mSigninManager.setUserAcceptedAccountManagement(confirmed);
    }

    @Override
    public String extractDomainName(String accountEmail) {
        return mSigninManager.extractDomainName(accountEmail);
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
        // TODO(crbug.com/41493758): Update the string & UI.
        Toast.makeText(
                        mWindowAndroid.getActivity().get(),
                        R.string.sign_in_to_chrome_disabled_by_user_summary,
                        Toast.LENGTH_SHORT)
                .show();
    }

    private void onBottomSheetDismiss(@StateChangeReason int reason) {
        if (mAccountPickerBottomSheetCoordinator == null) {
            return;
        }

        mAccountPickerBottomSheetCoordinator = null;
        // The case of successful sign-in is already handled by the SignInCallBack.
        if (reason != StateChangeReason.INTERACTION_COMPLETE) {
            mDelegate.onSignInCancel();
        }
    }
}
