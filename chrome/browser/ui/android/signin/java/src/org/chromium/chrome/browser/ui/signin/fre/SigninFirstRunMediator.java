// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.accounts.Account;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
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
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.ChildAccountStatus.Status;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;

class SigninFirstRunMediator implements AccountsChangeObserver, ProfileDataCache.Observer,
                                        AccountPickerCoordinator.Listener {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Delegate mDelegate;
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private AccountPickerDialogCoordinator mDialogCoordinator;
    private @Nullable String mSelectedAccountName;
    private @Nullable String mDefaultAccountName;

    SigninFirstRunMediator(
            Context context, ModalDialogManager modalDialogManager, Delegate delegate) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDelegate = delegate;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        mModel = SigninFirstRunProperties.createModel(this::onSelectedAccountClicked,
                this::onContinueAsClicked, this::onDismissClicked,
                ExternalAuthUtils.getInstance().canUseGooglePlayServices(), getFooterString(false));

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
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
    }

    void onNativeAndPolicyLoaded(boolean hasPolicies) {
        mModel.set(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED, true);
        mModel.set(SigninFirstRunProperties.FRE_POLICY, hasPolicies ? new FrePolicy() : null);
        final boolean isSigninSupported = ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                && !IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .isSigninDisabledByPolicy();
        mModel.set(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED, isSigninSupported);
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

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(mContext, this, mModalDialogManager);
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        if (!mModel.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
            mDelegate.acceptTermsOfService();
            return;
        }
        if (mSelectedAccountName == null) {
            mDelegate.addAccount();
            return;
        }
        if (mModel.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED)) {
            mDelegate.acceptTermsOfService();
            return;
        }
        assert mModel.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED)
            : "The continue button shouldn't be visible before the native is not initialized!";
        mDelegate.recordFreProgressHistogram(
                TextUtils.equals(mDefaultAccountName, mSelectedAccountName)
                        ? MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT
                        : MobileFreProgress.WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT);
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            mDelegate.acceptTermsOfService();
            return;
        }
        final SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        signinManager.onFirstRunCheckDone();
        signinManager.signin(
                AccountUtils.createAccountFromName(mSelectedAccountName), new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        mDelegate.acceptTermsOfService();
                    }

                    @Override
                    public void onSignInAborted() {
                        // TODO(crbug/1248090): Handle the sign-in error here
                    }
                });
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_DISMISS_CLICKED}.
     */
    private void onDismissClicked() {
        assert mModel.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED)
            : "The dismiss button shouldn't be visible before the native is not initialized!";
        mDelegate.recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .signOut(SignoutReason.ABORT_SIGNIN, mDelegate::acceptTermsOfService,
                            /* forceWipeUserData= */ false);
        } else {
            mDelegate.acceptTermsOfService();
        }
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

    private void onChildAccountStatusReady(@Status int status, @Nullable Account childAccount) {
        final boolean isChild = ChildAccountStatus.isChild(status);
        mModel.set(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED, isChild);
        mModel.set(SigninFirstRunProperties.FOOTER_STRING, getFooterString(isChild));
        // Selected account data will be updated in {@link #onProfileDataUpdated}
        mProfileDataCache.setBadge(isChild ? R.drawable.ic_account_child_20dp : 0);
    }

    private SpannableString getFooterString(boolean hasChildAccount) {
        final NoUnderlineClickableSpan clickableTermsOfServiceSpan =
                new NoUnderlineClickableSpan(mContext.getResources(),
                        view -> mDelegate.showInfoPage(R.string.google_terms_of_service_url));
        final SpanApplier.SpanInfo tosSpanInfo =
                new SpanApplier.SpanInfo("<TOS_LINK>", "</TOS_LINK>", clickableTermsOfServiceSpan);
        final NoUnderlineClickableSpan clickableUMADialogSpan = new NoUnderlineClickableSpan(
                mContext.getResources(), view -> mDelegate.openUmaDialog());
        final SpanApplier.SpanInfo umaSpanInfo =
                new SpanApplier.SpanInfo("<UMA_LINK>", "</UMA_LINK>", clickableUMADialogSpan);
        if (hasChildAccount) {
            final NoUnderlineClickableSpan clickablePrivacyPolicySpan =
                    new NoUnderlineClickableSpan(mContext.getResources(),
                            view -> mDelegate.showInfoPage(R.string.google_privacy_policy_url));
            final SpanApplier.SpanInfo privacySpanInfo = new SpanApplier.SpanInfo(
                    "<PRIVACY_LINK>", "</PRIVACY_LINK>", clickablePrivacyPolicySpan);
            return SpanApplier.applySpans(
                    mContext.getString(R.string.signin_fre_footer_supervised_user), tosSpanInfo,
                    umaSpanInfo, privacySpanInfo);
        } else {
            return SpanApplier.applySpans(
                    mContext.getString(R.string.signin_fre_footer), tosSpanInfo, umaSpanInfo);
        }
    }
}
