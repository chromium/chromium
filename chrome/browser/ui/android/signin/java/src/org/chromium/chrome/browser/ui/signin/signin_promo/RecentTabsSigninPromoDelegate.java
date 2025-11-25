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
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
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
    @IntDef({PromoState.NONE, PromoState.SIGNIN, PromoState.HISTORY_SYNC})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /** The promo content should promote sign-in. Shown to signed-out user. */
        int SIGNIN = 1;

        /**
         * The promo content should promote enabling history and tabs sync in the settings. Shown to
         * signed-in user with history and tabs sync disabled in settings.
         */
        int HISTORY_SYNC = 2;
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
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        switch (mPromoState) {
            case PromoState.SIGNIN:
                if (seamlessSigninStringType
                        == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
                    return mContext.getString(R.string.signin_history_sync_promo_title_recent_tabs);
                }
                if (seamlessSigninStringType
                        == SigninFeatureMap.SeamlessSigninStringType.CONTINUE_BUTTON) {
                    return mContext.getString(R.string.signin_account_picker_bottom_sheet_title);
                }
                return mContext.getString(R.string.signin_promo_title_recent_tabs);
            case PromoState.HISTORY_SYNC:
                return mContext.getString(R.string.signin_history_sync_promo_title_recent_tabs);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
    }

    @Override
    String getDescription(@Nullable String accountEmail) {
        @SigninFeatureMap.SeamlessSigninPromoType
        int seamlessSigninPromoType = SigninFeatureMap.getInstance().getSeamlessSigninPromoType();
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        switch (mPromoState) {
            case PromoState.SIGNIN:
                if (accountEmail == null
                        && seamlessSigninStringType
                                != SigninFeatureMap.SeamlessSigninStringType.NON_SEAMLESS) {
                    return mContext.getString(
                            R.string.signin_promo_description_recent_tabs_no_accounts);
                }
                if (seamlessSigninStringType
                        == SigninFeatureMap.SeamlessSigninStringType.CONTINUE_BUTTON) {
                    if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
                        return mContext.getString(
                                R.string.signin_promo_description_recent_tabs_group1, accountEmail);
                    } else if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                        return mContext.getString(
                                R.string.signin_promo_description_recent_tabs_group2);
                    }
                } else if (seamlessSigninStringType
                        == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
                    if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
                        return mContext.getString(
                                R.string.signin_promo_description_recent_tabs_group3, accountEmail);
                    } else if (seamlessSigninPromoType
                            == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                        return mContext.getString(
                                R.string.signin_promo_description_recent_tabs_group4);
                    }
                }
                return mContext.getString(R.string.signin_promo_description_recent_tabs);
            case PromoState.HISTORY_SYNC:
                return mContext.getString(R.string.signin_promo_description_recent_tabs);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
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
    boolean isSeamlessSigninAllowed() {
        return SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
    }

    @Override
    boolean shouldHideSecondaryButton() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            return mPromoState != PromoState.SIGNIN;
        }
        return true;
    }

    @Override
    boolean shouldHideDismissButton() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            return mPromoState != PromoState.SIGNIN;
        }
        return true;
    }

    @Override
    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        switch (mPromoState) {
            case PromoState.SIGNIN:
                if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
                    return super.getTextForPrimaryButton(profileData);
                }
                return mContext.getString(R.string.signin_promo_turn_on);
            case PromoState.HISTORY_SYNC:
                return mContext.getString(R.string.signin_promo_turn_on);
            case PromoState.NONE:
            default:
                throw new IllegalStateException("Forbidden promo type: " + mPromoState);
        }
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

    @Override
    boolean shouldDisplaySignedInLayout() {
        return mPromoState == PromoState.HISTORY_SYNC;
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
        if (!historySyncHelper.shouldDisplayHistorySync()) {
            return PromoState.NONE;
        }
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
                return PromoState.HISTORY_SYNC;
        }
        return PromoState.SIGNIN;
    }
}
