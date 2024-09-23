// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.accounts.Account;
import android.content.Context;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator handles the update and interaction of the fullscreen sign-in screen. */
@MainThread
public class FullscreenSigninCoordinator {
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
        void displayDeviceLockPage(Account selectedAccount);

        /**
         * Records the FRE progress histogram MobileFre.Progress.*. Only implemented for the FRE.
         *
         * @param state FRE state to record.
         */
        default void recordFreProgressHistogram(@MobileFreProgress int state) {}

        /**
         * Records MobileFre.FromLaunch.NativeAndPoliciesLoaded histogram. Only implemented for the
         * FRE.
         */
        default void recordNativePolicyAndChildStatusLoadedHistogram() {}

        /**
         * Records MobileFre.FromLaunch.NativeInitialized histogram. Only implemented for the FRE.
         */
        default void recordNativeInitializedHistogram() {}

        /**
         * Show an informational web page. The page doesn't show navigation control. Only
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
        Promise<Void> getNativeInitializationPromise();

        /** Returns {@code true} if the management notice should be shown on managed devices. */
        boolean shouldDisplayManagementNoticeOnManagedDevices();

        /** Returns {@code true} when the footer text should be displayed */
        boolean shouldDisplayFooterText();
    }

    private final FullscreenSigninMediator mMediator;

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
            @SigninAccessPoint int accessPoint) {
        mMediator =
                new FullscreenSigninMediator(
                        context,
                        modalDialogManager,
                        delegate,
                        privacyPreferencesManager,
                        accessPoint);
    }

    /** Releases the resources used by the coordinator. */
    public void destroy() {
        setView(null);
        mMediator.destroy();
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

    public void onAccountSelected(String accountName) {
        mMediator.onAccountSelected(accountName);
    }

    /** Continue the sign-in process with the currently selected account. */
    public void continueSignIn() {
        mMediator.proceedWithSignIn();
    }

    /** Abandon the sign-in process and dismiss the sign-in page. */
    public void cancelSignInAndDismiss() {
        mMediator.dismiss();
    }
}
