// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Responsible of showing the correct sub-component of the sign-in and history opt-in flow. */
public class SigninAndHistoryOptInCoordinator implements SigninAccountPickerCoordinator.Delegate {
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final ViewGroup mContainerView;

    private final Delegate mDelegate;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final OneshotSupplier<Profile> mProfileSupplier;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    // TODO(https://crbug.com/1520783): Start different flow according to the modes set.
    private final @NoAccountSigninMode int mNoAccountSigninMode;
    private final @HistoryOptInMode int mHistoryOptInMode;

    private SigninAccountPickerCoordinator mAccountPickerCoordinator;

    /** This is a delegate that the embedder needs to implement. */
    public interface Delegate {
        /** Called when the whole flow finishes. */
        void onFlowComplete();
    }

    /** The sign-in step that should be shown to the user when there's no account on the device. */
    @IntDef({
        NoAccountSigninMode.BOTTOM_SHEET,
        NoAccountSigninMode.ADD_ACCOUNT,
        NoAccountSigninMode.NO_SIGNIN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NoAccountSigninMode {
        /** Show the 0-account sign-in bottom sheet. */
        int BOTTOM_SHEET = 0;

        /** Bring the user to GMS Core to add an account, then sign-in with the new account. */
        int ADD_ACCOUNT = 1;

        /** No sign-in should be done, the entry point should not be visible to the user. */
        int NO_SIGNIN = 2;
    }

    /** The visibility rule to apply to the history opt-in step. */
    @IntDef({HistoryOptInMode.NONE, HistoryOptInMode.OPTIONAL, HistoryOptInMode.REQUIRED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HistoryOptInMode {
        /** Never show the history sync opt-in. */
        int NONE = 0;

        /** The history sync opt-in can be skipped (e.g. if the user declined too recently). */
        int OPTIONAL = 1;

        /** The history sync opt-in should always be shown. */
        int REQUIRED = 2;
    }

    /**
     * Creates an instance of {@link SigninAndHistoryOptInCoordinator} and shows the sign-in bottom
     * sheet.
     *
     * @param windowAndroid The window that hosts the sign-in & history opt-in flow.
     * @param activity The {@link Activity} that hosts the sign-in &opt-in flow.
     * @param delegate The delegate for this coordinator.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param profileSupplier The supplier of the current profile.
     * @param modalDialogManagerSupplier The supplier of the {@link ModalDialogManager}
     * @param signinAccessPoint The entry point for the sign-in.
     */
    public SigninAndHistoryOptInCoordinator(
            @NonNull WindowAndroid windowAndroid,
            @NonNull Activity activity,
            @NonNull Delegate delegate,
            @NonNull DeviceLockActivityLauncher deviceLockActivityLauncher,
            @NonNull OneshotSupplier<Profile> profileSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NoAccountSigninMode int noAccountSigninMode,
            @HistoryOptInMode int historyOptInMode,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.onAvailable(this::onProfileAvailable);
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mNoAccountSigninMode = noAccountSigninMode;
        mHistoryOptInMode = historyOptInMode;
        mSigninAccessPoint = signinAccessPoint;
        mContainerView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.signin_history_opt_in_container, null);

        // TODO(https://crbug.com/1520783): Implement loading state UI when async
        // initialization will be supported.
    }

    /** Called when the sign-in successfully finishes. */
    @Override
    public void onSignInComplete() {
        showHistoryOptInDialog();
    }

    /** Called when the sign-in is aborted. */
    @Override
    public void onSignInCancel() {
        onFlowComplete();
    }

    /** Provides the root view of the sign-in and history opt-in flow. */
    public @NonNull ViewGroup getView() {
        assert mContainerView != null;
        return mContainerView;
    }

    /**
     * Call the child coordinators' `destroy` method to release resources, should be called when the
     * hosting activity is destroyed.
     */
    public void destroy() {
        mAccountPickerCoordinator.destroy();
    }

    private void onProfileAvailable(Profile profile) {
        if (profile.isOffTheRecord()) {
            throw new IllegalStateException(
                    "Sign-in & history opt-in flow is using incognito profile");
        }

        SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(mProfileSupplier.get());
        mAccountPickerCoordinator =
                new SigninAccountPickerCoordinator(
                        mWindowAndroid,
                        mActivity,
                        mContainerView,
                        this,
                        mDeviceLockActivityLauncher,
                        signinManager,
                        mSigninAccessPoint);
    }

    private void showHistoryOptInDialog() {
        ModalDialogManager manager = mModalDialogManagerSupplier.get();
        assert manager != null;

        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CUSTOM_VIEW, getDialogContentView())
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(
                                ModalDialogProperties.DIALOG_STYLES,
                                ModalDialogProperties.DialogStyles.DIALOG_WHEN_LARGE)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new ModalDialogProperties.Controller() {
                                    @Override
                                    public void onClick(
                                            PropertyModel model, @ButtonType int buttonType) {
                                        // TODO(https://crbug.com/1520783): To implement.
                                    }

                                    @Override
                                    public void onDismiss(
                                            PropertyModel model,
                                            @DialogDismissalCause int dismissalCause) {
                                        // TODO(https://crbug.com/1520783): Better handle dismissal.
                                        onFlowComplete();
                                    }
                                })
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                new OnBackPressedCallback(true) {
                                    @Override
                                    public void handleOnBackPressed() {
                                        // TODO(https://crbug.com/1520783): Better handle dismissal.
                                        onFlowComplete();
                                    }
                                })
                        .build();

        manager.showDialog(
                dialogModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    private @NonNull View getDialogContentView() {
        // TODO(https://crbug.com/1520783): Remove the lines below and use the new history-sync
        // opt-in view in the dialog.
        View dialogContentView = new View(mActivity);
        dialogContentView.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        dialogContentView.setBackgroundColor(Color.CYAN);
        dialogContentView.setMinimumHeight(200);
        return dialogContentView;
    }

    private void onFlowComplete() {
        mDelegate.onFlowComplete();
    }
}
