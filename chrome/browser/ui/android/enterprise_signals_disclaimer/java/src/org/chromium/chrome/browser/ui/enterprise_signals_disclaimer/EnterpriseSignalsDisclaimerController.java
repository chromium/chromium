// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerCoordinator.PresentationMode;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerHost.DismissalCause;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

/**
 * Controller for the Enterprise Signals Disclaimer.
 *
 * <p>The disclaimer should be shown for managed users who have not accepted the disclaimer
 * previously. An attempt to show the disclaimer is made during startup and on the primary account
 * change.
 *
 * <p>The controller acts as the embedder of {@link EnterpriseSignalsDisclaimerCoordinator}: it
 * decides how the disclaimer is presented (a modal dialog on large form factors, a bottom sheet
 * otherwise), hosts the disclaimer view, records metrics and handles the result of the user
 * interaction.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerController implements SigninManager.SignInStateObserver {
    private static final long UNSET_TIME = -1;

    private final AppCompatActivity mActivity;
    private final CoordinatorFactory mCoordinatorFactory;
    private final HostFactory mHostFactory;
    private final Callback<String> mShowInfoPageCallback;
    private final Profile mProfile;
    private final SigninManager mSigninManager;
    private final MetricsHelper mMetricsHelper = new MetricsHelper();
    private final EnterpriseSignalsDisclaimerCoordinator.Delegate mCoordinatorDelegate =
            new CoordinatorDelegate();

    private @Nullable EnterpriseSignalsDisclaimerCoordinator mCoordinator;
    private @Nullable EnterpriseSignalsDisclaimerHost mDisclaimerHost;
    private @Nullable @MetricsHelper.ShownOn Integer mShownOn;
    private long mShownAtUptimeMillis = UNSET_TIME;
    private boolean mIsDestroyed;

    /** Used for testing to mock EnterpriseSignalsDisclaimerCoordinator. */
    interface CoordinatorFactory {
        EnterpriseSignalsDisclaimerCoordinator create(
                Context context,
                IdentityManager identityManager,
                CoreAccountInfo account,
                @PresentationMode int presentationMode,
                EnterpriseSignalsDisclaimerCoordinator.Delegate delegate);
    }

    /** Creates the host for the disclaimer. Can be replaced in tests. */
    interface HostFactory {
        EnterpriseSignalsDisclaimerHost create(
                @PresentationMode int presentationMode,
                EnterpriseSignalsDisclaimerCoordinator coordinator,
                Consumer<@DismissalCause Integer> onDismissedCallback);
    }

    /** Handles callbacks from the {@link EnterpriseSignalsDisclaimerCoordinator}. */
    private class CoordinatorDelegate implements EnterpriseSignalsDisclaimerCoordinator.Delegate {
        @Override
        public void showInfoPage(String url) {
            mShowInfoPageCallback.onResult(url);
        }

        @Override
        public void onShown() {
            MetricsHelper.recordShown(assertNonNull(mShownOn));
            mShownAtUptimeMillis = TimeUtils.uptimeMillis();
        }

        @Override
        public void onAccept() {
            if (mDisclaimerHost != null) {
                mDisclaimerHost.dismiss(DismissalCause.TAPPED_ACCEPT);
            }
        }

        @Override
        public void onDecline() {
            if (mDisclaimerHost != null) {
                mDisclaimerHost.dismiss(DismissalCause.TAPPED_SIGN_OUT);
            }
        }
    }

    /**
     * Creates an instance of {@link EnterpriseSignalsDisclaimerController} for non-OTR profiles.
     *
     * @param profile The {@link Profile} associated with the controller.
     * @param bottomSheetController The {@link BottomSheetController} for showing the disclaimer.
     * @param modalDialogManager The {@link ModalDialogManager} for showing the modal dialog.
     * @param activity The {@link AppCompatActivity} context.
     * @param showInfoPageCallback Callback invoked with a URL to open an info page.
     * @return The {@link EnterpriseSignalsDisclaimerController} instance, or null if the profile is
     *     off-the-record.
     */
    public static @Nullable EnterpriseSignalsDisclaimerController maybeCreateForProfile(
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
    static @Nullable EnterpriseSignalsDisclaimerController maybeCreateForProfile(
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

        return new EnterpriseSignalsDisclaimerController(
                signinManager,
                activity,
                profile,
                showInfoPageCallback,
                coordinatorFactory,
                hostFactory);
    }

    /**
     * Returns a {@link HostFactory} which presents the disclaimer in a modal dialog for {@link
     * PresentationMode#MODAL_DIALOG} and in a bottom sheet otherwise.
     */
    @VisibleForTesting
    static HostFactory createDefaultHostFactory(
            BottomSheetController bottomSheetController, ModalDialogManager modalDialogManager) {
        return (presentationMode, coordinator, onDismissedCallback) -> {
            if (presentationMode == PresentationMode.MODAL_DIALOG) {
                return new ModalDialogDisclaimerHost(
                        modalDialogManager, coordinator.getView(), onDismissedCallback);
            }
            return new BottomSheetDisclaimerHost(
                    bottomSheetController,
                    coordinator.getView(),
                    coordinator::getVerticalScrollOffset,
                    onDismissedCallback);
        };
    }

    @VisibleForTesting
    EnterpriseSignalsDisclaimerController(
            SigninManager signinManager,
            AppCompatActivity activity,
            Profile profile,
            Callback<String> showInfoPageCallback,
            CoordinatorFactory coordinatorFactory,
            HostFactory hostFactory) {
        mSigninManager = signinManager;
        mActivity = activity;
        mProfile = profile;
        mShowInfoPageCallback = showInfoPageCallback;
        mCoordinatorFactory = coordinatorFactory;
        mHostFactory = hostFactory;
        mIsDestroyed = false;
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
            // Passing an empty list of known accounts clears the acknowledgment for every
            // account, so the disclaimer is shown again on each startup.
            EnterpriseSignalsDisclaimerBridge.removeUnknownAccounts(List.of());
        }

        return maybeShow(MetricsHelper.ShownOn.STARTUP);
    }

    @VisibleForTesting
    boolean maybeShow(@MetricsHelper.ShownOn int shownOn) {
        if (mIsDestroyed) {
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
        if (gaiaId.toString().isEmpty()) {
            // If this happens something is very wrong.
            return false;
        }
        if (EnterpriseSignalsDisclaimerBridge.hasAccountAcknowledgedSignalsDisclaimer(gaiaId)) {
            return false;
        }

        destroyDisclaimer();

        mShownOn = shownOn;
        @PresentationMode int presentationMode = getPresentationMode();
        mCoordinator =
                mCoordinatorFactory.create(
                        mActivity,
                        identityManager,
                        primaryAccountInfo,
                        presentationMode,
                        mCoordinatorDelegate);
        mDisclaimerHost =
                mHostFactory.create(
                        presentationMode,
                        mCoordinator,
                        (dismissalCause) -> onDialogDismissed(dismissalCause, primaryAccountInfo));
        // If the dialog is not shown immediately it will be queued by the host and shown
        // whenever possible.
        MetricsHelper.recordShownRequested(shownOn);
        mDisclaimerHost.show();
        return true;
    }

    public void destroy() {
        mIsDestroyed = true;
        mSigninManager.removeSignInStateObserver(this);
        destroyDisclaimer();
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        // TODO(b/553341908): Once the existing management disclaimer is replaced with the
        // enterprise signals disclaimer, this function should be removed.
        maybeShow(MetricsHelper.ShownOn.SIGN_IN);
    }

    @Override
    public void onSignedOut() {
        destroyDisclaimer();
    }

    /** Large form factors get a modal dialog, while smaller screens get a bottom sheet. */
    private @PresentationMode int getPresentationMode() {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)
                ? PresentationMode.MODAL_DIALOG
                : PresentationMode.BOTTOM_SHEET;
    }

    private boolean isDisclaimerActive() {
        return mDisclaimerHost != null && mDisclaimerHost.isActive();
    }

    /** Tears down the currently displayed (or queued) disclaimer, if any. */
    private void destroyDisclaimer() {
        if (mDisclaimerHost != null) {
            var host = mDisclaimerHost;
            mDisclaimerHost = null;
            host.destroy();
        }
        if (mCoordinator != null) {
            var coordinator = mCoordinator;
            mCoordinator = null;
            coordinator.destroy();
        }
        mShownOn = null;
        mShownAtUptimeMillis = UNSET_TIME;
    }

    private void onDialogDismissed(@DismissalCause int dismissalCause, CoreAccountInfo account) {
        mMetricsHelper.recordResult(dismissalCause);
        if (mShownAtUptimeMillis != UNSET_TIME) {
            MetricsHelper.recordTimeToUserAction(TimeUtils.uptimeMillis() - mShownAtUptimeMillis);
            mShownAtUptimeMillis = UNSET_TIME;
        }

        destroyDisclaimer();

        if (dismissalCause == DismissalCause.TAPPED_ACCEPT) {
            assert !account.getGaiaId().toString().isEmpty();
            EnterpriseSignalsDisclaimerBridge.setAccountAcknowledgedSignalsDisclaimer(
                    account.getGaiaId());
        } else if (shouldSignOutBasedOnDismissalCause(dismissalCause)) {
            mSigninManager.runAfterOperationInProgress(
                    () -> {
                        if (mSigninManager.isSignOutAllowed()) {
                            mSigninManager.signOut(
                                    SignoutReason.USER_DECLINED_ENTERPRISE_SIGNALS_DISCLAIMER);
                        }
                    });
        }
    }

    private static boolean shouldSignOutBasedOnDismissalCause(@DismissalCause int dismissalCause) {
        return dismissalCause == DismissalCause.TAPPED_SIGN_OUT
                || dismissalCause == DismissalCause.DISMISSED_BY_BACK_PRESS
                || dismissalCause == DismissalCause.DISMISSED_BY_SWIPE_DOWN
                || dismissalCause == DismissalCause.DISMISSED_BY_TAP_OUTSIDE
                || dismissalCause == DismissalCause.DISMISSED_BY_CLOSE_BUTTON;
    }
}
