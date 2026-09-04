// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Objects;

/**
 * Controller for the Enterprise Signals Disclaimer.
 *
 * <p>The disclaimer should be shown for managed users who have not accepted the disclaimer
 * previously. An attempt to show the disclaimer is made during startup and on the primary account
 * change.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerController implements SigninManager.SignInStateObserver {
    private final AppCompatActivity mActivity;
    private final BottomSheetController mBottomSheetController;
    private final ModalDialogManager mModalDialogManager;
    private final CoordinatorFactory mCoordinatorFactory;
    private final EnterpriseSignalsDisclaimerCoordinator.Delegate mDelegate;
    private final Profile mProfile;
    private final SigninManager mSigninManager;

    private @Nullable EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    private boolean mIsDestroyed;

    /** Used for testing to mock EnterpriseSignalsDisclaimerCoordinator. */
    public interface CoordinatorFactory {
        EnterpriseSignalsDisclaimerCoordinator create(
                AppCompatActivity activity,
                BottomSheetController bottomSheetController,
                ModalDialogManager modalDialogManager,
                SigninManager signinManager,
                EnterpriseSignalsDisclaimerCoordinator.Delegate delegate,
                Runnable onDestroyCallback);
    }

    /**
     * Creates an instance of {@link EnterpriseSignalsDisclaimerController} for non-OTR profiles.
     *
     * @param profile The {@link Profile} associated with the controller.
     * @param bottomSheetController The {@link BottomSheetController} for showing the disclaimer.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the modal dialog.
     * @param activity The {@link AppCompatActivity} context.
     * @return The {@link EnterpriseSignalsDisclaimerController} instance, or null if the profile is
     *     off-the-record.
     */
    public static @Nullable EnterpriseSignalsDisclaimerController maybeCreateForProfile(
            Profile profile,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            AppCompatActivity activity,
            EnterpriseSignalsDisclaimerCoordinator.Delegate delegate) {
        return maybeCreateForProfile(
                profile,
                bottomSheetController,
                modalDialogManager,
                activity,
                delegate,
                EnterpriseSignalsDisclaimerCoordinator::new);
    }

    @VisibleForTesting
    static @Nullable EnterpriseSignalsDisclaimerController maybeCreateForProfile(
            Profile profile,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            AppCompatActivity activity,
            EnterpriseSignalsDisclaimerCoordinator.Delegate delegate,
            CoordinatorFactory coordinatorFactory) {
        if (profile.isOffTheRecord()) {
            return null;
        }

        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)) {
            return null;
        }

        final SigninManager signinManager =
                Objects.requireNonNull(IdentityServicesProvider.get().getSigninManager(profile));

        return new EnterpriseSignalsDisclaimerController(
                signinManager,
                bottomSheetController,
                modalDialogManager,
                activity,
                profile,
                delegate,
                coordinatorFactory);
    }

    @VisibleForTesting
    EnterpriseSignalsDisclaimerController(
            SigninManager signinManager,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            AppCompatActivity activity,
            Profile profile,
            EnterpriseSignalsDisclaimerCoordinator.Delegate delegate,
            CoordinatorFactory coordinatorFactory) {
        mSigninManager = signinManager;
        mBottomSheetController = bottomSheetController;
        mModalDialogManager = modalDialogManager;
        mActivity = activity;
        mProfile = profile;
        mDelegate = delegate;
        mCoordinatorFactory = coordinatorFactory;
        mIsDestroyed = false;
        mSigninManager.addSignInStateObserver(this);
    }

    /**
     * Attempts to show the enterprise signals disclaimer bottom sheet if necessary.
     *
     * @return true if the disclaimer was shown (or put in a queue), false otherwise.
     */
    public boolean maybeShow() {
        if (mIsDestroyed) {
            return false;
        }

        // The disclaimer is already being shown or will be shown in the future.
        if (mCoordinator != null && mCoordinator.isActive()) {
            return false;
        }

        final IdentityManager identityManager = mSigninManager.getIdentityManager();
        final AccountInfo primaryAccountInfo = identityManager.getPrimaryAccountInfo();
        if (primaryAccountInfo == null) {
            return false;
        }

        // TODO(b/512836948): Expand this check to include all forms of management.
        if (!ManagedBrowserUtils.isProfileManaged(mProfile)) {
            return false;
        }

        final GaiaId gaiaId = primaryAccountInfo.getGaiaId();
        if (gaiaId.toString().isEmpty()) {
            // If this happens something is very wrong.
            return false;
        }
        if (EnterpriseSignalsDisclaimerBridge.hasAccountAcknowledgedSignalsDisclaimer(gaiaId)) {
            return false;
        }

        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
        mCoordinator =
                mCoordinatorFactory.create(
                        mActivity,
                        mBottomSheetController,
                        mModalDialogManager,
                        mSigninManager,
                        mDelegate,
                        this::onCoordinatorDestroyed);
        // If the dialog is not shown immediately it will be queued by the controller and shown
        // whenever possible.
        mCoordinator.show();
        return true;
    }

    public void destroy() {
        mIsDestroyed = true;
        mSigninManager.removeSignInStateObserver(this);
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        // TODO(b/553341908): Once the existing management disclaimer is replaced with the
        // enterprise signals disclaimer, this function should be removed.
        maybeShow();
    }

    @Override
    public void onSignedOut() {
        if (mCoordinator != null) {
            mCoordinator.destroy();
            mCoordinator = null;
        }
    }

    private void onCoordinatorDestroyed() {
        mCoordinator = null;
    }
}
