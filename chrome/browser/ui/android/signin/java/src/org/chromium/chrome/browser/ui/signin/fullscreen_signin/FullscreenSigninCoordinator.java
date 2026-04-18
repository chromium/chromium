// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.MainThread;
import androidx.annotation.StringRes;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator handles the update and interaction of the fullscreen sign-in screen. */
@NullMarked
@MainThread
public class FullscreenSigninCoordinator implements IdentityManager.Observer {
    /** Delegate for the fullscreen signin MVC. */
    public interface Delegate {
        /** Notifies when the user clicked the "add account" button. */
        void addAccount();

        /**
         * Notifies when the user accepts the terms of service. Only implemented for the FRE.
         *
         * @param allowMetricsAndCrashUploading Whether the user has opted into uploading crash
         *     reports and UMA.
         */
        default void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {}

        /** Called when the interaction with the page is over and the next page should be shown. */
        void advanceToNextPage();

        /** Called to display the device lock page */
        void displayDeviceLockPage(CoreAccountId selectedAccountId);

        /**
         * Records histograms corresponding to the user accepting sign-in.
         *
         * @param promoAction the promo action corresponding to the account used to sign in.
         */
        void recordUserSignInHistograms(
                @org.chromium.components.signin.metrics.AccountConsistencyPromoAction
                        int promoAction);

        /** Records histograms corresponding to the user dismissing the sign-in screen. */
        void recordSigninDismissedHistograms();

        /**
         * Records the relevant histograms once the initial load is completed.
         *
         * @param slowestLoadPoint The slowest load point to be recorded.
         */
        void recordLoadCompletedHistograms(
                @FullscreenSigninMediator.LoadPoint int slowestLoadPoint);

        /** Records *.FromLaunch.NativeInitialized histogram. */
        void recordNativeInitializedHistogram();

        /**
         * Shows an informational web page. The page doesn't show navigation control. Only
         * implemented for the FRE.
         *
         * @param url Resource id for the URL of the web page.
         */
        default void showInfoPage(@StringRes int url) {}

        /** Returns the supplier that provides the Profile (when available). */
        OneshotSupplier<ProfileProvider> getProfileSupplier();

        /**
         * The supplier that supplies whether reading policy value is necessary.
         * See {@link PolicyLoadListener} for details.
         */
        OneshotSupplier<Boolean> getPolicyLoadListener();

        /** Returns the supplier that supplies child account status. */
        OneshotSupplier<Boolean> getChildAccountStatusSupplier();

        /**
         * Returns the promise that provides information about native initialization. Callers can
         * use {@link Promise#isFulfilled()} to check whether the native has already been
         * initialized.
         */
        Promise<@Nullable Void> getNativeInitializationPromise();

        /** Returns {@code true} if the management notice should be shown on managed devices. */
        boolean shouldDisplayManagementNoticeOnManagedDevices();

        /** Returns {@code true} when the footer text should be displayed */
        boolean shouldDisplayFooterText();
    }

    private final FullscreenSigninMediator mMediator;
    private final Delegate mDelegate;
    @Nullable private IdentityManager mIdentityManager;

    @Nullable
    private PropertyModelChangeProcessor<PropertyModel, FullscreenSigninView, PropertyKey>
            mPropertyModelChangeProcessor;

    /**
     * Constructs a coordinator instance.
     *
     * @param context is used to create the UI.
     * @param modalDialogManager is used to open dialogs like account picker dialog and uma dialog.
     * @param delegate is invoked to interact with classes outside the module.
     * @param privacyPreferencesManager is used to check whether metrics and crash reporting are
     *     disabled by policy and set the footer string accordingly.
     */
    public FullscreenSigninCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            Delegate delegate,
            PrivacyPreferencesManager privacyPreferencesManager,
            FullscreenSigninConfig config,
            @SigninAccessPoint int accessPoint) {
        mDelegate = delegate;
        mDelegate.getProfileSupplier().onAvailable(this::onProfileAvailable);
        mMediator =
                new FullscreenSigninMediator(
                        context,
                        modalDialogManager,
                        mDelegate,
                        privacyPreferencesManager,
                        config,
                        accessPoint);
    }

    private void onProfileAvailable(ProfileProvider result) {
        mIdentityManager =
                IdentityServicesProvider.get().getIdentityManager(result.getOriginalProfile());
        assumeNonNull(mIdentityManager).addObserver(this);
    }

    /** Releases the resources used by the coordinator. */
    public void destroy() {
        setView(null);
        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
        }
        mMediator.destroy();
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        if (mMediator.isContinueOrDismissClicked()) {
            // If the sign-in occurred through this promo, then it is already being handled.
            return;
        }
        if (eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)
                == PrimaryAccountChangeEvent.Type.SET) {
            mDelegate.advanceToNextPage();
        }
    }

    /**
     * Resets model properties in {@link FullscreenSigninMediator}. This method is called when the
     * user advances to the next page and then presses back and returns to the FRE again.
     */
    public void reset() {
        mMediator.reset();
    }

    /**
     * Sets the view that is controlled by the coordinator.
     *
     * @param view is the FRE view including the selected account, the continue/dismiss buttons, the
     *     footer string and other view components that change according to different state. Can be
     *     null, in which case the coordinator will just detach from the previous view.
     */
    public void setView(@Nullable FullscreenSigninView view) {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }

        if (view != null) {
            mPropertyModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mMediator.getModel(), view, FullscreenSigninViewBinder::bind);
        }
    }

    public void onAccountAdded(String accountName) {
        mMediator.onAccountAdded(accountName);
    }

    /** Continue the sign-in process with the currently selected account. */
    public void continueSignIn() {
        mMediator.proceedWithSignIn();
    }

    /** Abandon the sign-in process and dismiss the sign-in page. */
    public void cancelSignInAndDismiss() {
        mMediator.dismiss();
    }

    public Drawable getProfilePictureForTesting() {
        return mMediator.getProfilePictureForTesting(); // IN-TEST
    }

    public void setStartAnimationForTesting(boolean start) {
        mMediator.setStartAnimationForTesting(start); // IN-TEST
    }

    public @Nullable BadgeConfig getSigninAnimationBadgeConfigForTesting() {
        return mMediator.getSigninAnimationBadgeConfigForTesting(); // IN-TEST
    }

    public @Nullable BadgeConfig getContinueButtonBadgeConfigForTesting() {
        return mMediator.getContinueButtonBadgeConfigForTesting(); // IN-TEST
    }
}
