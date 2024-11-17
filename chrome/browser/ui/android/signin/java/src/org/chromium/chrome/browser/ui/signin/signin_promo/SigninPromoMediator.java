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
import org.chromium.ui.modelutil.PropertyModel;

final class SigninPromoMediator
        implements ProfileDataCache.Observer, AccountsChangeObserver, IdentityManager.Observer {
    private final IdentityManager mIdentityManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoDelegate mDelegate;
    private final PropertyModel mModel;

    SigninPromoMediator(
            IdentityManager identityManager,
            SigninManager signinManager,
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
                        delegate.getTitle(),
                        delegate.getDescription(),
                        delegate.getTextForPrimaryButton(profileData),
                        delegate.getTextForSecondaryButton(),
                        profileData == null || delegate.shouldHideSecondaryButton(),
                        delegate.shouldHideDismissButton());

        mProfileDataCache.addObserver(this);
        mIdentityManager.addObserver(this);
        mAccountManagerFacade.addObserver(this);
    }

    public void destroy() {
        mAccountManagerFacade.removeObserver(this);
        mIdentityManager.removeObserver(this);
        mProfileDataCache.removeObserver(this);
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
    }

    PropertyModel getModel() {
        return mModel;
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
