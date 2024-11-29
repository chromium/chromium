// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;

final class SigninPromoMediator
        implements ProfileDataCache.Observer, AccountsChangeObserver, IdentityManager.Observer {
    private final IdentityManager mIdentityManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoDelegate mDelegate;
    private final PropertyModel mModel;
    private final boolean mMaxImpressionReached;

    private boolean mShouldShowPromo;
    private boolean mWasImpressionRecorded;

    SigninPromoMediator(
            IdentityManager identityManager,
            SigninManager signinManager,
            SyncService syncService,
            AccountManagerFacade accountManagerFacade,
            ProfileDataCache profileDataCache,
            SigninPromoDelegate delegate) {
        mIdentityManager = identityManager;
        mAccountManagerFacade = accountManagerFacade;
        mProfileDataCache = profileDataCache;
        mDelegate = delegate;

        @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        @Nullable
        DisplayableProfileData profileData =
                visibleAccount == null
                        ? null
                        : mProfileDataCache.getProfileDataOrDefault(visibleAccount.getEmail());
        mModel =
                SigninPromoProperties.createModel(
                        profileData,
                        mDelegate::onPrimaryButtonClicked,
                        mDelegate::onSecondaryButtonClicked,
                        this::onDismissButtonClicked,
                        delegate.getTitle(),
                        delegate.getDescription(),
                        delegate.getTextForPrimaryButton(profileData),
                        delegate.getTextForSecondaryButton(),
                        profileData == null || delegate.shouldHideSecondaryButton(),
                        delegate.shouldHideDismissButton());
        mMaxImpressionReached = mDelegate.isMaxImpressionsReached();
        mShouldShowPromo = canShowPromo();

        mProfileDataCache.addObserver(this);
        mIdentityManager.addObserver(this);
        mAccountManagerFacade.addObserver(this);
    }

    void destroy() {
        mAccountManagerFacade.removeObserver(this);
        mIdentityManager.removeObserver(this);
        mProfileDataCache.removeObserver(this);
    }

    void recordImpression() {
        if (mWasImpressionRecorded) {
            // Impressions are recorded only once per coordinator lifecycle.
            return;
        }
        mDelegate.recordImpression();
        mWasImpressionRecorded = true;
    }

    boolean canShowPromo() {
        return !mMaxImpressionReached && mDelegate.canShowPromo();
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        if (visibleAccount != null && !visibleAccount.getEmail().equals(accountEmail)) {
            return;
        }
        updateModel(visibleAccount);
    }

    @Override
    public void onCoreAccountInfosChanged() {
        updateModel(getVisibleAccount());
    }

    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        updateModel(getVisibleAccount());
        updateState();
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onDismissButtonClicked() {
        mDelegate.onDismissButtonClicked();
        updateState();
    }

    private void updateModel(@Nullable CoreAccountInfo visibleAccount) {
        @Nullable
        DisplayableProfileData profileData =
                visibleAccount == null
                        ? null
                        : mProfileDataCache.getProfileDataOrDefault(visibleAccount.getEmail());
        mModel.set(SigninPromoProperties.PROFILE_DATA, profileData);
        mModel.set(
                SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON,
                profileData == null || mDelegate.shouldHideDismissButton());
    }

    private void updateState() {
        boolean shouldShowPromo = canShowPromo();
        if (mShouldShowPromo == shouldShowPromo) {
            return;
        }
        mShouldShowPromo = shouldShowPromo;
        mDelegate.onPromoVisibilityChange();
    }

    private @Nullable CoreAccountInfo getVisibleAccount() {
        @Nullable
        CoreAccountInfo visibleAccount =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultCoreAccountInfoIfFulfilled(
                            mAccountManagerFacade.getCoreAccountInfos());
        }
        return visibleAccount;
    }
}
