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
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
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
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

public class SigninAndHistoryOptInCoordinator implements AccountPickerDelegate {
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final ViewGroup mContainerView;

    private final Delegate mDelegate;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final SigninManager mSigninManager;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    private ScrimCoordinator mScrim;
    private BottomSheetObserver mBottomSheetObserver;
    private BottomSheetController mBottomSheetController;
    private AccountPickerBottomSheetCoordinator mAccountPickerBottomSheetCoordinator;

    public interface Delegate {
        void onFlowComplete();
    }

    public SigninAndHistoryOptInCoordinator(
            @NonNull WindowAndroid windowAndroid,
            @NonNull Activity activity,
            @NonNull Delegate delegate,
            @NonNull DeviceLockActivityLauncher deviceLockActivityLauncher,
            @NonNull Profile profile,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @SigninAccessPoint int signinAccessPoint) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mDelegate = delegate;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
        mSigninManager = IdentityServicesProvider.get().getSigninManager(profile);
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mSigninAccessPoint = signinAccessPoint;
        mContainerView =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.signin_history_opt_in_container, null);

        initAndShowBottomSheet();
    }

    @Override
    public void destroy() {}

    @Override
    public void signIn(
            CoreAccountInfo accountInfo, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
        SigninManager.SignInCallback callback =
                new SigninManager.SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        showHistoryOptInDialog();
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

    @Override
    public @EntryPoint int getEntryPoint() {
        // TODO(https://crbug.com/1520783): Add and use entry points for the new sign-in flow.
        return EntryPoint.WEB_SIGNIN;
    }

    public @NonNull ViewGroup getView() {
        assert mContainerView != null;
        return mContainerView;
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
                        onBottomSheetDismiss(reason);
                        mBottomSheetController.removeObserver(this);
                    }

                    @Override
                    public void onSheetStateChanged(int newState, @StateChangeReason int reason) {
                        switch (newState) {
                            case SheetState.PEEK:
                            case SheetState.HALF:
                            case SheetState.FULL:
                                break;
                            case SheetState.HIDDEN:
                                onBottomSheetDismiss(reason);
                                mBottomSheetController.removeObserver(this);
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

    private void makeSigninNotAllowedToast() {
        // TODO(https://crbug.com/1520783): Update the string & UI.
        Toast.makeText(
                        mWindowAndroid.getActivity().get(),
                        R.string.sign_in_to_chrome_disabled_by_user_summary,
                        Toast.LENGTH_SHORT)
                .show();
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

    private void onBottomSheetDismiss(@StateChangeReason int reason) {
        // TODO(https://crbug.com/1520783): Handle different dismiss reasons.
    }

    private void onFlowComplete() {
        mDelegate.onFlowComplete();
    }
}
