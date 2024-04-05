// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.Account;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.widget.ViewSwitcher;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Parent coordinator for the re-FRE promo */
public final class UpgradePromoCoordinator
        implements HistorySyncCoordinator.HistorySyncDelegate,
                FullscreenSigninCoordinator.Delegate {
    public interface Delegate {
        /** Notifies when the user clicked the "add account" button. */
        void addAccountInUpgradePromo();

        /**
         * The supplier that supplies whether reading policy value is necessary. See {@link
         * PolicyLoadListener} for details.
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

        void onFlowComplete();
    }

    private final Context mContext;
    private final OneshotSupplier<ProfileProvider> mProfileSupplier;
    private final Delegate mDelegate;
    private ViewSwitcher mViewSwitcher;
    private FullscreenSigninCoordinator mSigninCoordinator;
    private HistorySyncCoordinator mHistorySyncCoordinator;

    public UpgradePromoCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileSupplier,
            PrivacyPreferencesManager privacyPreferencesManager,
            Delegate delegate) {
        mContext = context;
        mViewSwitcher = new ViewSwitcher(context);
        mProfileSupplier = profileSupplier;
        mDelegate = delegate;
        inflateViewSwitcher();
        mSigninCoordinator =
                new FullscreenSigninCoordinator(
                        mContext, modalDialogManager, this, privacyPreferencesManager);
        mSigninCoordinator.setView(
                mViewSwitcher.findViewById(
                        org.chromium.chrome.browser.ui.signin.R.id.fullscreen_signin));
    }

    public void destroy() {
        mViewSwitcher.removeAllViews();
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }

        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
    }

    public ViewSwitcher getViewSwitcher() {
        return mViewSwitcher;
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void addAccount() {
        mDelegate.addAccountInUpgradePromo();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void acceptTermsOfService(boolean allowMetricsAndCrashUploading) {}

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void advanceToNextPage() {
        if (!shouldShowHistorySync()) {
            mDelegate.onFlowComplete();
            return;
        }
        // TODO(b/41493788): Add support for showing only the history sync screen when the user is
        // already signed in.
        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        mContext,
                        this,
                        mProfileSupplier.get().getOriginalProfile(),
                        SigninAccessPoint.SIGNIN_PROMO,
                        /* showEmailInFooter= */ false,
                        /* shouldSignOutOnDecline= */ false,
                        mViewSwitcher.findViewById(
                                org.chromium.chrome.browser.ui.signin.R.id.history_sync));
        mViewSwitcher.showNext();
        mSigninCoordinator.destroy();
        mSigninCoordinator = null;
    }

    @Override
    public void displayDeviceLockPage(Account selectedAccount) {
        // TODO(b/41496906): Maybe implement this method.
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void recordFreProgressHistogram(int state) {}

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void recordNativePolicyAndChildStatusLoadedHistogram() {
        // TODO(b/41493788): Maybe implement this method.
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void recordNativeInitializedHistogram() {
        // TODO(b/41493788): Maybe implement this method.
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void showInfoPage(int url) {}

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public OneshotSupplier<ProfileProvider> getProfileSupplier() {
        return mProfileSupplier;
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public OneshotSupplier<Boolean> getPolicyLoadListener() {
        return mDelegate.getPolicyLoadListener();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public OneshotSupplier<Boolean> getChildAccountStatusSupplier() {
        return mDelegate.getChildAccountStatusSupplier();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public Promise<Void> getNativeInitializationPromise() {
        return mDelegate.getNativeInitializationPromise();
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void dismissHistorySync() {
        mViewSwitcher.removeAllViews();
        mHistorySyncCoordinator.destroy();
        mHistorySyncCoordinator = null;
        mDelegate.onFlowComplete();
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public boolean isLargeScreen() {
        Configuration configuration = mContext.getResources().getConfiguration();
        return configuration.isLayoutSizeAtLeast(Configuration.SCREENLAYOUT_SIZE_LARGE);
    }

    private void inflateViewSwitcher() {
        Configuration configuration = mContext.getResources().getConfiguration();
        boolean useLandscapeLayout =
                configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
                        && !isLargeScreen();
        // TODO(b/41493788): Add support for screen rotation while the promo is being displayed.
        mViewSwitcher =
                (ViewSwitcher)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        useLandscapeLayout
                                                ? org.chromium.chrome.browser.ui.signin.R.layout
                                                        .upgrade_promo_landscape_view
                                                : org.chromium.chrome.browser.ui.signin.R.layout
                                                        .upgrade_promo_portrait_view,
                                        null);
    }

    private boolean shouldShowHistorySync() {
        // TODO(b/41493788): Update this method to account for enterprise policies, supervised
        // accounts, etc..
        IdentityManager identityManager =
                IdentityServicesProvider.get()
                        .getIdentityManager(mProfileSupplier.get().getOriginalProfile());
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return true;
        }
        return false;
    }
}
