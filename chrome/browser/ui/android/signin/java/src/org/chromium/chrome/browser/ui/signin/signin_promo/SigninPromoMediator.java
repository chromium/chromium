// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

final class SigninPromoMediator implements ProfileDataCache.Observer {
    private final IdentityManager mIdentityManager;
    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoDelegate mDelegate;
    private final PropertyModel mModel;

    SigninPromoMediator(
            IdentityManager identityManager,
            ProfileDataCache profileDataCache,
            SigninPromoDelegate delegate) {
        mIdentityManager = identityManager;
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
    }

    public void destroy() {
        mProfileDataCache.removeObserver(this);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        if (visibleAccount != null && !visibleAccount.getEmail().equals(accountEmail)) {
            return;
        }

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

    PropertyModel getModel() {
        return mModel;
    }

    private @Nullable CoreAccountInfo getVisibleAccount() {
        @Nullable
        CoreAccountInfo visibleAccount =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        final AccountManagerFacade accountManagerFacade =
                AccountManagerFacadeProvider.getInstance();
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultCoreAccountInfoIfFulfilled(
                            accountManagerFacade.getCoreAccountInfos());
        }
        return visibleAccount;
    }
}
