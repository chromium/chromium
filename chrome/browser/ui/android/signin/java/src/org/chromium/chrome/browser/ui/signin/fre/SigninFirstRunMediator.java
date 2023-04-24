// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.accounts.Account;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.signin.services.SigninManager.SignOutCallback;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunCoordinator.Delegate;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunProperties.FrePolicy;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public class SigninFirstRunMediator
        implements AccountsChangeObserver, ProfileDataCache.Observer,
                   AccountPickerCoordinator.Listener, FreUMADialogCoordinator.Listener {
    /**
     * Used for MobileFre.SlowestLoadPoint histogram. Should be treated as append-only.
     * See {@code LoadPoint} in tools/metrics/histograms/enums.xml.
     */
    @VisibleForTesting
    @IntDef({LoadPoint.NATIVE_INITIALIZATION, LoadPoint.POLICY_LOAD, LoadPoint.CHILD_STATUS_LOAD,
            LoadPoint.MAX})
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
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private boolean mDestroyed;

    private @LoadPoint int mSlowestLoadPoint;
    /** Whether the initial load phase has been completed. See {@link #onInitialLoadCompleted}. */
    private boolean mInitialLoadCompleted;

    private AccountPickerDialogCoordinator mDialogCoordinator;
    private @Nullable String mSelectedAccountName;
    private @Nullable String mDefaultAccountName;
    private boolean mAllowMetricsAndCrashUploading;

    SigninFirstRunMediator(Context context, ModalDialogManager modalDialogManager,
            Delegate delegate, PrivacyPreferencesManager privacyPreferencesManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDelegate = delegate;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        mModel = SigninFirstRunProperties.createModel(this::onSelectedAccountClicked,
                this::onContinueAsClicked, this::onDismissClicked,
                ExternalAuthUtils.getInstance().canUseGooglePlayServices(), getFooterString(false));

        mDelegate.getNativeInitializationPromise().then(result -> { onNativeLoaded(); });
        mDelegate.getPolicyLoadListener().onAvailable(hasPolicies -> onPolicyLoad());
        mDelegate.getChildAccountStatusSupplier().onAvailable(
                ignored -> onChildAccountStatusAvailable());

        mProfileDataCache.addObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        updateAccounts(
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()));
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
        mModel.set(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, false);
        mModel.set(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
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
                && mDelegate.getPolicyLoadListener().get() != null && !mInitialLoadCompleted) {
            mInitialLoadCompleted = true;
            onInitialLoadCompleted(mDelegate.getPolicyLoadListener().get());
            // TODO(https://crbug.com/1353330): Rename this method and the corresponding histogram.
            mDelegate.recordNativePolicyAndChildStatusLoadedHistogram();
            RecordHistogram.recordEnumeratedHistogram(
                    "MobileFre.SlowestLoadPoint", mSlowestLoadPoint, LoadPoint.MAX);
        }
    }

    /**
     * Called when the initial load phase is completed.
     *
     * After creation, {@link SigninFirstRunView} displays a loading spinner that is shown until
     * policies and the child account status are being checked. If needed, that phase also waits for
     * the native to be loaded (for example, if any app restrictions are detected). This method is
     * invoked when this initial waiting phase is over and the "Continue" button can be displayed.
     * It checks policies and child accounts to decide which version of the UI to display.
     *
     * @param hasPolicies Whether any enterprise policies have been found on the device. 'true' here
     *                    also means that native has been initialized.
     */
    void onInitialLoadCompleted(boolean hasPolicies) {
        mModel.set(SigninFirstRunProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER, false);

        boolean isSigninDisabledByPolicy = false;
        boolean isMetricsReportingDisabledByPolicy = false;
        if (hasPolicies) {
            isSigninDisabledByPolicy =
                    IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .isSigninDisabledByPolicy();
            isMetricsReportingDisabledByPolicy =
                    !mPrivacyPreferencesManager.isUsageAndCrashReportingPermittedByPolicy();

            final FrePolicy frePolicy = new FrePolicy();
            frePolicy.metricsReportingDisabledByPolicy = isMetricsReportingDisabledByPolicy;
            mModel.set(SigninFirstRunProperties.FRE_POLICY, frePolicy);
        }

        mModel.set(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED,
                ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                        && !isSigninDisabledByPolicy);
        mAllowMetricsAndCrashUploading = !isMetricsReportingDisabledByPolicy;

        mModel.set(SigninFirstRunProperties.FOOTER_STRING,
                getFooterString(isMetricsReportingDisabledByPolicy));
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onAccountsChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    @Override
    public void onAccountSelected(String accountName) {
        setSelectedAccountName(accountName);
        if (mDialogCoordinator != null) mDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
        mDelegate.addAccount();
    }

    /** Implements {@link FreUMADialogCoordinator.Listener} */
    @Override
    public void onAllowMetricsAndCrashUploadingChecked(boolean allowMetricsAndCrashUploading) {
        mAllowMetricsAndCrashUploading = allowMetricsAndCrashUploading;
    }

    private void openUmaDialog() {
        new FreUMADialogCoordinator(
                mContext, mModalDialogManager, this, mAllowMetricsAndCrashUploading);
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        if (isContinueOrDismissClicked()) return;
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(mContext, this, mModalDialogManager);
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        assert mDelegate.getNativeInitializationPromise().isFulfilled();
        if (isContinueOrDismissClicked()) return;
        assert !mModel.get(SigninFirstRunProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER)
            : "The continue button shouldn't be visible while the load spinner is shown!";

        if (!mModel.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
            mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
            mDelegate.advanceToNextPage();
            return;
        }
        if (mSelectedAccountName == null) {
            mDelegate.addAccount();
            return;
        }

        if (BuildInfo.getInstance().isAutomotive) {
            mDelegate.displayDeviceLockPage();
            return;
        }
        proceedWithSignIn();
    }

    /**
     * Accepts ToS and completes the account sign-in with the selected account.
     */
    public void proceedWithSignIn() {
        // This is needed to get metrics/crash reports from the sign-in flow itself.
        mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
        if (mModel.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED)) {
            // Don't perform the sign-in here, as it will be handled by SigninChecker.
            mDelegate.advanceToNextPage();
            return;
        }
        mDelegate.recordFreProgressHistogram(
                TextUtils.equals(mDefaultAccountName, mSelectedAccountName)
                        ? MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT
                        : MobileFreProgress.WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT);
        // If the user signs into an account on the FRE, goes to the sync consent page and presses
        // back to come back to the FRE, then there will already be an account signed in.
        @Nullable
        CoreAccountInfo signedInAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (signedInAccount != null && signedInAccount.getEmail().equals(mSelectedAccountName)) {
            mDelegate.advanceToNextPage();
            return;
        }
        mModel.set(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT, true);
        final SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        signinManager.signin(AccountUtils.createAccountFromName(mSelectedAccountName),
                SigninAccessPoint.SIGNIN_PROMO, new SignInCallback() {
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
                        // TODO(crbug/1248090): For now we enable the buttons again to not block the
                        // users from continuing to the next page. Should show a dialog with the
                        // signin error.
                        mModel.set(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT,
                                false);
                        mModel.set(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER, false);
                    }
                });
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_DISMISS_CLICKED}.
     */
    private void onDismissClicked() {
        if (isContinueOrDismissClicked()) return;
        assert !mModel.get(SigninFirstRunProperties.SHOW_INITIAL_LOAD_PROGRESS_SPINNER)
            : "The dismiss button shouldn't be visible while the load spinner is shown!";

        assert mDelegate.getNativeInitializationPromise().isFulfilled();

        dismiss();
    }

    /**
     * Dismisses the sign-in page and continues without a signed-in account.
     */
    public void dismiss() {
        mDelegate.recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
        mDelegate.acceptTermsOfService(mAllowMetricsAndCrashUploading);
        SigninPreferencesManager.getInstance().temporarilySuppressNewTabPagePromos();
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            mModel.set(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER, true);
            SignOutCallback signOutCallback = () -> {
                if (mDestroyed) {
                    // FirstRunActivity was destroyed while we were waiting for the sign-out.
                    return;
                }

                mDelegate.advanceToNextPage();
            };
            IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .signOut(SignoutReason.ABORT_SIGNIN, signOutCallback,
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
        return mModel.get(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER_WITH_TEXT)
                || mModel.get(SigninFirstRunProperties.SHOW_SIGNIN_PROGRESS_SPINNER);
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        updateSelectedAccountData(mSelectedAccountName);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountName, accountEmail)) {
            mModel.set(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
        }
    }

    private void updateAccounts(List<Account> accounts) {
        if (accounts.isEmpty()) {
            mDefaultAccountName = null;
            mSelectedAccountName = null;
            mModel.set(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA, null);
            if (mDialogCoordinator != null) {
                mDialogCoordinator.dismissDialog();
            }
        } else {
            mDefaultAccountName = accounts.get(0).name;
            if (mSelectedAccountName == null
                    || AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null) {
                setSelectedAccountName(mDefaultAccountName);
            }
        }

        AccountUtils.checkChildAccountStatus(
                mAccountManagerFacade, accounts, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable Account childAccount) {
        mModel.set(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED, isChild);
        // Selected account data will be updated in {@link #onProfileDataUpdated}
        mProfileDataCache.setBadge(isChild ? R.drawable.ic_account_child_20dp : 0);
    }

    /**
     * Builds footer string dynamically.
     * First line has a TOS link. Second line appears only if MetricsReporting is not
     * disabled by policy.
     */
    private SpannableString getFooterString(boolean isMetricsReportingDisabled) {
        String footerString = mContext.getString(R.string.signin_fre_footer_tos);

        ArrayList<SpanApplier.SpanInfo> spans = new ArrayList<>();
        // Terms of Service SpanInfo.
        final NoUnderlineClickableSpan clickableTermsOfServiceSpan =
                new NoUnderlineClickableSpan(mContext,
                        view
                        -> mDelegate.showInfoPage(ColorUtils.inNightMode(mContext)
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
}
