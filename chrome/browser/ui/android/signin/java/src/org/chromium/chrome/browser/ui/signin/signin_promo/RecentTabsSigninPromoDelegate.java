// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** {@link SigninPromoDelegate} for recent tabs signin promo. */
@NullMarked
public class RecentTabsSigninPromoDelegate extends SigninPromoDelegate {

    /** Indicates the type of content the should be shown in the visible promo. */
    @IntDef({PromoState.NONE, PromoState.SIGNIN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /** The promo content should promote sign-in. Shown to signed-out user. */
        int SIGNIN = 1;
    }

    private final String mPromoShowCountPreferenceName;
    private @PromoState int mPromoState = PromoState.NONE;

    public RecentTabsSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange) {
        super(context, profile, launcher, onPromoStateChange);

        mPromoShowCountPreferenceName =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.RECENT_TABS);
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
    @SigninPreferencesManager.SigninPromoAccessPointId
    String getAccessPointName() {
        return SigninPreferencesManager.SigninPromoAccessPointId.RECENT_TABS;
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
    boolean canShowPromo() {
        return mPromoState != PromoState.NONE;
    }

    @Override
    boolean refreshPromoState(@Nullable CoreAccountInfo visibleAccount) {
        @PromoState int newState = computePromoState();
        boolean wasStateChanged = mPromoState != newState;
        mPromoState = newState;
        return wasStateChanged;
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
    void recordImpression() {
        ChromeSharedPreferences.getInstance().incrementInt(mPromoShowCountPreferenceName);
    }

    @Override
    @HistorySyncConfig.OptInMode
    int getHistoryOptInMode() {
        return HistorySyncConfig.OptInMode.REQUIRED;
    }

    @Override
    int getPromoShownCount() {
        return ChromeSharedPreferences.getInstance().readInt(mPromoShowCountPreferenceName);
    }

    private @PromoState int computePromoState() {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assumeNonNull(signinManager);
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                && !signinManager.isSigninAllowed()) {
            // If sign-in is not possible, then history sync isn't possible either.
            return PromoState.NONE;
        }
        final HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(mProfile);
        return historySyncHelper.shouldSuppressHistorySync() ? PromoState.NONE : PromoState.SIGNIN;
    }
}
