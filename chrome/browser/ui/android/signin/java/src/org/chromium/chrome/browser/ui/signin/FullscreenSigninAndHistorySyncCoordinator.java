// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.accounts.Account;
import android.app.Activity;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;

import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninMediator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninView;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncView;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Parent coordinator for the re-FRE promo */
@NullMarked
public final class FullscreenSigninAndHistorySyncCoordinator
        implements SigninAndHistorySyncCoordinator,
                HistorySyncCoordinator.HistorySyncDelegate,
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
        Promise<@Nullable Void> getNativeInitializationPromise();

        void onFlowComplete(SigninAndHistorySyncCoordinator.Result result);
    }

    /**
     * The view switcher child UIs and their order. The order matches the order of the children
     * views in fullscreen_signin_and_history_sync_portrait/landscape_view.xml
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

    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private final OneshotSupplier<ProfileProvider> mProfileSupplier;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final FullscreenSigninAndHistorySyncConfig mConfig;
    private final @SigninAccessPoint int mSigninAccessPoint;
    private final Delegate mDelegate;
    private final boolean mDidShowSignin;
    private final long mActivityStartTime;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;
    private final FrameLayout mViewHolder;
    private @ChildView int mCurrentView;
    private FullscreenSigninView mFullscreenSigninView;
    private View mHistorySyncView;
    private @Nullable FullscreenSigninCoordinator mSigninCoordinator;
    private @Nullable HistorySyncCoordinator mHistorySyncCoordinator;

    public FullscreenSigninAndHistorySyncCoordinator(
            WindowAndroid windowAndroid,
            Activity activity,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileSupplier,
            PrivacyPreferencesManager privacyPreferencesManager,
            FullscreenSigninAndHistorySyncConfig config,
            @SigninAccessPoint int signinAccessPoint,
            Delegate delegate,
            long activityStartTime,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mCurrentView = ChildView.SIGNIN;
        mViewHolder = new FrameLayout(activity);
        mViewHolder.setBackgroundColor(SemanticColorUtils.getDefaultBgColor(mActivity));
        mModalDialogManager = modalDialogManager;
        mProfileSupplier = profileSupplier;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mConfig = config;
        mSigninAccessPoint = signinAccessPoint;
        mDelegate = delegate;
        mActivityStartTime = activityStartTime;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
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
                            mConfig.signinConfig,
                            mSigninAccessPoint);
            mViewHolder.addView(getCurrentChildView());
            mSigninCoordinator.setView((FullscreenSigninView) getCurrentChildView());
            // TODO(crbug.com/347657449): Record other AccountConsistencyPromoActions.
            SigninMetricsUtils.logAccountConsistencyPromoAction(
                    AccountConsistencyPromoAction.SHOWN, mSigninAccessPoint);
            mDidShowSignin = true;
        }
    }

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
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

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
    public void onAddAccountCanceled() {}

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
    public void onAccountAdded(String accountName) {
        assertNonNull(mSigninCoordinator);
        mSigninCoordinator.onAccountAdded(accountName);
    }

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
    public View getView() {
        return mViewHolder;
    }

    /**
     * Implements {@link SigninAndHistorySyncCoordinator}. Removes existing views from the view
     * switcher and re-inflates them with the correct layout after a configuration change.
     */
    @Override
    public void onConfigurationChange() {
        mViewHolder.removeAllViews();
        inflateViewBundle();
        showChildView(mCurrentView);
    }

    /** Implements {@link SigninAndHistorySyncCoordinator}. */
    @Override
    public @BackPressResult int handleBackPress() {
        switch (mCurrentView) {
            case ChildView.SIGNIN:
                if (isSignedIn()) {
                    Profile profile = assumeNonNull(mProfileSupplier.get()).getOriginalProfile();
                    SigninManager signinManager =
                            IdentityServicesProvider.get().getSigninManager(profile);
                    assumeNonNull(signinManager);
                    signinManager.signOut(SignoutReason.ABORT_SIGNIN);
                }
                mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());
                break;
            case ChildView.HISTORY_SYNC:
                if (!mDidShowSignin) {
                    mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());
                    return BackPressResult.SUCCESS;
                }
                showChildView(ChildView.SIGNIN);
                assumeNonNull(mSigninCoordinator);
                mSigninCoordinator.reset();
        }
        return BackPressResult.SUCCESS;
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
            mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());
            return;
        }
        Profile profile = assumeNonNull(mProfileSupplier.get()).getOriginalProfile();
        if (!SigninAndHistorySyncCoordinator.shouldShowHistorySync(
                profile, mConfig.historyOptInMode)) {
            HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(profile);
            historySyncHelper.recordHistorySyncNotShown(mSigninAccessPoint);
            // TODO(crbug.com/376469696): Differentiate the failure & completion case here.
            mDelegate.onFlowComplete(
                    new SigninAndHistorySyncCoordinator.Result(mDidShowSignin, false));
            return;
        }
        showChildView(ChildView.HISTORY_SYNC);
    }

    @Override
    public void displayDeviceLockPage(Account selectedAccount) {
        String accountName = selectedAccount == null ? null : selectedAccount.name;
        mDeviceLockActivityLauncher.launchDeviceLockActivity(
                mActivity,
                accountName,
                /* requireDeviceLockReauthentication= */ true,
                mWindowAndroid,
                (resultCode, data) -> {
                    if (resultCode == Activity.RESULT_OK && mSigninCoordinator != null) {
                        mSigninCoordinator.continueSignIn();
                    }
                },
                DeviceLockActivityLauncher.Source.FULLSCREEN_SIGNIN);
    }

    @Override
    public void recordUserSignInHistograms(@AccountConsistencyPromoAction int promoAction) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction, mSigninAccessPoint);
    }

    @Override
    public void recordSigninDismissedHistograms() {
        SigninMetricsUtils.logAccountConsistencyPromoAction(
                AccountConsistencyPromoAction.DISMISSED_BUTTON, mSigninAccessPoint);
    }

    @Override
    public void recordLoadCompletedHistograms(
            @FullscreenSigninMediator.LoadPoint int slowestLoadPoint) {
        RecordHistogram.recordTimesHistogram(
                "Signin.Timestamps.Android.Fullscreen.LoadCompleted",
                SystemClock.elapsedRealtime() - mActivityStartTime);
    }

    @Override
    public void recordNativeInitializedHistogram() {
        RecordHistogram.recordTimesHistogram(
                "Signin.Timestamps.Android.Fullscreen.NativeInitialized",
                SystemClock.elapsedRealtime() - mActivityStartTime);
    }

    /** Implements {@link FullscreenSigninCoordinator.Delegate} */
    @Override
    public OneshotSupplier<ProfileProvider> getProfileSupplier() {
        return mProfileSupplier;
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

    @Override
    public OneshotSupplier<Boolean> getPolicyLoadListener() {
        return mDelegate.getPolicyLoadListener();
    }

    @Override
    public OneshotSupplier<Boolean> getChildAccountStatusSupplier() {
        return mDelegate.getChildAccountStatusSupplier();
    }

    @Override
    public Promise<@Nullable Void> getNativeInitializationPromise() {
        return mDelegate.getNativeInitializationPromise();
    }

    /** Implements {@link HistorySyncCoordinator.HistorySyncDelegate} */
    @Override
    public void dismissHistorySync(boolean didSignOut, boolean isHistorySyncAccepted) {
        mViewHolder.removeAllViews();
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
        SigninAndHistorySyncCoordinator.Result flowResult =
                new SigninAndHistorySyncCoordinator.Result(
                        mDidShowSignin && !didSignOut, isHistorySyncAccepted);
        mDelegate.onFlowComplete(flowResult);
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void recordHistorySyncOptIn(
            @SigninAccessPoint int accessPoint, boolean isHistorySyncAccepted) {
        if (isHistorySyncAccepted) {
            SigninMetricsUtils.logHistorySyncAcceptButtonClicked(accessPoint);
        } else {
            SigninMetricsUtils.logHistorySyncDeclineButtonClicked(accessPoint);
        }
    }

    @EnsuresNonNull({"mFullscreenSigninView", "mHistorySyncView"})
    private void inflateViewBundle() {
        boolean useLandscapeLayout = SigninUtils.shouldShowDualPanesHorizontalLayout(mActivity);
        ViewGroup viewBundle =
                (ViewGroup)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        useLandscapeLayout
                                                ? R.layout
                                                        .fullscreen_signin_and_history_sync_landscape_view
                                                : R.layout
                                                        .fullscreen_signin_and_history_sync_portrait_view,
                                        null);
        mFullscreenSigninView = viewBundle.findViewById(R.id.fullscreen_signin);
        mHistorySyncView = viewBundle.findViewById(R.id.history_sync);
        mViewHolder.setId(viewBundle.getId());
        // Remove all child views from the bundle so that they can be added to mViewHolder later.
        viewBundle.removeAllViews();
    }

    private boolean isSignedIn() {
        Profile profile = assumeNonNull(mProfileSupplier.get()).getOriginalProfile();
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(identityManager);
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
                                mConfig.signinConfig,
                                mSigninAccessPoint);
                mSigninCoordinator.setView((FullscreenSigninView) getCurrentChildView());
                if (mHistorySyncCoordinator != null) {
                    mHistorySyncCoordinator.destroy();
                    mHistorySyncCoordinator = null;
                }
                break;
            case ChildView.HISTORY_SYNC:
                maybeCreateHistorySyncCoordinator();
                assumeNonNull(mHistorySyncCoordinator);
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

        boolean shouldSignOutOnDecline =
                mDidShowSignin && mConfig.historyOptInMode == HistorySyncConfig.OptInMode.REQUIRED;
        Profile profile = assumeNonNull(mProfileSupplier.get()).getOriginalProfile();
        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        mActivity,
                        this,
                        profile,
                        mConfig.historySyncConfig,
                        mSigninAccessPoint,
                        /* showEmailInFooter= */ !mDidShowSignin,
                        /* shouldSignOutOnDecline= */ shouldSignOutOnDecline,
                        null);
    }
}
