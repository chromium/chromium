// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import android.accounts.Account;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
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
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;

@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class FullscreenSigninMediator
        implements AccountsChangeObserver,
                ProfileDataCache.Observer,
                AccountPickerCoordinator.Listener,
                UMADialogCoordinator.Listener {
    private static final String TAG = "SigninFRMediator";

    /**
     * Used for MobileFre.SlowestLoadPoint histogram. Should be treated as append-only.
     * See {@code LoadPoint} in tools/metrics/histograms/enums.xml.
     */
    @VisibleForTesting
    @IntDef({
        LoadPoint.NATIVE_INITIALIZATION,
        LoadPoint.POLICY_LOAD,
        LoadPoint.CHILD_STATUS_LOAD,
        LoadPoint.MAX
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadPoint {
        int NATIVE_INITIALIZATION = 0;
        int POLICY_LOAD = 1;
        int CHILD_STATUS_LOAD = 2;
        int MAX = 3;
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Delegate mDelegate;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final @SigninAccessPoint int mAccessPoint;
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private boolean mDestroyed;

    private @LoadPoint int mSlowestLoadPoint;

    /** Whether the initial load phase has been completed. See {@link #onInitialLoadCompleted}. */
    private boolean mInitialLoadCompleted;

    private AccountPickerDialogCoordinator mDialogCoordinator;
    // TODO(crbug.com/40921927): Replace with CoreAccountInfo.
    private @Nullable String mSelectedAccountEmail;
    // TODO(crbug.com/40921927): Replace with CoreAccountInfo.
    private @Nullable String mDefaultAccountEmail;
    private boolean mAllowMetricsAndCrashUploading;

    FullscreenSigninMediator(
            Context context,
            ModalDialogManager modalDialogManager,
            Delegate delegate,
            PrivacyPreferencesManager privacyPreferencesManager,
            @SigninAccessPoint int accessPoint) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDelegate = delegate;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mAccessPoint = accessPoint;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        mModel =
                FullscreenSigninProperties.createModel(
                        this::onSelectedAccountClicked,
                        this::onContinueAsClicked,
                        this::onDismissClicked,
                        ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                                && !disableSignInForAutomotiveDevice(),
                        R.string.fre_welcome,
                        R.string.signin_fre_subtitle_legacy);

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

        mProfileDataCache.addObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        updateAccounts(
                AccountUtils.getCoreAccountInfosIfFulfilledOrEmpty(
                        mAccountManagerFacade.getCoreAccountInfos()));
        SigninMetricsUtils.logSigninStarted(accessPoint);
    }

    PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        assert !mDestroyed;
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
        mDestroyed = true;
    }

    void reset() {
        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, false);
        mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
    }

    private Account getSelectedAccount() {
        return AccountUtils.createAccountFromName(mSelectedAccountEmail);
    }

    private void onNativeLoaded() {
        // This happens asynchronously, so this check is necessary to ensure we don't interact with
        // the delegate after the mediator is destroyed. See https://crbug.com/1294998.
        if (mDestroyed) return;

        mSlowestLoadPoint = LoadPoint.NATIVE_INITIALIZATION;
        mDelegate.recordNativeInitializedHistogram();
        checkWhetherInitialLoadCompleted();
    }

    private void onChildAccountStatusAvailable() {
        mSlowestLoadPoint = LoadPoint.CHILD_STATUS_LOAD;
        checkWhetherInitialLoadCompleted();
    }

    private void onPolicyLoad() {
        mSlowestLoadPoint = LoadPoint.POLICY_LOAD;
        checkWhetherInitialLoadCompleted();
    }

    /** Checks the initial load status. See {@link #onInitialLoadCompleted} for details. */
    private void checkWhetherInitialLoadCompleted() {
        // This happens asynchronously, so this check is necessary to ensure we don't interact with
        // the delegate after the mediator is destroyed. See https://crbug.com/1294998.
        if (mDestroyed) return;

        // The initialization flow requires native to be ready before the initial loading spinner
        // can be hidden.
        if (!mDelegate.getNativeInitializationPromise().isFulfilled()) return;

        if (mDelegate.getChildAccountStatusSupplier().get() != null
                && mDelegate.getPolicyLoadListener().get() != null
                && !mInitialLoadCompleted) {
            mInitialLoadCompleted = true;
            onInitialLoadCompleted(mDelegate.getPolicyLoadListener().get());
            // TODO(crbug.com/40235150): Rename this method and the corresponding histogram.
            mDelegate.recordNativePolicyAndChildStatusLoadedHistogram();
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileFre.SlowestLoadPoint", mSlowestLoadPoint, LoadPoint.MAX);
        }
    }

    /**
     * Called when the initial load phase is completed.
     *
     * After creation, {@link FullscreenSigninView} displays a loading spinner that is shown until
     * policies and the child account status are being checked. If needed, that phase also waits for
     * the native to be loaded (for example, if any app restrictions are detected). This method is
     * invoked when this initial waiting phase is over and the "Continue" button can be displayed.
     * It checks policies and child accounts to decide which version of the UI to display.
     *
     * @param hasPolicies Whether any enterprise policies have been found on the device. 'true' here
     *                    also means that native has been initialized.
     */
    void onInitialLoadCompleted(boolean hasPolicies) {
        boolean isSigninDisabledByPolicy = false;
        boolean isMetricsReportingDisabledByPolicy = false;
        Log.i(TAG, "#onInitialLoadCompleted() hasPolicies:" + hasPolicies);
        Profile profile = mDelegate.getProfileSupplier().get().getOriginalProfile();
        if (hasPolicies) {
            isSigninDisabledByPolicy =
                    IdentityServicesProvider.get()
                            .getSigninManager(profile)
                            .isSigninDisabledByPolicy();
            Log.i(
                    TAG,
                    "#onInitialLoadCompleted() isSigninDisabledByPolicy:"
                            + isSigninDisabledByPolicy);
            isMetricsReportingDisabledByPolicy =
                    !mPrivacyPreferencesManager.isUsageAndCrashReportingPermittedByPolicy();
            mModel.set(
                    FullscreenSigninProperties.SHOW_ENTERPRISE_MANAGEMENT_NOTICE,
                    mDelegate.shouldDisplayManagementNoticeOnManagedDevices());
        }

        boolean isSigninSupported =
                ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                        && !isSigninDisabledByPolicy
                        && !disableSignInForAutomotiveDevice();
        mModel.set(FullscreenSigninProperties.IS_SIGNIN_SUPPORTED, isSigninSupported);
        mModel.set(FullscreenSigninProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER, false);

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            if (isSigninSupported) {
                mModel.set(FullscreenSigninProperties.TITLE_STRING_ID, R.string.signin_fre_title);
            }
            SyncService syncService = SyncServiceFactory.getForProfile(profile);
            boolean isSyncDataManaged =
                    IntStream.range(UserSelectableType.FIRST_TYPE, UserSelectableType.LAST_TYPE + 1)
                            .anyMatch(syncService::isTypeManagedByPolicy);
            mModel.set(
                    FullscreenSigninProperties.SUBTITLE_STRING_ID,
                    isSyncDataManaged
                            ? R.string.signin_fre_subtitle_without_sync
                            : R.string.signin_fre_subtitle);
        }

        mAllowMetricsAndCrashUploading = !isMetricsReportingDisabledByPolicy;
        mModel.set(
                FullscreenSigninProperties.FOOTER_STRING,
                getFooterString(isMetricsReportingDisabledByPolicy));
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onCoreAccountInfosChanged() {
        // TODO(crbug.com/40065164): Replace onAccountsChanged() with this method.
        mAccountManagerFacade.getCoreAccountInfos().then(this::updateAccounts);
    }

    @Override
    public void onAccountSelected(String accountName) {
        setSelectedAccountEmail(accountName);
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
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(mContext, this, mModalDialogManager);
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
        if (mSelectedAccountEmail == null) {
            mDelegate.addAccount();
            return;
        }

        if (BuildInfo.getInstance().isAutomotive) {
            mDelegate.displayDeviceLockPage(getSelectedAccount());
            return;
        }
        proceedWithSignIn();
    }

    /** Accepts ToS and completes the account sign-in with the selected account. */
    void proceedWithSignIn() {
        // This is needed to get metrics/crash reports from the sign-in flow itself.
        mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                && mModel.get(FullscreenSigninProperties.IS_SELECTED_ACCOUNT_SUPERVISED)) {
            // Don't perform the sign-in here, as it will be handled by SigninChecker.
            mDelegate.advanceToNextPage();
            return;
        }
        mDelegate.recordFreProgressHistogram(
                TextUtils.equals(mDefaultAccountEmail, mSelectedAccountEmail)
                        ? MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT
                        : MobileFreProgress.WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT);
        // If the user signs into an account on the FRE, goes to the next page and presses
        // back to come back to the welcome screen, then there will already be an account signed in.
        @Nullable
        CoreAccountInfo signedInAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(
                                mDelegate.getProfileSupplier().get().getOriginalProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (signedInAccount != null && signedInAccount.getEmail().equals(mSelectedAccountEmail)) {
            mDelegate.advanceToNextPage();
            return;
        }
        final SigninManager signinManager =
                IdentityServicesProvider.get()
                        .getSigninManager(
                                mDelegate.getProfileSupplier().get().getOriginalProfile());
        final SignInCallback signInCallback =
                new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        if (mDestroyed) {
                            // FirstRunActivity was destroyed while we were waiting for sign-in.
                            return;
                        }
                        mDelegate.advanceToNextPage();
                    }

                    @Override
                    public void onSignInAborted() {
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
        CoreAccountInfo selectedAccount =
                AccountUtils.findCoreAccountInfoByEmail(
                        mAccountManagerFacade.getCoreAccountInfos().getResult(),
                        mSelectedAccountEmail);
        if (selectedAccount != null) {
            mModel.set(FullscreenSigninProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, true);
            final @SigninAccessPoint int accessPoint =
                    mModel.get(FullscreenSigninProperties.IS_SELECTED_ACCOUNT_SUPERVISED)
                            ? SigninAccessPoint.FORCED_SIGNIN
                            : mAccessPoint;
            FreManagementNoticeDialogHelper.checkAccountManagementAndSignIn(
                    selectedAccount,
                    signinManager,
                    accessPoint,
                    signInCallback,
                    mContext,
                    mModalDialogManager);
        }
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
        mDelegate.recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
        mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
        SigninPreferencesManager.getInstance().temporarilySuppressNewTabPagePromos();
        if (IdentityServicesProvider.get()
                .getIdentityManager(mDelegate.getProfileSupplier().get().getOriginalProfile())
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
            IdentityServicesProvider.get()
                    .getSigninManager(mDelegate.getProfileSupplier().get().getOriginalProfile())
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

    private void setSelectedAccountEmail(String accountEmail) {
        mSelectedAccountEmail = accountEmail;
        updateSelectedAccountData(mSelectedAccountEmail);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountEmail, accountEmail)) {
            mModel.set(
                    FullscreenSigninProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
        }
    }

    private void updateAccounts(List<CoreAccountInfo> coreAccountInfos) {
        if (coreAccountInfos.isEmpty()) {
            mDefaultAccountEmail = null;
            mSelectedAccountEmail = null;
            mModel.set(FullscreenSigninProperties.SELECTED_ACCOUNT_DATA, null);
            if (mDialogCoordinator != null) {
                mDialogCoordinator.dismissDialog();
            }
        } else {
            mDefaultAccountEmail = coreAccountInfos.get(0).getEmail();
            if (mSelectedAccountEmail == null
                    || AccountUtils.findCoreAccountInfoByEmail(
                                    coreAccountInfos, mSelectedAccountEmail)
                            == null) {
                setSelectedAccountEmail(mDefaultAccountEmail);
            }
        }

        AccountUtils.checkChildAccountStatus(
                mAccountManagerFacade, coreAccountInfos, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable CoreAccountInfo childInfo) {
        mModel.set(FullscreenSigninProperties.IS_SELECTED_ACCOUNT_SUPERVISED, isChild);
        // Selected account data will be updated in {@link #onProfileDataUpdated}
        mProfileDataCache.setBadge(isChild ? R.drawable.ic_account_child_20dp : 0);
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
        final NoUnderlineClickableSpan clickableTermsOfServiceSpan =
                new NoUnderlineClickableSpan(
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
            final NoUnderlineClickableSpan clickableUMADialogSpan =
                    new NoUnderlineClickableSpan(mContext, view -> openUmaDialog());
            spans.add(
                    new SpanApplier.SpanInfo("<UMA_LINK>", "</UMA_LINK>", clickableUMADialogSpan));
        }

        // Apply spans to footer string.
        return SpanApplier.applySpans(footerString, spans.toArray(new SpanApplier.SpanInfo[0]));
    }

    private static boolean disableSignInForAutomotiveDevice() {
        return BuildInfo.getInstance().isAutomotive;
    }
}
