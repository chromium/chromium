// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;

import androidx.annotation.ColorInt;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.SeamlessSigninCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.SigninBottomSheetUiCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;

/** Responsible of showing the sign-in bottom sheet. */
@NullMarked
public class SigninBottomSheetCoordinator implements AccountPickerDelegate {
    private static final int HISTORY_SYNC_ENTER_ANIMATION_DELAY_MS = 100;

    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;

    private final Delegate mDelegate;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final SigninManager mSigninManager;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final @Nullable CoreAccountId mSelectedCoreAccountId;

    private @Nullable SigninBottomSheetUiCoordinator mSigninUiCoordinator;

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

        /**
         * Called when the bottom sheet scrim color is changed, and the hosting activity's status
         * bar needs to be updated to the provided color.
         */
        void setStatusBarColor(@ColorInt int color);
    }

    /**
     * Creates an instance of {@link SigninBottomSheetCoordinator} and show the sign-in bottom
     * sheet.
     *
     * @param windowAndroid The window that hosts the sign-in flow.
     * @param activity The {@link Activity} that hosts the sign-in flow.
     * @param delegate The delegate for this coordinator.
     * @param bottomSheetController The controller of the sign-in bottomsheet.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param signinManager The sign-in manager to start the sign-in.
     * @param bottomSheetStrings The object containing the strings shown by the bottom sheet.
     * @param accountPickerLaunchMode Indicate the first bottom sheet view shown to the user.
     * @param isSeamlessSigninFlow If true, attempt a seamless sign-in flow, which automatically
     *     signs in the user.
     * @param signinAccessPoint The entry point for the sign-in.
     * @param selectedAccountId the account id to use as default, if present. Account id must be
     *     nonnull for seamless signin.
     */
    public SigninBottomSheetCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            Delegate delegate,
            BottomSheetController bottomSheetController,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            SigninManager signinManager,
            AccountPickerBottomSheetStrings bottomSheetStrings,
            @AccountPickerLaunchMode int accountPickerLaunchMode,
            boolean isSeamlessSigninFlow,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId selectedAccountId) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mSigninManager = signinManager;
        mSigninAccessPoint = signinAccessPoint;
        mSelectedCoreAccountId = selectedAccountId;

        if (isSeamlessSigninFlow) {
            assert mSelectedCoreAccountId != null
                    : "Must provide a nonnullable {@link CoreAccountId} for seamless sign-in flow";
            SeamlessSigninCoordinator seamlessSigninCoordinator =
                    new SeamlessSigninCoordinator(
                            mWindowAndroid,
                            mActivity,
                            mSigninManager.getIdentityManager(),
                            mSigninManager,
                            bottomSheetController,
                            this,
                            bottomSheetStrings,
                            mDeviceLockActivityLauncher,
                            mSigninAccessPoint,
                            assertNonNull(mSelectedCoreAccountId));
            mSigninUiCoordinator = seamlessSigninCoordinator;
            seamlessSigninCoordinator.launchSigninFlow();
        } else {
            mSigninUiCoordinator =
                    new AccountPickerBottomSheetCoordinator(
                            mWindowAndroid,
                            mSigninManager.getIdentityManager(),
                            mSigninManager,
                            bottomSheetController,
                            this,
                            bottomSheetStrings,
                            mDeviceLockActivityLauncher,
                            accountPickerLaunchMode,
                            mSigninAccessPoint == SigninAccessPoint.WEB_SIGNIN,
                            mSigninAccessPoint,
                            mSelectedCoreAccountId);
        }
    }

    /** Called when an account is added on the device. */
    public void onAccountAdded(String accountEmail) {
        assertNonNull(mSigninUiCoordinator).onAccountAdded(accountEmail);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public boolean canHandleAddAccount() {
        return true;
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
    public void onSignInComplete(
            CoreAccountInfo accountInfo, AccountPickerDelegate.SigninStateController controller) {
        controller.onSigninComplete();
        destroy();
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> mDelegate.onSignInComplete(),
                HISTORY_SYNC_ENTER_ANIMATION_DELAY_MS);
    }

    @Override
    public void onSignInCancel() {
        mDelegate.onSignInCancel();
    }

    public void destroy() {
        if (mSigninUiCoordinator != null) {
            mSigninUiCoordinator.dismiss();
            mSigninUiCoordinator = null;
        }
    }
}
