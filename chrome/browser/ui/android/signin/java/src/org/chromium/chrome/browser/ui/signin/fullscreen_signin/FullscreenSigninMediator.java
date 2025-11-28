// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.accounts.Account;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.Event;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.services.SigninManager.SignOutCallback;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninCoordinator.Delegate;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

@NullMarked
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class FullscreenSigninMediator
        implements AccountsChangeObserver,
                ProfileDataCache.Observer,
                AccountPickerCoordinator.Listener,
                UMADialogCoordinator.Listener {
    private static final String TAG = "SigninFRMediator";

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
    private final Delegate mDelegate;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final @SigninAccessPoint int mAccessPoint;
    private final FullscreenSigninConfig mConfig;
    private final PropertyModel mModel;
    private @Nullable ProfileDataCache mProfileDataCache;
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

    void destroy() {
        assert !mDestroyed;
        if (mProfileDataCache != null) {
            mProfileDataCache.removeObserver(this);
        }
        mAccountManagerFacade.removeObserver(this);
        mDestroyed = true;
    }

    void reset() {
        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, false);
        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
    }

    private Account getSelectedAccount() {
        return CoreAccountInfo.getAndroidAccountFrom(assertNonNull(mSelectedAccount));
    }

    private void onNativeLoaded() {
        // This happens asynchronously, so this check is necessary to ensure we don't interact with
        // the delegate after the mediator is destroyed. See https://crbug.com/1294998.
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
        // the delegate after the mediator is destroyed. See https://crbug.com/1294998.
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
        Profile profile = assumeNonNull(mDelegate.getProfileSupplier().get()).getOriginalProfile();
        if (mProfileDataCache == null) {
            IdentityManager identityManager =
                    IdentityServicesProvider.get().getIdentityManager(profile);
            mProfileDataCache =
                    ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                            mContext, assertNonNull(identityManager));
            mProfileDataCache.addObserver(this);
            updateSelectedAccountData();
        }

        AccountUtils.checkIsSubjectToParentalControls(
                mAccountManagerFacade,
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()),
                this::onChildAccountStatusReady);

        boolean isMetricsReportingDisabledByPolicy = false;
        Log.i(TAG, "#onInitialLoadCompleted() hasPolicies:" + hasPolicies);
        if (hasPolicies) {
            isMetricsReportingDisabledByPolicy =
                    !mPrivacyPreferencesManager.isUsageAndCrashReportingPermittedByPolicy();
            mModel.set(
                    FullscreenSigninProperties.SHOW_ENTERPRISE_MANAGEMENT_NOTICE,
                    mDelegate.shouldDisplayManagementNoticeOnManagedDevices());
        }

        boolean isSigninSupported =
                ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                        && UserPrefs.get(profile).getBoolean(Pref.SIGNIN_ALLOWED)
                        && !mConfig.shouldDisableSignin;
        mModel.set(FullscreenSigninProperties.IS_SIGNIN_SUPPORTED, isSigninSupported);
        updateDimissText();
        mModel.set(FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER, false);

        if (isSigninSupported) {
            mModel.set(FullscreenSigninProperties.TITLE_STRING, mConfig.title);
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
            mModel.set(
                    FullscreenSigninProperties.SUBTITLE_STRING,
                    isSyncDataManaged
                            ? mContext.getString(R.string.signin_fre_subtitle_without_sync)
                            : mConfig.subtitle);
        } else {
            mModel.set(FullscreenSigninProperties.SUBTITLE_STRING, null);
        }

        mAllowMetricsAndCrashUploading = !isMetricsReportingDisabledByPolicy;
        mModel.set(
                FullscreenSigninProperties.FOOTER_STRING,
                getFooterString(isMetricsReportingDisabledByPolicy));
    }

    private void updateDimissText() {
        // TODO(crbug.com/464416507): Remove this method once the
        // FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT flag is cleaned up.
        if (FullscreenSigninConfig.DISMISS_TEXT_NOT_INITIALIZED.equals(mConfig.dismissText)) {
            final int secondaryButtonTextId =
                    SigninFeatureMap.isEnabled(
                                    SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
                            ? R.string.signin_fre_stay_signed_out_button
                            : R.string.signin_fre_dismiss_button;
            mModel.set(
                    FullscreenSigninProperties.DISMISS_BUTTON_STRING,
                    mContext.getString(secondaryButtonTextId));
        }
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
    public void onProfileDataUpdated(String accountEmail) {
        if (mSelectedAccount != null
                && TextUtils.equals(mSelectedAccount.getEmail(), accountEmail)) {
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
            mDelegate.displayDeviceLockPage(getSelectedAccount());
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
        Profile profile = assumeNonNull(mDelegate.getProfileSupplier().get()).getOriginalProfile();
        @Nullable CoreAccountInfo signedInAccount =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(profile))
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (signedInAccount != null && Objects.equals(signedInAccount, mSelectedAccount)) {
            mDelegate.advanceToNextPage();
            return;
        }
        final SigninManager signinManager =
                IdentityServicesProvider.get().getSigninManager(profile);
        assumeNonNull(signinManager);
        final SignInCallback signInCallback =
                new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        signinTimestampsLogger.recordTimestamp(Event.SIGNIN_COMPLETED);
                        if (mDestroyed) {
                            // FirstRunActivity was destroyed while we were waiting for sign-in.
                            return;
                        }
                        mDelegate.advanceToNextPage();
                    }

                    @Override
                    public void onSignInAborted() {
                        signinTimestampsLogger.recordTimestamp(Event.SIGNIN_ABORTED);
                        // TODO(crbug.com/40790332): For now we enable the buttons again to not
                        // block the
                        // users from continuing to the next page. Should show a dialog with the
                        // signin error.
                        mModel.set(
                                FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT,
                                false);
                        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
                    }
                };

        if (mSelectedAccount != null) {
            mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, true);
            final @SigninAccessPoint int accessPoint =
                    mModel.get(FullscreenSigninProperties.IS_SELECTED_ACCOUNT_SUPERVISED)
                            ? SigninAccessPoint.FORCED_SIGNIN
                            : mAccessPoint;
            if (signedInAccount != null) {
                // If there already exists another signed-in account, first sign-out and then
                // sign-in with the selected account.
                signOutThenSignInWithSelectedAccount(
                        mSelectedAccount,
                        signinManager,
                        accessPoint,
                        signinTimestampsLogger,
                        signInCallback);
            } else {
                FreManagementNoticeDialogHelper.checkAccountManagementAndSignIn(
                        mSelectedAccount,
                        signinManager,
                        signinTimestampsLogger,
                        accessPoint,
                        signInCallback,
                        mContext,
                        mModalDialogManager);
            }
        }
    }

    private void signOutThenSignInWithSelectedAccount(
            CoreAccountInfo selectedAccount,
            SigninManager signinManager,
            @SigninAccessPoint int accessPoint,
            SigninFlowTimestampsLogger signinTimestampsLogger,
            @Nullable SignInCallback signInCallback) {
        SignOutCallback signOutCallback =
                () ->
                        FreManagementNoticeDialogHelper.checkAccountManagementAndSignIn(
                                selectedAccount,
                                signinManager,
                                signinTimestampsLogger,
                                accessPoint,
                                signInCallback,
                                mContext,
                                mModalDialogManager);
        signinManager.signOut(
                SignoutReason.ABORT_SIGNIN, signOutCallback, /* forceWipeUserData= */ false);
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
            SignOutCallback signOutCallback =
                    () -> {
                        if (mDestroyed) {
                            // FirstRunActivity was destroyed while we were waiting for the
                            // sign-out.
                            return;
                        }

                        mDelegate.advanceToNextPage();
                    };
            assumeNonNull(IdentityServicesProvider.get().getSigninManager(profile))
                    .signOut(
                            SignoutReason.ABORT_SIGNIN,
                            signOutCallback,
                            /* forceWipeUserData= */ false);
        } else {
            mDelegate.advanceToNextPage();
        }
    }

    /**
     * Returns whether the user has already clicked either 'Continue' or 'Dismiss'.
     * If the user has pressed either of the two buttons consecutive taps are ignored.
     * See crbug.com/1294994 for details.
     */
    private boolean isContinueOrDismissClicked() {
        // These property keys are set when continue or dismiss button is clicked respectively.
        return mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT)
                || mModel.get(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER);
    }

    private void setSelectedAccount(CoreAccountInfo account) {
        mSelectedAccount = account;
        updateSelectedAccountData();
    }

    private void updateSelectedAccountData() {
        if (mProfileDataCache != null && mSelectedAccount != null) {
            mModel.set(
                    FullscreenSigninProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(mSelectedAccount.getEmail()));
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
            mModel.set(FullscreenSigninProperties.SELECTED_ACCOUNT_DATA, null);
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

    private void onChildAccountStatusReady(boolean isChild, @Nullable CoreAccountInfo childInfo) {
        if (mProfileDataCache == null) {
            return;
        }
        mModel.set(FullscreenSigninProperties.IS_SELECTED_ACCOUNT_SUPERVISED, isChild);
        // Selected account data will be updated in {@link #onProfileDataUpdated}
        mProfileDataCache.setBadge(
                isChild
                        ? ProfileDataCache.createDefaultSizeChildAccountBadgeConfig(
                                mContext, R.drawable.ic_account_child_20dp)
                        : null);
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
}
