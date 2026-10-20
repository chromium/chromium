// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import android.app.Activity;
import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.signin.services.AccountPreviewDataService;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetView.PresentationMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the seamless sign-in flow. This class handles the UI logic where the user signs in
 * directly from a promo. The bottom sheet is not shown by default for seamless sign-in; it will
 * only be displayed in cases of sign-in errors, or for management notices.
 */
@NullMarked
public class SeamlessSigninCoordinator
        implements SigninBottomSheetUiCoordinator, AccountPickerBottomSheetMediator.Delegate {

    private final Activity mActivity;
    private final IdentityManager mIdentityManager;
    private final boolean mUseDialog;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final AccountPickerBottomSheetMediator mAccountPickerBottomSheetMediator;
    private final AccountPickerPresenter mPresenter;

    private @Nullable AccountPickerBottomSheetView mView;
    private @Nullable EnterpriseSignalsDisclaimerCoordinator mManagementNoticeCoordinator;
    private boolean mIsDestroyed;

    /**
     * Constructs the SeamlessSigninCoordinator.
     *
     * @param windowAndroid The current activity window.
     * @param activity The {@link Activity} that hosts the sign-in flow.
     * @param identityManager The IdentityManager for the current profile.
     * @param signinManager The sign-in manager to start the sign-in.
     * @param accountPreviewDataService The service to retrieve account preview data.
     * @param modalDialogManager The {@link ModalDialogManager} for the current activity.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param accountPickerDelegate The delegate for account picker actions.
     * @param accountPickerBottomSheetStrings The strings for the account picker bottom sheet.
     * @param deviceLockActivityLauncher The launcher to start up the device lock page.
     * @param signinAccessPoint The entry point for the sign-in flow.
     * @param selectedAccountId The account to be signed in seamlessly.
     */
    @MainThread
    public SeamlessSigninCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            IdentityManager identityManager,
            SigninManager signinManager,
            @Nullable AccountPreviewDataService accountPreviewDataService,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            AccountPickerDelegate accountPickerDelegate,
            AccountPickerBottomSheetStrings accountPickerBottomSheetStrings,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            @SigninAccessPoint int signinAccessPoint,
            CoreAccountId selectedAccountId) {
        mActivity = activity;
        mIdentityManager = identityManager;
        mSigninAccessPoint = signinAccessPoint;
        mUseDialog = SigninUtils.shouldShowAccountPickerDialog(activity);
        AccountPickerDismissalLogger dismissalLogger =
                new AccountPickerDismissalLogger(signinAccessPoint, /* isWebSignin= */ false);
        mPresenter =
                mUseDialog
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
        mAccountPickerBottomSheetMediator =
                AccountPickerBottomSheetMediator.createForSeamlessSignin(
                        windowAndroid,
                        identityManager,
                        signinManager,
                        accountPreviewDataService,
                        accountPickerDelegate,
                        this::requestDisplayUi,
                        this::dismiss,
                        accountPickerBottomSheetStrings,
                        deviceLockActivityLauncher,
                        signinAccessPoint,
                        selectedAccountId,
                        /* delegate= */ this);
    }

    @MainThread
    public void launchSigninFlow() {
        mAccountPickerBottomSheetMediator.launchDeviceLockIfNeededAndSignIn();
    }

    @MainThread
    void destroy() {
        if (mIsDestroyed) {
            return;
        }

        mIsDestroyed = true;
        destroyManagementNoticeCoordinator();
        mAccountPickerBottomSheetMediator.destroy();
        mPresenter.destroy();
    }

    /**
     * Displays the UI (dialog or bottom sheet) to present a seamless sign-in error or managed
     * account confirmation.
     */
    @MainThread
    void requestDisplayUi() {
        if (mView == null) {
            // UI initialized lazily, in most cases no UI will be shown
            mView =
                    new AccountPickerBottomSheetView(
                            mActivity,
                            mAccountPickerBottomSheetMediator,
                            mUseDialog
                                    ? PresentationMode.MODAL_DIALOG
                                    : PresentationMode.BOTTOM_SHEET);
            // The management notice may have been set up before the view was created.
            attachManagementNoticeToView();
            PropertyModelChangeProcessor.create(
                    mAccountPickerBottomSheetMediator.getModel(),
                    mView,
                    AccountPickerBottomSheetViewBinder::bind);

            mPresenter.show(mView);
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SHOWN, mSigninAccessPoint);
        }
    }

    /** Implements {@link AccountPickerBottomSheetMediator.Delegate}. */
    @Override
    public void setUpManagementNoticeForAccount(CoreAccountInfo account) {
        destroyManagementNoticeCoordinator();
        mManagementNoticeCoordinator =
                new EnterpriseSignalsDisclaimerCoordinator(
                        mActivity,
                        mIdentityManager,
                        account,
                        mUseDialog
                                ? EnterpriseSignalsDisclaimerCoordinator.PresentationMode
                                        .MODAL_DIALOG
                                : EnterpriseSignalsDisclaimerCoordinator.PresentationMode
                                        .BOTTOM_SHEET,
                        mAccountPickerBottomSheetMediator);
        // If the view isn't created yet, the notice is attached in {@link #requestDisplayUi()}.
        attachManagementNoticeToView();
    }

    private void attachManagementNoticeToView() {
        if (mView == null || mManagementNoticeCoordinator == null) return;
        mView.setManagementNoticeView(
                mManagementNoticeCoordinator.getView(),
                mManagementNoticeCoordinator.getTitleView(),
                mManagementNoticeCoordinator::getVerticalScrollOffset);
    }

    private void destroyManagementNoticeCoordinator() {
        if (mManagementNoticeCoordinator == null) return;
        mManagementNoticeCoordinator.destroy();
        mManagementNoticeCoordinator = null;
    }

    /** Implements {@link SigninBottomSheetUiCoordinator}. Dismiss the UI, if shown. */
    @Override
    @MainThread
    public void dismiss() {
        if (mView != null) {
            mPresenter.dismiss();
        } else {
            destroy();
        }
    }

    /** Implements {@link SigninBottomSheetUiCoordinator}. */
    @Override
    public void onAccountAdded(String accountEmail) {
        throw new IllegalStateException(
                "The 'add account' flow is not supported from the seamless sign-in flow as it's"
                        + " designed to be a non-interactive flow for a pre-selected account.");
    }

    @Nullable View getBottomSheetViewForTesting() {
        return mView == null ? null : mView.getContentView();
    }
}
