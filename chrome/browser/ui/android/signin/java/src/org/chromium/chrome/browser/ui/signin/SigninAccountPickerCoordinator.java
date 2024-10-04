// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.graphics.Color;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.activity.ComponentActivity;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.Toast;

/** Responsible of showing the sign-in bottom sheet. */
public class SigninAccountPickerCoordinator implements AccountPickerDelegate {
    private static final int HISTORY_SYNC_ENTER_ANIMATION_DELAY_MS = 100;

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
        /**
         * Called when the user triggers the "add account" action the sign-in bottom sheet. Triggers
         * the "add account" flow in the embedder.
         */
        void addAccount();

        /** Called when the sign-in successfully finishes. */
        void onSignInComplete();

        /** Called when the bottom sheet is dismissed without completing sign-in. */
        void onSignInCancel();

        /** Called when the bottom sheet scrim color is updated. */
        void setScrimColor(@ColorInt int scrimColor);
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
     * @param bottomSheetStrings The object containing the strings shown by the bottom sheet.
     * @param accountPickerLaunchMode Indicate the first bottom sheet view shown to the user.
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
            @NonNull AccountPickerBottomSheetStrings bottomSheetStrings,
            @AccountPickerLaunchMode int accountPickerLaunchMode) {
        ViewGroup sheetContainer = new FrameLayout(mActivity);
        sheetContainer.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mContainerView.addView(sheetContainer);
        @ColorInt int scrimColor = mActivity.getColor(R.color.default_scrim_color);
        mScrim =
                new ScrimCoordinator(
                        mActivity,
                        new ScrimCoordinator.SystemUiScrimDelegate() {
                            @Override
                            public void setStatusBarScrimFraction(float scrimFraction) {
                                // Update the status bar color to match the currently shown scrim
                                // color when the latter is changed.
                                float alpha = ((float) Color.alpha(scrimColor)) * scrimFraction;
                                @ColorInt
                                int color = ColorUtils.setAlphaComponent(scrimColor, (int) alpha);
                                mDelegate.setScrimColor(color);
                            }

                            @Override
                            public void setScrimColor(@ColorInt int scrimColor) {
                                mDelegate.setScrimColor(scrimColor);
                            }

                            @Override
                            public void setNavigationBarScrimFraction(float scrimFraction) {}
                        },
                        (ViewGroup) sheetContainer.getParent(),
                        scrimColor);

        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> mScrim,
                        (sheet) -> {},
                        mActivity.getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0,
                        /* desktopWindowStateProvider= */ null);

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
                SecondaryActivity.SIGNIN_AND_HISTORY_SYNC);

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

    /** Called when an account is added on the device. */
    public void onAccountAdded(@NonNull String accountEmail) {
        mAccountPickerBottomSheetCoordinator.onAccountAdded(accountEmail);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public boolean canHandleAddAccount() {
        return SigninUtils.shouldShowNewSigninFlow();
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void addAccount() {
        assert canHandleAddAccount();
        mDelegate.addAccount();
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onAccountPickerDestroy() {
        // The bottom sheet dismissal should already be requested when this method is called.
        // Therefore no further cleaning is needed.
    }

    /** Implements {@link AccountPickerDelegate}. */
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
                        PostTask.postDelayedTask(
                                TaskTraits.UI_DEFAULT,
                                () -> mDelegate.onSignInComplete(),
                                HISTORY_SYNC_ENTER_ANIMATION_DELAY_MS);
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

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void isAccountManaged(CoreAccountInfo accountInfo, Callback<Boolean> callback) {
        mSigninManager.isAccountManaged(accountInfo, callback);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void setUserAcceptedAccountManagement(boolean confirmed) {
        mSigninManager.setUserAcceptedAccountManagement(confirmed);
    }

    /** Implements {@link AccountPickerDelegate}. */
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
