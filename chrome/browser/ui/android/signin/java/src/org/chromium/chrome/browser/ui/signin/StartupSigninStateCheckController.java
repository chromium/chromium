// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerController.ShownOn;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.modaldialog.ModalDialogManager;

// TODO(b/570998772): Move this class to the enterprise signals disclaimer directory and merge with
// the StartupEnterpriseSignalsDisclaimerController.

/**
 * Controller responsible for verifying sign-in state correctness on activity startup. Currently
 * handles recovering enterprise management disclaimer acceptance if there was an error in
 * processing the acceptance bit during sign-in.
 */
@NullMarked
public class StartupSigninStateCheckController implements NativeInitObserver, DestroyObserver {
    private final Context mContext;
    private final ModalDialogManager mDialogManager;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final EnterpriseSignalsDisclaimerController mDisclaimerController;
    private boolean mDestroyed;

    /**
     * @param context The Activity {@link Context}.
     * @param dialogManager The {@link ModalDialogManager} for showing the management notice.
     * @param bottomSheetController The {@link BottomSheetController} for showing the enterprise
     *     signals disclaimer.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} of the activity.
     * @param profileSupplier Supplier of the current {@link Profile}.
     * @param showInfoPageCallback Callback invoked with a URL to open an info page from the
     *     enterprise signals disclaimer.
     */
    public StartupSigninStateCheckController(
            Context context,
            ModalDialogManager dialogManager,
            BottomSheetController bottomSheetController,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Callback<String> showInfoPageCallback) {
        this(
                context,
                dialogManager,
                lifecycleDispatcher,
                profileSupplier,
                new EnterpriseSignalsDisclaimerController(
                        context, bottomSheetController, dialogManager, showInfoPageCallback));
    }

    @VisibleForTesting
    StartupSigninStateCheckController(
            Context context,
            ModalDialogManager dialogManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier,
            EnterpriseSignalsDisclaimerController disclaimerController) {
        mContext = context;
        mDialogManager = dialogManager;
        mLifecycleDispatcher = lifecycleDispatcher;
        mProfileSupplier = profileSupplier;
        mDisclaimerController = disclaimerController;

        mLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        mDestroyed = true;
        mLifecycleDispatcher.unregister(this);
        mDisclaimerController.destroy();
    }

    @Override
    public void onFinishNativeInitialization() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.VERIFY_STARTUP_SIGNIN_STATE)) {
            return;
        }

        Profile profile = mProfileSupplier.get();
        assert profile != null;

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(profile);
        // Incognito profile has no signin manager and this check is not necessary.
        if (signinManager == null) {
            return;
        }
        @Nullable AccountInfo primaryAccount =
                signinManager.getIdentityManager().getPrimaryAccountInfo();
        // If not signed in, there is nothing to check.
        if (primaryAccount == null) {
            return;
        }

        checkAndRecoverManagementConsent(signinManager, primaryAccount);
    }

    /**
     * Checks if the user is managed but has either not accepted the management disclaimer, there
     * was a failure in recognizing the account as managed, or the acceptance bit was otherwise lost
     * or improperly processed. If so, the user is in an incorrect state of being logged in to a
     * managed account that hasn't been acknowledged as managed. This method will recover from this
     * state by prompting the user to accept the management disclaimer. If they do not, they will be
     * signed out.
     */
    private void checkAndRecoverManagementConsent(
            SigninManager signinManager, AccountInfo primaryAccount) {
        if (signinManager.getUserAcceptedAccountManagement()) {
            return;
        }

        signinManager.isAccountManaged(
                primaryAccount,
                (Boolean isAccountManaged) -> {
                    if (mDestroyed) {
                        return;
                    }
                    if (isAccountManaged) {
                        showManagementNotice(signinManager, primaryAccount);
                    }
                });
    }

    /** Shows the management notice for the primary account. */
    private void showManagementNotice(SigninManager signinManager, AccountInfo primaryAccount) {
        ConfirmManagedSyncDataDialogCoordinator.Listener listener =
                new ConfirmManagedSyncDataDialogCoordinator.Listener() {
                    @Override
                    public void onConfirm() {
                        if (mDestroyed
                                || !isPrimaryAccount(signinManager, primaryAccount.getId())) {
                            return;
                        }
                        signinManager.setUserAcceptedAccountManagement(true);
                    }

                    @Override
                    public void onCancel() {
                        if (mDestroyed
                                || !isPrimaryAccount(signinManager, primaryAccount.getId())) {
                            return;
                        }
                        signinManager.setUserAcceptedAccountManagement(false);
                        signinManager.runAfterOperationInProgress(
                                () -> {
                                    if (!mDestroyed
                                            && signinManager.isSignOutAllowed()
                                            && isPrimaryAccount(
                                                    signinManager, primaryAccount.getId())) {
                                        signinManager.signOut(
                                                SignoutReason
                                                        .USER_DECLINED_ENTERPRISE_MANAGEMENT_DISCLAIMER);
                                    }
                                });
                    }
                };

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_NEW_MANAGEMENT_NOTICE)) {
            new ConfirmManagedSyncDataDialogCoordinator(
                    mContext,
                    mDialogManager,
                    listener,
                    signinManager.extractDomainName(primaryAccount.getEmail()));
            return;
        }

        mDisclaimerController.show(
                signinManager.getIdentityManager(),
                primaryAccount,
                ShownOn.SIGNIN_STATE_RECOVERY,
                new EnterpriseSignalsDisclaimerController.Listener() {
                    @Override
                    public void onAccepted() {
                        listener.onConfirm();
                    }

                    @Override
                    public void onDeclined() {
                        listener.onCancel();
                    }
                });
    }

    private static boolean isPrimaryAccount(
            SigninManager signinManager, CoreAccountId primaryAccountId) {
        CoreAccountInfo primaryAccountInfo =
                signinManager.getIdentityManager().getPrimaryAccountInfo();
        if (primaryAccountInfo == null) {
            return false;
        }

        return primaryAccountId.equals(primaryAccountInfo.getId());
    }
}
