// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** {@link SigninPromoDelegate} for ntp signin promo. */
@NullMarked
public class NtpSigninPromoDelegate extends SigninPromoDelegate {

    /** Indicates the type of content the should be shown in the visible promo. */
    @IntDef({PromoState.NONE, PromoState.SIGNIN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /** The promo content should promote sign-in. Shown to signed-out user. */
        int SIGNIN = 1;
    }

    @VisibleForTesting static final int MAX_IMPRESSIONS_NTP = 5;
    // 14 days in hours.
    @VisibleForTesting static final int NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS = 336;
    @VisibleForTesting static final int NTP_SYNC_PROMO_RESET_AFTER_DAYS = 30;
    private static final int NTP_SYNC_PROMO_INCREASE_SHOW_COUNT_AFTER_MINUTES = 30;
    private @PromoState int mPromoState = PromoState.NONE;

    /**
     * If the signin promo card has been hidden for longer than the {@link
     * #NTP_SYNC_PROMO_RESET_AFTER_DAYS}, resets the impression counts, {@link
     * ChromePreferenceKeys#SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME} and {@link
     * ChromePreferenceKeys#SIGNIN_PROMO_NTP_LAST_SHOWN_TIME} to allow the promo card to show again.
     */
    public static void resetNtpSyncPromoLimitsIfHiddenForTooLong() {
        final long currentTime = System.currentTimeMillis();
        final long resetAfterMs = NTP_SYNC_PROMO_RESET_AFTER_DAYS * DateUtils.DAY_IN_MILLIS;
        final long lastShownTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
        if (lastShownTime <= 0) return;

        if (currentTime - lastShownTime >= resetAfterMs) {
            ChromeSharedPreferences.getInstance().writeInt(getPromoShowCountPreferenceName(), 0);
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME);
            ChromeSharedPreferences.getInstance()
                    .removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME);
        }
    }

    public NtpSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange) {
        super(context, profile, launcher, onPromoStateChange);
    }

    @Override
    String getTitle() {
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        if (seamlessSigninStringType == SigninFeatureMap.SeamlessSigninStringType.NON_SEAMLESS) {
            return mContext.getString(R.string.signin_promo_title_ntp_feed_top_promo);
        }
        if (seamlessSigninStringType == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
            // We reuse the same title string as in the case with the "Sign in as" button
            return mContext.getString(R.string.signin_promo_title_ntp_sign_in_as_button);
        }
        return mContext.getString(R.string.signin_account_picker_bottom_sheet_title);
    }

    @Override
    String getDescription(@Nullable String accountEmail) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            if (accountEmail == null) {
                return mContext.getString(R.string.custom_tabs_signed_out_message_subtitle);
            }
            @SigninFeatureMap.SeamlessSigninPromoType
            int seamlessSigninPromoType =
                    SigninFeatureMap.getInstance().getSeamlessSigninPromoType();
            @SigninFeatureMap.SeamlessSigninStringType
            int seamlessSigninStringType =
                    SigninFeatureMap.getInstance().getSeamlessSigninStringType();
            if (seamlessSigninStringType
                    == SigninFeatureMap.SeamlessSigninStringType.CONTINUE_BUTTON) {
                if (seamlessSigninPromoType
                        == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
                    return mContext.getString(
                            R.string.signin_promo_description_ntp_group1, accountEmail);
                } else if (seamlessSigninPromoType
                        == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                    return mContext.getString(R.string.signin_promo_description_ntp_group2);
                }
            } else if (seamlessSigninStringType
                    == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
                if (seamlessSigninPromoType
                        == SigninFeatureMap.SeamlessSigninPromoType.TWO_BUTTONS) {
                    return mContext.getString(
                            R.string.signin_promo_description_ntp_group3, accountEmail);
                } else if (seamlessSigninPromoType
                        == SigninFeatureMap.SeamlessSigninPromoType.COMPACT) {
                    return mContext.getString(R.string.signin_promo_description_ntp_group4);
                }
            }
        }
        return mContext.getString(R.string.signin_promo_description_ntp_feed_top_promo);
    }

    @Override
    @SigninPreferencesManager.SigninPromoAccessPointId
    String getAccessPointName() {
        return SigninPreferencesManager.SigninPromoAccessPointId.NTP;
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.NTP_FEED_TOP_PROMO;
    }

    @Override
    void onDismissButtonClicked() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);
    }

    @Override
    boolean canShowPromo() {
        return mPromoState != PromoState.NONE;
    }

    @Override
    boolean refreshPromoState(@Nullable CoreAccountInfo visibleAccount) {
        @PromoState int newState = computePromoState(visibleAccount);
        boolean wasStateChanged = mPromoState != newState;
        mPromoState = newState;
        return wasStateChanged;
    }

    @Override
    boolean isSeamlessSigninAllowed() {
        return SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
    }

    @Override
    void recordImpression() {
        final long currentTime = System.currentTimeMillis();
        final long lastShownTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
        if (currentTime - lastShownTime
                < NTP_SYNC_PROMO_INCREASE_SHOW_COUNT_AFTER_MINUTES * DateUtils.MINUTE_IN_MILLIS) {
            return;
        }
        if (ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME)
                == 0) {
            ChromeSharedPreferences.getInstance()
                    .writeLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, currentTime);
        }
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, currentTime);
        ChromeSharedPreferences.getInstance().incrementInt(getPromoShowCountPreferenceName());
    }

    @Override
    boolean isMaxImpressionsReached() {
        return getPromoShownCount() >= MAX_IMPRESSIONS_NTP;
    }

    @Override
    int getPromoShownCount() {
        return ChromeSharedPreferences.getInstance().readInt(getPromoShowCountPreferenceName());
    }

    private static boolean timeElapsedSinceFirstShownExceedsLimit() {
        final long timeSinceFirstShownLimitMs =
                NTP_SYNC_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS * DateUtils.HOUR_IN_MILLIS;
        final long currentTime = System.currentTimeMillis();
        final long firstShownTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, 0L);
        return firstShownTime > 0 && currentTime - firstShownTime >= timeSinceFirstShownLimitMs;
    }

    private static String getPromoShowCountPreferenceName() {
        return ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                SigninPreferencesManager.SigninPromoAccessPointId.NTP);
    }

    private @PromoState int computePromoState(@Nullable CoreAccountInfo visibleAccount) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assumeNonNull(signinManager);
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                || !signinManager.isSigninAllowed()) {
            return PromoState.NONE;
        }

        boolean isPromoDismissed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);
        if (isPromoDismissed) {
            return PromoState.NONE;
        }
        if (timeElapsedSinceFirstShownExceedsLimit()) {
            return PromoState.NONE;
        }
        if (visibleAccount == null) {
            return PromoState.SIGNIN;
        }
        // Don't show the promo if account image is not available yet.
        return identityManager.findExtendedAccountInfoByEmailAddress(visibleAccount.getEmail())
                        == null
                ? PromoState.NONE
                : PromoState.SIGNIN;
    }
}
