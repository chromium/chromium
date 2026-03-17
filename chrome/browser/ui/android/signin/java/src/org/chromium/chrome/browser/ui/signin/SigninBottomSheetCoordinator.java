// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerLaunchMode;
import org.chromium.chrome.browser.ui.signin.account_picker.PostSigninOperationResult;
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

    private final Delegate mDelegate;
    private final @FlowVariant String mSigninFlowVariant;

    private @Nullable SigninBottomSheetUiCoordinator mSigninUiCoordinator;

    /** This is a delegate that the embedder needs to implement. */
    public interface Delegate {
        /**
         * Called when the user triggers the "add account" action the sign-in bottom sheet. Triggers
         * the "add account" flow in the embedder.
         */
        void addAccount();

        /**
         * Notifies the delegate that the sign-in step has completed successfully, and allows it to
         * perform domain-specific post-sign-in logic.
         *
         * <p>This is called while the sign-in bottom sheet is still visible.
         *
         * @param signedInAccount The account that was just signed in.
         * @param onComplete Callback to be called when the post-sign-in delegate logic is finished.
         */
        void runPostSigninAction(
                CoreAccountInfo signedInAccount,
                Callback<@PostSigninOperationResult Integer> onComplete);

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
     * Creates an instance of {@link SigninBottomSheetCoordinator}
     *
     * @param delegate The delegate for this coordinator.
     * @param signinFlowVariant The flow variant for logging purposes.
     */
    public SigninBottomSheetCoordinator(Delegate delegate, @FlowVariant String signinFlowVariant) {
        mDelegate = delegate;
        mSigninFlowVariant = signinFlowVariant;
    }

    /**
     * Initializes the signin coordinator and possibly shows the bottomsheet.
     *
     * <p>Separate from constructor to avoid synchronous sign-in callbacks (e.g. seamless sign-in)
     * from running before the embedder has finished its own initialization and assigned this
     * coordinator instance to a member variable.
     *
     * @param windowAndroid The window that hosts the sign-in flow.
     * @param activity The {@link Activity} that hosts the sign-in flow.
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
    public void show(
            WindowAndroid windowAndroid,
            Activity activity,
            BottomSheetController bottomSheetController,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            SigninManager signinManager,
            AccountPickerBottomSheetStrings bottomSheetStrings,
            @AccountPickerLaunchMode int accountPickerLaunchMode,
            boolean isSeamlessSigninFlow,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId selectedAccountId) {
        if (isSeamlessSigninFlow) {
            assert selectedAccountId != null
                    : "Must provide a nonnullable {@link CoreAccountId} for seamless sign-in flow";
            SeamlessSigninCoordinator seamlessSigninCoordinator =
                    new SeamlessSigninCoordinator(
                            windowAndroid,
                            activity,
                            signinManager.getIdentityManager(),
                            signinManager,
                            bottomSheetController,
                            this,
                            bottomSheetStrings,
                            deviceLockActivityLauncher,
                            signinAccessPoint,
                            assertNonNull(selectedAccountId));
            mSigninUiCoordinator = seamlessSigninCoordinator;
            seamlessSigninCoordinator.launchSigninFlow();
        } else {
            mSigninUiCoordinator =
                    new AccountPickerBottomSheetCoordinator(
                            windowAndroid,
                            signinManager.getIdentityManager(),
                            signinManager,
                            bottomSheetController,
                            this,
                            bottomSheetStrings,
                            deviceLockActivityLauncher,
                            accountPickerLaunchMode,
                            signinAccessPoint == SigninAccessPoint.WEB_SIGNIN,
                            signinAccessPoint,
                            selectedAccountId);
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
    public void runPostSigninAction(
            CoreAccountInfo signedInAccount,
            Callback<@PostSigninOperationResult Integer> onComplete) {
        mDelegate.runPostSigninAction(signedInAccount, onComplete);
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onSignInComplete(
            CoreAccountInfo accountInfo, AccountPickerDelegate.SigninStateController controller) {
        controller.onSigninComplete();
        destroy();
        mDelegate.onSignInComplete();
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public void onSignInCancel() {
        mDelegate.onSignInCancel();
    }

    /** Implements {@link AccountPickerDelegate}. */
    @Override
    public @FlowVariant String getSigninFlowVariant() {
        return mSigninFlowVariant;
    }

    public void destroy() {
        if (mSigninUiCoordinator != null) {
            mSigninUiCoordinator.dismiss();
            mSigninUiCoordinator = null;
        }
    }
}
