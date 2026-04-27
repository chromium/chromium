// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.Event;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.ForcedSigninController;
import org.chromium.chrome.browser.ui.signin.ForcedSigninStatusProvider;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator.Delegate;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

@NullMarked
public class FullscreenSigninMediator
        implements AccountsChangeObserver,
                ProfileDataCache.Observer,
                AccountPickerCoordinator.Listener,
                UMADialogCoordinator.Listener {
    private static final String TAG = "SigninFRMediator";

    private static final int SIGNIN_ANIMATION_DELAY_MS = 600;
    private static boolean sAnimationsEnabled = true;

    // LINT.IfChange(LoadPoint)
    /** Used for MobileFre.SlowestLoadPoint histogram. Should be treated as append-only. */
    @VisibleForTesting
    @IntDef({
        LoadPoint.NATIVE_INITIALIZATION,
        LoadPoint.POLICY_LOAD,
        LoadPoint.CHILD_STATUS_LOAD,
        LoadPoint.ACCOUNT_FETCHING,
        LoadPoint.MAX
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadPoint {
        int NATIVE_INITIALIZATION = 0;
        int POLICY_LOAD = 1;
        int CHILD_STATUS_LOAD = 2;
        int ACCOUNT_FETCHING = 3;
        int MAX = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/mobile/enums.xml:LoadPoint)

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private @MonotonicNonNull SigninManager mSigninManager;
    private @MonotonicNonNull ForcedSigninStatusProvider mForcedSigninStatusProvider;
    private final Delegate mDelegate;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final @SigninAccessPoint int mAccessPoint;
    private final FullscreenSigninConfig mConfig;
    private final PropertyModel mModel;

    // Two separate caches are necessary because scaling the large (110dp) animation image down
    // to the small (40dp) button size would make the child account badge unreadable.
    private @Nullable ProfileDataCache mContinueButtonProfileDataCache;
    private @Nullable ProfileDataCache mSigninAnimationProfileDataCache;
    private boolean mDestroyed;

    /** Whether the initial load phase has been completed. See {@link #onInitialLoadCompleted}. */
    private boolean mInitialLoadCompleted;

    private @Nullable AccountPickerDialogCoordinator mDialogCoordinator;
    private @Nullable CoreAccountInfo mSelectedAccount;
    private @Nullable CoreAccountInfo mDefaultAccount;
    private @Nullable CoreAccountInfo mAddedAccount;
    // This field is used to save the added account email while the account info becomes available
    // in AccountManagerFacade for sign-in.
    private @Nullable String mPendingAddedAccountEmail;
    private boolean mAllowMetricsAndCrashUploading;
    private boolean mIsSigninSupported;
    private boolean mIsSigninForcedByPolicy;
    private boolean mIsChild;
    private int mForcedSigninToken = TokenHolder.INVALID_TOKEN;

    FullscreenSigninMediator(
            Context context,
            ModalDialogManager modalDialogManager,
            Delegate delegate,
            PrivacyPreferencesManager privacyPreferencesManager,
            FullscreenSigninConfig config,
            @SigninAccessPoint int accessPoint) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mDelegate = delegate;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mAccessPoint = accessPoint;
        mConfig = config;

        mInitialLoadCompleted =
                mDelegate.getNativeInitializationPromise().isFulfilled()
                        && mAccountManagerFacade.getAccounts().isFulfilled()
                        && mDelegate.getChildAccountStatusSupplier().get() != null
                        && mDelegate.getPolicyLoadListener().get() != null;
        mModel =
                FullscreenSigninProperties.createModel(
                        this::onSelectedAccountClicked,
                        this::onContinueAsClicked,
                        this::onDismissClicked,
                        ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                                && !mConfig.shouldDisableSignin,
                        mConfig.logoId,
                        mContext.getString(R.string.fre_welcome),
                        mConfig.subtitle,
                        mConfig.dismissText,
                        /* showInitialLoadProgressSpinner= */ !mInitialLoadCompleted);

        if (mInitialLoadCompleted) {
            onInitialLoadCompleted(assertNonNull(mDelegate.getPolicyLoadListener().get()));
        } else {
            mDelegate
                    .getNativeInitializationPromise()
                    .then(
                            result -> {
                                onNativeLoaded();
                            });
            mDelegate.getPolicyLoadListener().onAvailable(hasPolicies -> onPolicyLoad());
            mDelegate
                    .getChildAccountStatusSupplier()
                    .onAvailable(ignored -> onChildAccountStatusAvailable());
        }

        mAccountManagerFacade.addObserver(this);
        updateAccounts(
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()));
        SigninMetricsUtils.logSigninStarted(accessPoint);
    }

    PropertyModel getModel() {
        return mModel;
    }

    Drawable getProfilePictureForTesting() {
        return mModel.get(FullscreenSigninProperties.PROFILE_PICTURE);
    }

    void setStartAnimationForTesting(boolean start) {
        mModel.set(FullscreenSigninProperties.START_ANIMATION, start);
    }

    @Nullable BadgeConfig getSigninAnimationBadgeConfigForTesting() {
        if (mSigninAnimationProfileDataCache == null || mSelectedAccount == null) return null;
        return mSigninAnimationProfileDataCache.getBadgeConfigForTesting( // IN-TEST
                mSelectedAccount.getId());
    }

    @Nullable BadgeConfig getContinueButtonBadgeConfigForTesting() {
        if (mContinueButtonProfileDataCache == null || mSelectedAccount == null) return null;
        return mContinueButtonProfileDataCache.getBadgeConfigForTesting( // IN-TEST
                mSelectedAccount.getId());
    }

    void destroy() {
        assert !mDestroyed;
        if (mContinueButtonProfileDataCache != null) {
            mContinueButtonProfileDataCache.removeObserver(this);
        }
        if (mSigninAnimationProfileDataCache != null) {
            mSigninAnimationProfileDataCache.removeObserver(this);
        }
        if (mForcedSigninStatusProvider != null) {
            mForcedSigninStatusProvider.hideForcedSigninScreen(mForcedSigninToken);
            mForcedSigninToken = TokenHolder.INVALID_TOKEN;
        }
        mAccountManagerFacade.removeObserver(this);
        mDestroyed = true;
    }

    void reset() {
        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, false);
        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
    }

    private void onNativeLoaded() {
        // This happens asynchronously, so this check is necessary to ensure we don't interact with
        // the delegate after the mediator is destroyed. See https://crbug.com/40214143.
        if (mDestroyed) return;

        mDelegate.recordNativeInitializedHistogram();
        checkWhetherInitialLoadCompleted(LoadPoint.NATIVE_INITIALIZATION);
    }

    private void onChildAccountStatusAvailable() {
        checkWhetherInitialLoadCompleted(LoadPoint.CHILD_STATUS_LOAD);
    }

    private void onPolicyLoad() {
        checkWhetherInitialLoadCompleted(LoadPoint.POLICY_LOAD);
    }

    /** Checks the initial load status. See {@link #onInitialLoadCompleted} for details. */
    private void checkWhetherInitialLoadCompleted(@LoadPoint int slowestLoadPoint) {
        // This happens asynchronously, so this check is necessary to ensure we don't interact with
        // the delegate after the mediator is destroyed. See https://crbug.com/40214143.
        if (mDestroyed) return;

        // The initialization flow requires native to be ready before the initial loading spinner
        // can be hidden.
        if (!mDelegate.getNativeInitializationPromise().isFulfilled()) return;

        // We need the account fetching to be complete before we can hide the initial loading
        // spinner.
        if (!mAccountManagerFacade.getAccounts().isFulfilled()) return;

        if (mDelegate.getChildAccountStatusSupplier().get() != null
                && mDelegate.getPolicyLoadListener().get() != null
                && !mInitialLoadCompleted) {
            mInitialLoadCompleted = true;
            onInitialLoadCompleted(mDelegate.getPolicyLoadListener().get());
            // TODO(crbug.com/40235150): Rename this method and the corresponding histogram.
            mDelegate.recordLoadCompletedHistograms(slowestLoadPoint);
        }
    }

    /**
     * Called when the initial load phase is completed.
     *
     * <p>After creation, {@link FullscreenSigninView} displays a loading spinner that is shown
     * until policies and the child account status are being checked. If needed, that phase also
     * waits for the native to be loaded (for example, if any app restrictions are detected). This
     * method is invoked when this initial waiting phase is over and the "Continue" button can be
     * displayed. It checks policies and child accounts to decide which version of the UI to
     * display.
     *
     * @param hasPolicies Whether any enterprise policies have been found on the device. 'true' here
     *     also means that native has been initialized.
     */
    void onInitialLoadCompleted(boolean hasPolicies) {
        if (mDestroyed) return;

        Log.i(TAG, "#onInitialLoadCompleted() hasPolicies:" + hasPolicies);
        Profile profile = assumeNonNull(mDelegate.getProfileSupplier().get()).getOriginalProfile();
        mSigninManager = assertNonNull(IdentityServicesProvider.get().getSigninManager(profile));
        mForcedSigninStatusProvider = ForcedSigninStatusProvider.getForProfile(profile);
        initializeProfileDataCache(profile);

        // 1. Update all fields.
        mIsSigninSupported = isSigninSupported(profile);
        if (hasPolicies) {
            mAllowMetricsAndCrashUploading =
                    mPrivacyPreferencesManager.isUsageAndCrashReportingPermittedByPolicy();
            mIsSigninForcedByPolicy = ForcedSigninController.isForcedSigninPolicyEnabled();
            mForcedSigninToken =
                    mIsSigninForcedByPolicy
                            ? mForcedSigninStatusProvider.showForcedSigninScreen()
                            : TokenHolder.INVALID_TOKEN;
        } else {
            mAllowMetricsAndCrashUploading = true;
            mIsSigninForcedByPolicy = false;
        }

        // 2. Update the property model.
        mModel.set(FullscreenSigninProperties.IS_SIGNIN_SUPPORTED, mIsSigninSupported);
        mModel.set(
                FullscreenSigninProperties.SHOW_ENTERPRISE_MANAGEMENT_NOTICE,
                hasPolicies && mDelegate.shouldDisplayManagementNoticeOnManagedDevices());

        updateShouldHideDismissButton();

        mModel.set(FullscreenSigninProperties.TITLE_STRING, getTitleText());
        mModel.set(
                FullscreenSigninProperties.SUBTITLE_STRING, getSubtitleText(profile, hasPolicies));

        mModel.set(
                FullscreenSigninProperties.FOOTER_STRING,
                getFooterString(!mAllowMetricsAndCrashUploading));
        mModel.set(FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER, false);

        // Apply supervised account properties
        AccountUtils.checkIsSubjectToParentalControls(
                mAccountManagerFacade,
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()),
                this::onChildAccountStatusReady);
    }

    private void initializeProfileDataCache(Profile profile) {
        assert mContinueButtonProfileDataCache == null;
        assert mSigninAnimationProfileDataCache == null;
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        mContinueButtonProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        mContext, assertNonNull(identityManager));
        mContinueButtonProfileDataCache.addObserver(this);
        // Create a separate cache for the sign-in animation. We use the logo height (110dp)
        // to ensure the profile picture is high-resolution when it replaces the Chrome logo.
        // A separate cache is used because scaling a 110dp image back down to 40dp for the
        // "Continue" button would make the child account badge nearly invisible.
        mSigninAnimationProfileDataCache =
                ProfileDataCache.createWithoutBadge(
                        mContext, identityManager, R.dimen.fullscreen_signin_logo_default_height);
        mSigninAnimationProfileDataCache.addObserver(this);
        updateSelectedAccountData();
    }

    private boolean isSigninSupported(Profile profile) {
        return ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                && UserPrefs.get(profile).getBoolean(Pref.SIGNIN_ALLOWED)
                && !mConfig.shouldDisableSignin;
    }

    private String getTitleText() {
        if (mIsSigninForcedByPolicy) {
            return mContext.getString(R.string.signin_fre_title_signin_forced_by_policy);
        }
        if (!mIsSigninSupported) {
            return mContext.getString(R.string.fre_welcome);
        }
        return mConfig.title;
    }

    private @Nullable String getSubtitleText(Profile profile, boolean hasPolicies) {
        if (!mIsSigninSupported || mIsChild) {
            return null;
        }
        if (mIsSigninForcedByPolicy) {
            return mContext.getString(R.string.signin_fre_subtitle_signin_forced_by_policy);
        } else if (hasPolicies && mDelegate.shouldDisplayManagementNoticeOnManagedDevices()) {
            return null;
        }

        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assumeNonNull(syncService);
        boolean isSyncDataManaged = false;
        for (int typeId = UserSelectableType.FIRST_TYPE;
                typeId <= UserSelectableType.LAST_TYPE;
                typeId++) {
            if (syncService.isTypeManagedByPolicy(typeId)) {
                isSyncDataManaged = true;
                break;
            }
        }
        return isSyncDataManaged
                ? mContext.getString(R.string.signin_fre_subtitle_without_sync)
                : mConfig.subtitle;
    }

    private void updateShouldHideDismissButton() {
        mModel.set(
                FullscreenSigninProperties.SHOULD_HIDE_DISMISS_BUTTON,
                mIsChild || mIsSigninForcedByPolicy || !mIsSigninSupported);
    }

    void onAccountAdded(String accountEmail) {
        var accounts =
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts());
        mAddedAccount = AccountUtils.findAccountByEmail(accounts, accountEmail);
        if (mAddedAccount == null) {
            mPendingAddedAccountEmail = accountEmail;
            return;
        }

        setSelectedAccount(mAddedAccount);
        if (mDialogCoordinator != null) mDialogCoordinator.dismissDialog();
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        if (mSelectedAccount != null
                && TextUtils.equals(mSelectedAccount.getEmail(), profileData.getAccountEmail())) {
            updateSelectedAccountData();
        }
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onCoreAccountInfosChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
        checkWhetherInitialLoadCompleted(LoadPoint.ACCOUNT_FETCHING);
    }

    @Override
    public void onAccountSelected(CoreAccountInfo account) {
        if (mPendingAddedAccountEmail != null) {
            // If another account is selected before the added account is available in account
            // manager facade then clear the pending added account email so that it doesn't get
            // selected automatically in #updateAccounts().
            mPendingAddedAccountEmail = null;
        }
        setSelectedAccount(account);
        if (mDialogCoordinator != null) mDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
        mDelegate.addAccount();
    }

    /** Implements {@link UMADialogCoordinator.Listener} */
    @Override
    public void onAllowMetricsAndCrashUploadingChecked(boolean allowMetricsAndCrashUploading) {
        mAllowMetricsAndCrashUploading = allowMetricsAndCrashUploading;
    }

    private void openUmaDialog() {
        new UMADialogCoordinator(
                mContext, mModalDialogManager, this, mAllowMetricsAndCrashUploading);
    }

    /**
     * Callback for the PropertyKey {@link FullscreenSigninProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        if (isContinueOrDismissClicked()) return;

        IdentityManager identityManager =
                IdentityServicesProvider.get()
                        .getIdentityManager(
                                assumeNonNull(mDelegate.getProfileSupplier().get())
                                        .getOriginalProfile());
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(
                        mContext, this, mModalDialogManager, assertNonNull(identityManager));
    }

    /** Callback for the PropertyKey {@link FullscreenSigninProperties#ON_CONTINUE_AS_CLICKED}. */
    private void onContinueAsClicked() {
        assert mDelegate.getNativeInitializationPromise().isFulfilled();
        if (isContinueOrDismissClicked()) return;
        assert !mModel.get(FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER)
                : "The continue button shouldn't be visible while the load spinner is shown!";

        if (!mModel.get(FullscreenSigninProperties.IS_SIGNIN_SUPPORTED)) {
            mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
            mDelegate.advanceToNextPage();
            return;
        }
        if (mSelectedAccount == null) {
            mDelegate.addAccount();
            return;
        }

        if (DeviceInfo.isAutomotive()) {
            mDelegate.displayDeviceLockPage(mSelectedAccount.getId());
            return;
        }
        proceedWithSignIn();
    }

    /** Accepts ToS and completes the account sign-in with the selected account. */
    void proceedWithSignIn() {
        // This is needed to get metrics/crash reports from the sign-in flow itself.
        mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
        mDelegate.recordUserSignInHistograms(getSigninPromoAction());
        SigninFlowTimestampsLogger signinTimestampsLogger =
                SigninFlowTimestampsLogger.startLogging(FlowVariant.FULLSCREEN);
        // If the user signs into an account on the FRE, goes to the next page and presses
        // back to come back to the welcome screen, then there will already be an account signed in.
        @Nullable CoreAccountInfo signedInAccount = getSignedInAccount();
        if (signedInAccount != null && Objects.equals(signedInAccount, mSelectedAccount)) {
            mDelegate.advanceToNextPage();
        }

        if (mSelectedAccount != null) {
            mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, true);
            if (FeatureList.isNativeInitialized()
                    && ChromeFeatureList.isEnabled(ChromeFeatureList.XPLAT_SYNCED_SETUP)) {
                startSignInAnimation(signinTimestampsLogger);
            } else {
                // Proceed to the next step without waiting for animation.
                finishSignIn(signinTimestampsLogger);
            }
        }
    }

    /**
     * Starts the sign-in animation.
     *
     * @param signinTimestampsLogger a logger for signin flow events.
     */
    private void startSignInAnimation(SigninFlowTimestampsLogger signinTimestampsLogger) {
        DisplayableProfileData profileData =
                mModel.get(FullscreenSigninProperties.BOTTOM_GROUP_ACCOUNT_DATA);
        mModel.set(
                FullscreenSigninProperties.TITLE_STRING,
                String.format(
                        mContext.getString(R.string.signed_in_fre_title),
                        profileData.getGivenNameOrFullNameOrEmail()));
        mModel.set(
                FullscreenSigninProperties.PROFILE_PICTURE,
                // If the signin animation is starting, an account should already have been selected
                // and the sign-in animation profile picture data cache should have been
                // initialized.
                assumeNonNull(mSigninAnimationProfileDataCache)
                        .getById(assumeNonNull(mSelectedAccount).getId())
                        .getImage());
        if (sAnimationsEnabled) {
            mModel.set(
                    FullscreenSigninProperties.ANIMATOR_LISTENER,
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            // To give users more time to read the welcome text, we will wait for a
                            // delay time (according to UX request).
                            ThreadUtils.postOnUiThreadDelayed(
                                    () -> finishSignIn(signinTimestampsLogger),
                                    SIGNIN_ANIMATION_DELAY_MS);
                        }

                        @Override
                        public void onAnimationCancel(Animator animation) {
                            getSigninCallback(signinTimestampsLogger).onSignInAborted();
                        }
                    });
            // PROFILE_PICTURE is expected to be set before starting the animation.
            mModel.set(FullscreenSigninProperties.START_ANIMATION, true);
        } else {
            finishSignIn(signinTimestampsLogger);
        }
    }

    /**
     * Finishes the sign-in flow, either by signing out and then in with the selected account, or by
     * checking for account management and signing in with the selected account.
     *
     * @param signinTimestampsLogger a logger for signin flow events.
     */
    private void finishSignIn(SigninFlowTimestampsLogger signinTimestampsLogger) {
        if (mDestroyed) return;
        @Nullable CoreAccountInfo signedInAccount = getSignedInAccount();
        final SignInCallback signInCallback = getSigninCallback(signinTimestampsLogger);
        final @SigninAccessPoint int accessPoint =
                mModel.get(FullscreenSigninProperties.SHOULD_HIDE_DISMISS_BUTTON)
                        ? SigninAccessPoint.FORCED_SIGNIN
                        : mAccessPoint;
        assumeNonNull(mSelectedAccount);
        if (signedInAccount != null) {
            // If there already exists another signed-in account, first
            // sign-out and then sign-in with the selected account.
            signOutThenSignInWithSelectedAccount(
                    mSelectedAccount, accessPoint, signinTimestampsLogger, signInCallback);
        } else {
            FreManagementNoticeDialogHelper.checkAccountManagementAndSignIn(
                    mSelectedAccount,
                    assertNonNull(mSigninManager),
                    signinTimestampsLogger,
                    accessPoint,
                    signInCallback,
                    mContext,
                    mModalDialogManager);
        }
    }

    /**
     * @param signinTimestampsLogger a logger for signin flow events.
     * @return a {@link SignInCallback} that reacts to sign-in being completed or aborted.
     */
    private SignInCallback getSigninCallback(SigninFlowTimestampsLogger signinTimestampsLogger) {
        return new SignInCallback() {
            @Override
            public void onSignInComplete() {
                if (mDestroyed) return;
                signinTimestampsLogger.recordTimestamp(Event.SIGNIN_COMPLETED);
                if (mConfig.signinSurveyType != null) {
                    SigninSurveyController.registerTrigger(
                            assertNonNull(getProfile()), mConfig.signinSurveyType);
                }
                mDelegate.advanceToNextPage();
            }

            @Override
            public void onSignInAborted() {
                if (mDestroyed) return;
                signinTimestampsLogger.recordTimestamp(Event.SIGNIN_ABORTED);
                // TODO(crbug.com/40790332): For now we enable the buttons again to not
                // block the users from continuing to the next page. Should show a dialog
                // with the signin error.
                mModel.set(
                        FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, false);
                mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
                mModel.set(FullscreenSigninProperties.LOGO_DRAWABLE_ID, mConfig.logoId);
                mModel.set(FullscreenSigninProperties.SHOW_ANIMATION, mConfig.logoId == 0);
                mModel.set(FullscreenSigninProperties.TITLE_STRING, mConfig.title);
            }
        };
    }

    /**
     * Signs out from the current account and then signs in with the selected account.
     *
     * @param selectedAccount The account to sign in with.
     * @param accessPoint The signin access point.
     * @param signinTimestampsLogger a logger for signin flow events.
     * @param signInCallback The callback to be called after sign-in completes or aborts.
     */
    private void signOutThenSignInWithSelectedAccount(
            CoreAccountInfo selectedAccount,
            @SigninAccessPoint int accessPoint,
            SigninFlowTimestampsLogger signinTimestampsLogger,
            @Nullable SignInCallback signInCallback) {
        Runnable signOutCallback =
                () -> {
                    if (mDestroyed) return;
                    FreManagementNoticeDialogHelper.checkAccountManagementAndSignIn(
                            selectedAccount,
                            assertNonNull(mSigninManager),
                            signinTimestampsLogger,
                            accessPoint,
                            signInCallback,
                            mContext,
                            mModalDialogManager);
                };
        assumeNonNull(mSigninManager)
                .signOut(
                        SignoutReason.ABORT_SIGNIN,
                        signOutCallback,
                        /* forceWipeUserData= */ false);
    }

    /**
     * @return The {@link Profile} of the user, or null if it's not available yet.
     */
    private @Nullable Profile getProfile() {
        @Nullable ProfileProvider profileProvider = mDelegate.getProfileSupplier().get();
        if (profileProvider == null) return null;
        return profileProvider.getOriginalProfile();
    }

    /**
     * @return The signed-in {@link CoreAccountInfo} of the user, or null if the user isn't signed
     *     in or the profile is not available yet.
     */
    private @Nullable CoreAccountInfo getSignedInAccount() {
        Profile profile = getProfile();
        if (profile == null) return null;
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) return null;
        return identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
    }

    private @AccountConsistencyPromoAction int getSigninPromoAction() {
        assert mSelectedAccount != null;
        if (Objects.equals(mSelectedAccount, mDefaultAccount)) {
            return AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT;
        } else if (Objects.equals(mSelectedAccount, mAddedAccount)) {
            return AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT;
        }
        return AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT;
    }

    /** Callback for the PropertyKey {@link FullscreenSigninProperties#ON_DISMISS_CLICKED}. */
    private void onDismissClicked() {
        if (isContinueOrDismissClicked()) return;
        assert !mModel.get(FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER)
                : "The dismiss button shouldn't be visible while the load spinner is shown!";

        assert mDelegate.getNativeInitializationPromise().isFulfilled();

        dismiss();
    }

    /** Dismisses the sign-in page and continues without a signed-in account. */
    void dismiss() {
        mDelegate.recordSigninDismissedHistograms();
        mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
        SigninPreferencesManager.getInstance().temporarilySuppressNewTabPagePromos();
        Profile profile = assumeNonNull(mDelegate.getProfileSupplier().get()).getOriginalProfile();
        if (assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile))
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER, true);
            Runnable signOutCallback =
                    () -> {
                        if (mDestroyed) {
                            // FirstRunActivity was destroyed while we were waiting for the
                            // sign-out.
                            return;
                        }

                        mDelegate.advanceToNextPage();
                    };
            assumeNonNull(mSigninManager)
                    .signOut(
                            SignoutReason.ABORT_SIGNIN,
                            signOutCallback,
                            /* forceWipeUserData= */ false);
        } else {
            mDelegate.advanceToNextPage();
        }
    }

    /**
     * Returns whether the user has already clicked either 'Continue' or 'Dismiss'. If the user has
     * pressed either of the two buttons consecutive taps are ignored. See crbug.com/40214140 for
     * details.
     */
    boolean isContinueOrDismissClicked() {
        // These property keys are set when continue or dismiss button is clicked respectively.
        return mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT)
                || mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER);
    }

    private void setSelectedAccount(CoreAccountInfo account) {
        mSelectedAccount = account;
        updateSelectedAccountData();
    }

    private void updateSelectedAccountData() {
        if (mSelectedAccount == null) return;

        if (mContinueButtonProfileDataCache != null) {
            // Both caches are initialized together so they're either both null or none of them are.
            assert mSigninAnimationProfileDataCache != null;

            mModel.set(
                    FullscreenSigninProperties.BOTTOM_GROUP_ACCOUNT_DATA,
                    mContinueButtonProfileDataCache.getById(mSelectedAccount.getId()));
            mModel.set(FullscreenSigninProperties.ENABLE_ACCOUNT_SELECTION, !mIsChild);

            // Until real data arrives, PROFILE_PICTURE is a placeholder silhouette.
            // When the sign-in animation starts, it sets the PROFILE_PICTURE and then sets
            // START_ANIMATION to true in the model.
            // We perform the check below because if START_ANIMATION is false, we are still
            // displaying the Chrome icon. In that scenario, the arrival of the profile picture data
            // should not cause the Chrome icon to be replaced. However, if the animation has
            // already started, we'll replace the placeholder with the real profile image.
            if (mModel.get(FullscreenSigninProperties.START_ANIMATION)) {
                mModel.set(
                        FullscreenSigninProperties.PROFILE_PICTURE,
                        mSigninAnimationProfileDataCache
                                .getById(mSelectedAccount.getId())
                                .getImage());
            }
        }
    }

    private void updateAccounts(List<AccountInfo> accounts) {
        @Nullable AccountInfo pendingAddedAccount =
                mPendingAddedAccountEmail == null
                        ? null
                        : AccountUtils.findAccountByEmail(accounts, mPendingAddedAccountEmail);
        if (pendingAddedAccount != null) {
            mPendingAddedAccountEmail = null;
            mAddedAccount = pendingAddedAccount;
            onAccountSelected(mAddedAccount);
            return;
        }

        if (accounts.isEmpty()) {
            mDefaultAccount = null;
            mSelectedAccount = null;
            mModel.set(FullscreenSigninProperties.BOTTOM_GROUP_ACCOUNT_DATA, null);
            mModel.set(FullscreenSigninProperties.ENABLE_ACCOUNT_SELECTION, false);
            if (mDialogCoordinator != null) {
                mDialogCoordinator.dismissDialog();
            }
        } else {
            mDefaultAccount = accounts.get(0);
            mSelectedAccount =
                    mSelectedAccount == null
                            ? null
                            : AccountUtils.findAccountByEmail(
                                    accounts, mSelectedAccount.getEmail());
            if (mSelectedAccount == null) {
                setSelectedAccount(mDefaultAccount);
            }
        }

        AccountUtils.checkIsSubjectToParentalControls(
                mAccountManagerFacade, accounts, this::onChildAccountStatusReady);
    }

    @VisibleForTesting
    void onChildAccountStatusReady(boolean isChild, @Nullable CoreAccountInfo childInfo) {
        if (mContinueButtonProfileDataCache == null) {
            // Both caches are initialized together so they're either both null or none of them are.
            assert mSigninAnimationProfileDataCache == null;
            return;
        }

        mIsChild = isChild;
        mModel.set(FullscreenSigninProperties.SHOW_ACCOUNT_SUPERVISION_NOTICE, mIsChild);
        if (mIsChild) {
            // Subtitle is hidden for child accounts.
            mModel.set(FullscreenSigninProperties.SUBTITLE_STRING, null);
        }
        mModel.set(
                FullscreenSigninProperties.ENABLE_ACCOUNT_SELECTION,
                !mIsChild && mSelectedAccount != null);
        updateShouldHideDismissButton();
        // Selected account data will be updated in {@link #onProfileDataUpdated}

        BadgeConfig continueButtonBadgeConfig = null;
        BadgeConfig signinAnimationBadgeConfig = null;
        if (isChild) {
            continueButtonBadgeConfig =
                    BadgeConfig.create(R.drawable.ic_account_child_20dp)
                            .withDefaultSizeChildAccountConfig()
                            .build(mContext);
            // The recommendation for a 32dp icon is to use the 40dp icon and scale it down.
            signinAnimationBadgeConfig =
                    BadgeConfig.create(R.drawable.ic_account_child_40dp)
                            .withLargeChildAccountConfig()
                            .build(mContext);
        }
        mContinueButtonProfileDataCache.setBadge(continueButtonBadgeConfig);
        if (mSigninAnimationProfileDataCache != null) {
            mSigninAnimationProfileDataCache.setBadge(signinAnimationBadgeConfig);
        }
    }

    /**
     * Builds footer string dynamically. Returns null if no footer text should be displayed. First
     * line has a TOS link. Second line appears only if MetricsReporting is not disabled by policy.
     */
    private @Nullable SpannableString getFooterString(boolean isMetricsReportingDisabled) {
        if (!mDelegate.shouldDisplayFooterText()) {
            return null;
        }
        String footerString = mContext.getString(R.string.signin_fre_footer_tos);

        ArrayList<SpanApplier.SpanInfo> spans = new ArrayList<>();
        // Terms of Service SpanInfo.
        final ChromeClickableSpan clickableTermsOfServiceSpan =
                new ChromeClickableSpan(
                        mContext,
                        view ->
                                mDelegate.showInfoPage(
                                        ColorUtils.inNightMode(mContext)
                                                ? R.string.google_terms_of_service_dark_mode_url
                                                : R.string.google_terms_of_service_url));
        spans.add(
                new SpanApplier.SpanInfo("<TOS_LINK>", "</TOS_LINK>", clickableTermsOfServiceSpan));

        // Metrics and Crash Reporting SpanInfo.
        if (!isMetricsReportingDisabled) {
            footerString += " " + mContext.getString(R.string.signin_fre_footer_metrics_reporting);
            final ChromeClickableSpan clickableUMADialogSpan =
                    new ChromeClickableSpan(mContext, view -> openUmaDialog());
            spans.add(
                    new SpanApplier.SpanInfo("<UMA_LINK>", "</UMA_LINK>", clickableUMADialogSpan));
        }

        // Apply spans to footer string.
        return SpanApplier.applySpans(footerString, spans.toArray(new SpanApplier.SpanInfo[0]));
    }

    public static void disableAnimationsForTesting() {
        boolean oldValue = sAnimationsEnabled;
        sAnimationsEnabled = false;
        ResettersForTesting.register(() -> sAnimationsEnabled = oldValue);
    }
}
