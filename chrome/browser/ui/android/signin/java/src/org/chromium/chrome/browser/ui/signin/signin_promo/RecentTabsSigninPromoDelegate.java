// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** {@link SigninPromoDelegate} for recent tabs signin promo. */
public class RecentTabsSigninPromoDelegate extends SigninPromoDelegate {
    public RecentTabsSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange) {
        super(context, profile, launcher, onPromoStateChange);
    }

    @Override
    String getTitle() {
        return mContext.getString(R.string.signin_promo_title_recent_tabs);
    }

    @Override
    String getDescription() {
        return mContext.getString(R.string.signin_promo_description_recent_tabs);
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.RECENT_TABS;
    }

    @Override
    void onDismissButtonClicked() {
        throw new IllegalStateException("Recent tabs promos shouldn't have a dismiss button");
    }

    @Override
    boolean canShowPromo(@Nullable CoreAccountInfo visibleAccount) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                && !signinManager.isSigninAllowed()) {
            // If sign-in is not possible, then history sync isn't possible either.
            return false;
        }
        final HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(mProfile);
        return !historySyncHelper.shouldSuppressHistorySync();
    }

    @Override
    boolean shouldHideSecondaryButton() {
        return true;
    }

    @Override
    boolean shouldHideDismissButton() {
        return true;
    }

    @Override
    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        return mContext.getString(R.string.signin_promo_turn_on);
    }

    @Override
    @HistorySyncConfig.OptInMode
    int getHistoryOptInMode() {
        return HistorySyncConfig.OptInMode.REQUIRED;
    }
}
