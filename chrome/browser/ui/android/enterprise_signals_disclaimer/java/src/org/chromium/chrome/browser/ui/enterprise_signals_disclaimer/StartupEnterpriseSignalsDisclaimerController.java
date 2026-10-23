// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Objects;

/**
 * Controller for the Enterprise Signals Disclaimer.
 *
 * <p>The disclaimer should be shown for managed users who have not accepted the disclaimer
 * previously. An attempt to show the disclaimer is made during startup and on the primary account
 * change.
 *
 * <p>On top of the presentation and metrics handled by {@link
 * EnterpriseSignalsDisclaimerController}, this controller decides when the disclaimer should be
 * shown and handles the result of the user interaction.
 */
@NullMarked
public class StartupEnterpriseSignalsDisclaimerController
        extends EnterpriseSignalsDisclaimerController implements SigninManager.SignInStateObserver {
    private final Profile mProfile;
    private final SigninManager mSigninManager;

    /**
     * Creates an instance of {@link StartupEnterpriseSignalsDisclaimerController} for non-OTR
     * profiles.
     *
     * @param profile The {@link Profile} associated with the controller.
     * @param bottomSheetController The {@link BottomSheetController} for showing the disclaimer.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the modal dialog.
     * @param activity The {@link AppCompatActivity} context.
     * @param showInfoPageCallback Callback invoked with a URL to open an info page.
     * @return The {@link StartupEnterpriseSignalsDisclaimerController} instance, or null if the
     *     profile is off-the-record.
     */
    public static @Nullable StartupEnterpriseSignalsDisclaimerController maybeCreateForProfile(
            Profile profile,
            BottomSheetController bottomSheetController,
            ModalDialogManager modalDialogManager,
            AppCompatActivity activity,
            Callback<String> showInfoPageCallback) {
        return maybeCreateForProfile(
                profile,
                activity,
                showInfoPageCallback,
                EnterpriseSignalsDisclaimerCoordinator::new,
                createDefaultHostFactory(bottomSheetController, modalDialogManager));
    }

    @VisibleForTesting
    static @Nullable StartupEnterpriseSignalsDisclaimerController maybeCreateForProfile(
            Profile profile,
            AppCompatActivity activity,
            Callback<String> showInfoPageCallback,
            CoordinatorFactory coordinatorFactory,
            HostFactory hostFactory) {
        if (profile.isOffTheRecord()) {
            return null;
        }

        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)) {
            return null;
        }

        final SigninManager signinManager =
                Objects.requireNonNull(IdentityServicesProvider.get().getSigninManager(profile));

        return new StartupEnterpriseSignalsDisclaimerController(
                signinManager,
                activity,
                profile,
                showInfoPageCallback,
                coordinatorFactory,
                hostFactory);
    }

    @VisibleForTesting
    StartupEnterpriseSignalsDisclaimerController(
            SigninManager signinManager,
            AppCompatActivity activity,
            Profile profile,
            Callback<String> showInfoPageCallback,
            CoordinatorFactory coordinatorFactory,
            HostFactory hostFactory) {
        super(activity, showInfoPageCallback, coordinatorFactory, hostFactory);
        mSigninManager = signinManager;
        mProfile = profile;
        mSigninManager.addSignInStateObserver(this);
    }

    /**
     * Attempts to show the enterprise signals disclaimer bottom sheet if necessary.
     *
     * @return true if the disclaimer was shown (or put in a queue), false otherwise.
     */
    public boolean maybeShowOnStartup() {
        // This is used for testing only and will be removed together with the flag.
        if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.ANDROID_DEVICE_SIGNALS_DISCLAIMER,
                ChromeFeatureList.ANDROID_DEVICE_SIGNALS_DISCLAIMER_CLEAR_CONSENT)) {
            // Clears the acknowledgment for the primary account, so the disclaimer is shown again
            // on each startup.
            final CoreAccountInfo primaryAccountInfo =
                    mSigninManager.getIdentityManager().getPrimaryAccountInfo();
            if (primaryAccountInfo != null) {
                EnterpriseSignalsDisclaimerBridge.clearAccountAcknowledgedSignalsDisclaimer(
                        mProfile, primaryAccountInfo.getGaiaId());
            }
        }

        return maybeShow(ShownOn.STARTUP);
    }

    @VisibleForTesting
    boolean maybeShow(@ShownOn int shownOn) {
        if (isDestroyed()) {
            return false;
        }

        // The disclaimer is already being shown or will be shown in the future.
        if (isDisclaimerActive()) {
            return false;
        }

        final IdentityManager identityManager = mSigninManager.getIdentityManager();
        final AccountInfo primaryAccountInfo = identityManager.getPrimaryAccountInfo();
        if (primaryAccountInfo == null) {
            return false;
        }

        if (!ManagedBrowserUtils.isProfileManaged(mProfile)) {
            return false;
        }

        final GaiaId gaiaId = primaryAccountInfo.getGaiaId();
        if (EnterpriseSignalsDisclaimerBridge.hasAccountAcknowledgedSignalsDisclaimer(
                mProfile, gaiaId)) {
            return false;
        }

        return show(
                identityManager,
                primaryAccountInfo,
                shownOn,
                new Listener() {
                    @Override
                    public void onAccepted() {
                        onDisclaimerAccepted(primaryAccountInfo);
                    }

                    @Override
                    public void onDeclined() {
                        onDisclaimerDeclined();
                    }
                });
    }

    @Override
    public void destroy() {
        mSigninManager.removeSignInStateObserver(this);
        super.destroy();
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        // TODO(b/553341908): Once the existing management disclaimer is replaced with the
        // enterprise signals disclaimer, this function should be removed.
        maybeShow(ShownOn.SIGN_IN);
    }

    @Override
    public void onSignedOut() {
        destroyDisclaimer();
    }

    private void onDisclaimerAccepted(CoreAccountInfo account) {
        EnterpriseSignalsDisclaimerBridge.setAccountAcknowledgedSignalsDisclaimer(
                mProfile, account.getGaiaId());
    }

    private void onDisclaimerDeclined() {
        mSigninManager.runAfterOperationInProgress(
                () -> {
                    if (mSigninManager.isSignOutAllowed()) {
                        mSigninManager.signOut(
                                SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER);
                    }
                });
    }
}
