// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.content.Context;

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
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;

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
    private boolean mDestroyed;

    public StartupSigninStateCheckController(
            Context context,
            ModalDialogManager dialogManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MonotonicObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mDialogManager = dialogManager;
        mLifecycleDispatcher = lifecycleDispatcher;
        mProfileSupplier = profileSupplier;

        mLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        mDestroyed = true;
        mLifecycleDispatcher.unregister(this);
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
                        ConfirmManagedSyncDataDialogCoordinator.Listener listener =
                                new ConfirmManagedSyncDataDialogCoordinator.Listener() {
                                    @Override
                                    public void onConfirm() {
                                        signinManager.setUserAcceptedAccountManagement(true);
                                    }

                                    @Override
                                    public void onCancel() {
                                        signinManager.setUserAcceptedAccountManagement(false);
                                        signinManager.signOut(SignoutReason.ABORT_SIGNIN);
                                    }
                                };

                        new ConfirmManagedSyncDataDialogCoordinator(
                                mContext,
                                mDialogManager,
                                listener,
                                signinManager.extractDomainName(primaryAccount.getEmail()));
                    }
                });
    }
}
