// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.accounts.Account;
import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninView;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncView;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Parent coordinator for the re-FRE promo */
public final class UpgradePromoCoordinator
        implements HistorySyncCoordinator.HistorySyncDelegate,
                FullscreenSigninCoordinator.Delegate {
    public interface Delegate {
        /** Notifies when the user clicked the "add account" button. */
        void addAccount();

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
        ChildView.SIGNIN,
        ChildView.HISTORY_SYNC,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ChildView {
        /** The fullscreen sign-in UI. */
        int SIGNIN = 0;

        /** The History Sync opt-in UI. */
        int HISTORY_SYNC = 1;
    }

    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private final OneshotSupplier<ProfileProvider> mProfileSupplier;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final Delegate mDelegate;
    private final boolean mDidShowSignin;
    private @ChildView int mCurrentView;
    private FullscreenSigninView mFullscreenSigninView;
    private View mHistorySyncView;
    private FrameLayout mViewHolder;
    private FullscreenSigninCoordinator mSigninCoordinator;
    private HistorySyncCoordinator mHistorySyncCoordinator;

    public UpgradePromoCoordinator(
            Activity activity,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileSupplier,
            PrivacyPreferencesManager privacyPreferencesManager,
            Delegate delegate) {
        mActivity = activity;
        mCurrentView = ChildView.SIGNIN;
        mViewHolder = new FrameLayout(activity);
        mViewHolder.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(mActivity));
        mModalDialogManager = modalDialogManager;
        mProfileSupplier = profileSupplier;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mDelegate = delegate;
        inflateViewBundle();
        if (isSignedIn()) {
            advanceToNextPage();
            mDidShowSignin = false;
        } else {
            mSigninCoordinator =
                    new FullscreenSigninCoordinator(
                            mActivity,
                            mModalDialogManager,
                            this,
                            mPrivacyPreferencesManager,
                            SigninAccessPoint.SIGNIN_PROMO);
            mViewHolder.addView(getCurrentChildView());
            mSigninCoordinator.setView((FullscreenSigninView) getCurrentChildView());
            // TODO(crbug.com/347657449): Record other AccountConsistencyPromoActions.
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SHOWN, SigninAccessPoint.SIGNIN_PROMO);
            mDidShowSignin = true;
        }
    }

    public void destroy() {
        mViewHolder.removeAllViews();
        if (mSigninCoordinator != null) {
            mSigninCoordinator.destroy();
            mSigninCoordinator = null;
        }

        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
    }

    public View getView() {
        return mViewHolder;
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void addAccount() {
        mDelegate.addAccount();
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public void advanceToNextPage() {
        if (!isSignedIn() || mCurrentView == ChildView.HISTORY_SYNC) {
            mDelegate.onFlowComplete();
            return;
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO)) {
            // Always show history sync when the upgrade promo was forced on by a flag.
            showChildView(ChildView.HISTORY_SYNC);
            return;
        }
        Profile profile = mProfileSupplier.get().getOriginalProfile();
        HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
        if (historySyncHelper.shouldSuppressHistorySync() || historySyncHelper.isDeclinedOften()) {
            historySyncHelper.recordHistorySyncNotShown(SigninAccessPoint.SIGNIN_PROMO);
            mDelegate.onFlowComplete();
            return;
        }
        showChildView(ChildView.HISTORY_SYNC);
    }

    @Override
    public void displayDeviceLockPage(Account selectedAccount) {
        // TODO(b/41496906): Maybe implement this method.
    }

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

    /** Implements {@link FullscreenSigninCoordinator.Delegate}. */
    @Override
    public boolean shouldDisplayManagementNoticeOnManagedDevices() {
        // Management notice shouldn't be shown in the Upgrade promo flow, even on managed devices.
        return false;
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public boolean shouldDisplayFooterText() {
        return false;
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void dismissHistorySync() {
        mViewHolder.removeAllViews();
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
        mDelegate.onFlowComplete();
    }

    /**
     * Removes existing views from the view switcher and re-inflates them with the correct layout
     * after a configuration change.
     */
    public void recreateLayoutAfterConfigurationChange() {
        mViewHolder.removeAllViews();
        inflateViewBundle();
        showChildView(mCurrentView);
    }

    public void onAccountSelected(String accountName) {
        mSigninCoordinator.onAccountSelected(accountName);
    }

    public void handleBackPress() {
        switch (mCurrentView) {
            case ChildView.SIGNIN:
                if (isSignedIn()) {
                    SigninManager signinManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(mProfileSupplier.get().getOriginalProfile());
                    signinManager.signOut(SignoutReason.ABORT_SIGNIN);
                }
                mDelegate.onFlowComplete();
                break;
            case ChildView.HISTORY_SYNC:
                if (!mDidShowSignin) {
                    mDelegate.onFlowComplete();
                    return;
                }
                showChildView(ChildView.SIGNIN);
                mSigninCoordinator.reset();
        }
    }

    private void inflateViewBundle() {
        boolean useLandscapeLayout = SigninUtils.shouldShowDualPanesHorizontalLayout(mActivity);
        ViewGroup viewBundle =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        useLandscapeLayout
                                                ? R.layout.upgrade_promo_landscape_view
                                                : R.layout.upgrade_promo_portrait_view,
                                        null);
        mFullscreenSigninView = viewBundle.findViewById(R.id.fullscreen_signin);
        mHistorySyncView = viewBundle.findViewById(R.id.history_sync);
        mViewHolder.setId(viewBundle.getId());
        // Remove all child views from the bundle so that they can be added to mViewHolder later.
        viewBundle.removeAllViews();
    }

    private boolean isSignedIn() {
        IdentityManager identityManager =
                IdentityServicesProvider.get()
                        .getIdentityManager(mProfileSupplier.get().getOriginalProfile());
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
    }

    private void showChildView(@ChildView int child) {
        mCurrentView = child;
        mViewHolder.removeAllViews();
        mViewHolder.addView(getCurrentChildView());
        switch (child) {
            case ChildView.SIGNIN:
                mSigninCoordinator =
                        new FullscreenSigninCoordinator(
                                mActivity,
                                mModalDialogManager,
                                this,
                                mPrivacyPreferencesManager,
                                SigninAccessPoint.SIGNIN_PROMO);
                mSigninCoordinator.setView((FullscreenSigninView) getCurrentChildView());
                if (mHistorySyncCoordinator != null) {
                    mHistorySyncCoordinator.destroy();
                    mHistorySyncCoordinator = null;
                }
                break;
            case ChildView.HISTORY_SYNC:
                maybeCreateHistorySyncCoordinator();
                mHistorySyncCoordinator.setView(
                        (HistorySyncView) mHistorySyncView,
                        SigninUtils.shouldShowDualPanesHorizontalLayout(mActivity));

                if (mSigninCoordinator != null) {
                    mSigninCoordinator.destroy();
                    mSigninCoordinator = null;
                }
                break;
        }
    }

    private View getCurrentChildView() {
        return switch (mCurrentView) {
            case ChildView.SIGNIN -> mFullscreenSigninView;
            case ChildView.HISTORY_SYNC -> mHistorySyncView;
            default -> throw new IllegalStateException(mCurrentView + " view index doesn't exist");
        };
    }

    private void maybeCreateHistorySyncCoordinator() {
        if (mHistorySyncCoordinator != null) {
            return;
        }

        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        mActivity,
                        this,
                        mProfileSupplier.get().getOriginalProfile(),
                        SigninAccessPoint.SIGNIN_PROMO,
                        /* showEmailInFooter= */ isSignedIn(),
                        /* shouldSignOutOnDecline= */ false,
                        null);
    }
}
