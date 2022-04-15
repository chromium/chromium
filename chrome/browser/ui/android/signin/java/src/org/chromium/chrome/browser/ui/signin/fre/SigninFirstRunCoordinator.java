// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.content.Context;

import androidx.annotation.MainThread;
import androidx.annotation.StringRes;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator handles the update and interaction of the FRE sign-in screen.
 */
@MainThread
public class SigninFirstRunCoordinator {
    /** Delegate for signin fist run MVC. */
    public interface Delegate {
        /** Notifies when the user clicked the "add account" button. */
        void addAccount();

        /** Notifies when the user accepts the terms of service. */
        void acceptTermsOfService();

        /** Called when the interaction with the page is over and the next page should be shown. */
        void advanceToNextPage();

        /**
         * Records the FRE progress histogram MobileFre.Progress.*.
         * @param state FRE state to record.
         */
        void recordFreProgressHistogram(@MobileFreProgress int state);

        /**
         * Show an informational web page. The page doesn't show navigation control.
         * @param url Resource id for the URL of the web page.
         */
        void showInfoPage(@StringRes int url);

        /**
         * Opens a dialog to get consent for recording UMA data.
         */
        void openUmaDialog();
    }

    private final SigninFirstRunMediator mMediator;

    /**
     * Constructs a coordinator instance.
     *
     * @param context is used to create the UI.
     * @param view is the FRE view including the selected account, the continue/dismiss buttons,
     *        the footer string and other view components that change according to different state.
     * @param modalDialogManager is used to open dialogs like account picker dialog and uma dialog.
     * @param delegate is invoked to interact with classes outside the module.
     * @param privacyPreferencesManager is used to check whether metrics and crash reporting are
     *         disabled by policy and set the footer string accordingly.
     */
    public SigninFirstRunCoordinator(Context context, SigninFirstRunView view,
            ModalDialogManager modalDialogManager, Delegate delegate,
            PrivacyPreferencesManager privacyPreferencesManager) {
        mMediator = new SigninFirstRunMediator(
                context, modalDialogManager, delegate, privacyPreferencesManager);
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), view, SigninFirstRunViewBinder::bind);
    }

    /**
     * Releases the resources used by the coordinator.
     */
    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Resets model properties in {@link SigninFirstRunMediator}.
     * This method is called when the user advances to the sync consent page and then presses back
     * and returns to the FRE again.
     */
    public void reset() {
        mMediator.reset();
    }

    /**
     * Notifies that native is loaded, policies are available if any exists and child account
     * status is fetched.
     * @param hasPolicies whether policies are found on device.
     */
    public void onNativePolicyAndChildStatusLoaded(boolean hasPolicies) {
        ThreadUtils.assertOnUiThread();
        mMediator.onNativeAndPolicyLoaded(hasPolicies);
    }

    public void onAccountSelected(String accountName) {
        mMediator.onAccountSelected(accountName);
    }

    public boolean isMetricsReportingDisabledByPolicy() {
        return mMediator.isMetricsReportingDisabledByPolicy();
    }
}
