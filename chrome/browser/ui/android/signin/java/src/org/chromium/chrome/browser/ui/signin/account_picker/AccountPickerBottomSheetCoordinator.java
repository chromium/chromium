// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the account picker bottom sheet or modal dialog. */
@NullMarked
public class AccountPickerBottomSheetCoordinator implements SigninBottomSheetUiCoordinator {
    private final AccountPickerBottomSheetView mView;
    private final AccountPickerPresenter mPresenter;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerCoordinator mAccountPickerCoordinator;

    /**
     * Constructs the AccountPickerBottomSheetCoordinator and shows the bottom sheet or modal dialog
     * on the screen.
     */
    @MainThread
    public AccountPickerBottomSheetCoordinator(
            WindowAndroid windowAndroid,
            IdentityManager identityManager,
            SigninManager signinManager,
            @Nullable AccountPreviewDataService accountPreviewDataService,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @AccountPickerLaunchMode int launchMode,
            boolean isWebSignin,
            @SigninAccessPoint int signinAccessPoint,
            @Nullable CoreAccountId selectedAccountId) {
        Activity activity = assumeNonNull(windowAndroid.getActivity().get());
        var dismissalLogger = new AccountPickerDismissalLogger(signinAccessPoint, isWebSignin);
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.SHOWN, signinAccessPoint);

        mAccountPickerBottomSheetMediator =
                AccountPickerBottomSheetMediator.create(
                        windowAndroid,
                        identityManager,
                        signinManager,
                        accountPreviewDataService,
                        accountPickerDelegate,
                        this::dismiss,
                        accountPickerBottomSheetStrings,
                        deviceLockActivityLauncher,
                        launchMode,
                        isWebSignin,
                        signinAccessPoint,
                        selectedAccountId);

        boolean showAsDialog = SigninUtils.shouldShowAccountPickerDialog(activity);

        mPresenter =
                showAsDialog
                        ? new ModalDialogAccountPickerPresenter(
                                modalDialogManager,
                                dismissalLogger,
                                accountPickerDelegate,
                                this::destroy)
                        : new BottomSheetAccountPickerPresenter(
                                bottomSheetController,
                                dismissalLogger,
                                accountPickerDelegate,
                                this::destroy);

        mView =
                new AccountPickerBottomSheetView(
                        activity,
                        mAccountPickerBottomSheetMediator,
                        showAsDialog
                                ? AccountPickerBottomSheetView.PresentationMode.MODAL_DIALOG
                                : AccountPickerBottomSheetView.PresentationMode.BOTTOM_SHEET);

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

        mPresenter.show(mView);
    }

    /** Releases the resources used by AccountPickerBottomSheetCoordinator. */
    @MainThread
    private void destroy() {
        mAccountPickerCoordinator.destroy();
        mAccountPickerBottomSheetMediator.destroy();
        mPresenter.destroy();
    }

    /** Implements {@link SigninBottomSheetUiCoordinator}. */
    @Override
    @MainThread
    public void dismiss() {
        mPresenter.dismiss();
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
