// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.accounts.Account;
import android.content.Context;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
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

        /**
         * Notifies when the user accepts the terms of service.
         * @param allowMetricsAndCrashUploading Whether the user has opted into uploading crash
         *         reports and UMA.
         * */
        void acceptTermsOfService(boolean allowMetricsAndCrashUploading);

        /** Called when the interaction with the page is over and the next page should be shown. */
        void advanceToNextPage();

        /** Called to display the device lock page  */
        void displayDeviceLockPage(Account selectedAccount);

        /**
         * Records the FRE progress histogram MobileFre.Progress.*.
         * @param state FRE state to record.
         */
        void recordFreProgressHistogram(@MobileFreProgress int state);

        /** Records MobileFre.FromLaunch.NativeAndPoliciesLoaded histogram. **/
        void recordNativePolicyAndChildStatusLoadedHistogram();

        /** Records MobileFre.FromLaunch.NativeInitialized histogram. **/
        void recordNativeInitializedHistogram();

        /**
         * Show an informational web page. The page doesn't show navigation control.
         * @param url Resource id for the URL of the web page.
         */
        void showInfoPage(@StringRes int url);

        /**
         * The supplier that supplies whether reading policy value is necessary.
         * See {@link PolicyLoadListener} for details.
         */
        OneshotSupplier<Boolean> getPolicyLoadListener();

        /**
         * Returns the supplier that supplies child account status.
         */
        OneshotSupplier<Boolean> getChildAccountStatusSupplier();

        /**
         * Returns the promise that provides information about native initialization. Callers can
         * use {@link Promise#isFulfilled()} to check whether the native has already been
         * initialized.
         */
        Promise<Void> getNativeInitializationPromise();
    }

    private final SigninFirstRunMediator mMediator;

    @Nullable
    private PropertyModelChangeProcessor<PropertyModel, SigninFirstRunView, PropertyKey>
            mPropertyModelChangeProcessor;

    /**
     * Constructs a coordinator instance.
     *
     * @param context is used to create the UI.
     * @param modalDialogManager is used to open dialogs like account picker dialog and uma dialog.
     * @param delegate is invoked to interact with classes outside the module.
     * @param privacyPreferencesManager is used to check whether metrics and crash reporting are
     *         disabled by policy and set the footer string accordingly.
     */
    public SigninFirstRunCoordinator(Context context, ModalDialogManager modalDialogManager,
            Delegate delegate, PrivacyPreferencesManager privacyPreferencesManager) {
        mMediator = new SigninFirstRunMediator(
                context, modalDialogManager, delegate, privacyPreferencesManager);
    }

    /**
     * Releases the resources used by the coordinator.
     */
    public void destroy() {
        setView(null);
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
     * Sets the view that is controlled by the coordinator.
     * @param view is the FRE view including the selected account, the continue/dismiss buttons,
     *        the footer string and other view components that change according to different state.
     *        Can be null, in which case the coordinator will just detach from the previous view.
     */
    public void setView(@Nullable SigninFirstRunView view) {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }

        if (view != null) {
            mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mMediator.getModel(), view, SigninFirstRunViewBinder::bind);
        }
    }

    public void onAccountSelected(String accountName) {
        mMediator.onAccountSelected(accountName);
    }

    /**
     * Continue the sign-in process with the currently selected account.
     */
    public void continueSignIn() {
        mMediator.proceedWithSignIn();
    }

    /**
     * Abandon the sign-in process and dismiss the sign-in page.
     */
    public void cancelSignInAndDismiss() {
        mMediator.dismiss();
    }
}
