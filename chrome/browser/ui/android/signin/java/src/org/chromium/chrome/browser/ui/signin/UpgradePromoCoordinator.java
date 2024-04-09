// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.Account;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.widget.ViewSwitcher;

import androidx.annotation.IntDef;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninView;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

    /**
     * The view switcher child UIs and their order. The order matches the order of the children
     * views in upgrade_promo_portrait/landscape_view.xml
     */
    @IntDef({
        ViewSwitcherChild.SIGNIN,
        ViewSwitcherChild.HISTORY_SYNC,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ViewSwitcherChild {
        /** The fullscreen sign-in UI. */
        int SIGNIN = 0;

        /** The History Sync opt-in UI. */
        int HISTORY_SYNC = 1;
    }

    private final Context mContext;
    private final OneshotSupplier<ProfileProvider> mProfileSupplier;
    private final Delegate mDelegate;
    private final boolean mDidShowSignin;
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
        if (isSignedIn()) {
            advanceToNextPage();
            mDidShowSignin = false;
        } else {
            mSigninCoordinator =
                    new FullscreenSigninCoordinator(
                            mContext, modalDialogManager, this, privacyPreferencesManager);
            mSigninCoordinator.setView((FullscreenSigninView) mViewSwitcher.getCurrentView());
            mDidShowSignin = true;
        }
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
        // TODO(b/41493788): Update this method to account for enterprise policies, supervised
        // accounts, etc..
        if (!isSignedIn() || mViewSwitcher.getDisplayedChild() == ViewSwitcherChild.HISTORY_SYNC) {
            mDelegate.onFlowComplete();
            return;
        }
        mViewSwitcher.setDisplayedChild(ViewSwitcherChild.HISTORY_SYNC);
        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        mContext,
                        this,
                        mProfileSupplier.get().getOriginalProfile(),
                        SigninAccessPoint.SIGNIN_PROMO,
                        /* showEmailInFooter= */ !mDidShowSignin,
                        /* shouldSignOutOnDecline= */ false,
                        mViewSwitcher.getCurrentView());
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }
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

    /**
     * Removes existing views from the view switcher and re-inflates them with the correct layout
     * after a configuration change.
     */
    public void recreateLayoutAfterConfigurationChange() {
        mViewSwitcher.removeAllViews();
        mViewSwitcher = null;
        inflateViewSwitcher();
        if (mSigninCoordinator != null) {
            mViewSwitcher.setDisplayedChild(ViewSwitcherChild.SIGNIN);
            mSigninCoordinator.setView((FullscreenSigninView) mViewSwitcher.getCurrentView());
            return;
        }
        advanceToNextPage();
    }

    private void inflateViewSwitcher() {
        Configuration configuration = mContext.getResources().getConfiguration();
        boolean useLandscapeLayout =
                configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
                        && !isLargeScreen();
        mViewSwitcher =
                (ViewSwitcher)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        useLandscapeLayout
                                                ? R.layout.upgrade_promo_landscape_view
                                                : R.layout.upgrade_promo_portrait_view,
                                        null);
    }

    private boolean isSignedIn() {
        IdentityManager identityManager =
                IdentityServicesProvider.get()
                        .getIdentityManager(mProfileSupplier.get().getOriginalProfile());
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
    }
}
