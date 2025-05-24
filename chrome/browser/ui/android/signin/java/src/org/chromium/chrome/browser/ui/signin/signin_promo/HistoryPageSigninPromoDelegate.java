// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
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
import java.util.concurrent.TimeUnit;

/** {@link SigninPromoDelegate} for the History page sign-in promo. */
@NullMarked
public class HistoryPageSigninPromoDelegate extends SigninPromoDelegate {

    /** Indicates the type of content the should be shown in the visible promo. */
    @IntDef({PromoState.NONE, PromoState.HISTORY_SYNC})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /**
         * The promo content should promote history sync. Shown to signed-in user with history &
         * tabs sync disabled.
         */
        int HISTORY_SYNC = 1;
    }

    // 2 weeks in ms.
    @VisibleForTesting
    static final long MIN_DELAY_BETWEEN_IMPRESSIONS_MS = TimeUnit.DAYS.toMillis(14);

    @VisibleForTesting static final int MAX_IMPRESSIONS = 10;

    private final boolean mIsCreatedInCct;
    private final String mPromoShowCountPreferenceName;
    private @PromoState int mPromoState = PromoState.NONE;

    public HistoryPageSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange,
            boolean isCreatedInCct) {
        super(context, profile, launcher, onPromoStateChange);

        mIsCreatedInCct = isCreatedInCct;
        mPromoShowCountPreferenceName =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.HISTORY_PAGE);
    }

    @Override
    String getTitle() {
        return mContext.getString(R.string.signin_promo_title_history_page);
    }

    @Override
    String getDescription() {
        return mContext.getString(R.string.signin_promo_description_history_page);
    }

    @Override
    @SigninPreferencesManager.SigninPromoAccessPointId
    String getAccessPointName() {
        return SigninPreferencesManager.SigninPromoAccessPointId.HISTORY_PAGE;
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.HISTORY_PAGE;
    }

    @Override
    void onDismissButtonClicked() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_DECLINED, true);
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
    AccountPickerBottomSheetStrings getBottomSheetStrings() {
        return new AccountPickerBottomSheetStrings.Builder(
                        R.string.signin_account_picker_bottom_sheet_title)
                .setSubtitleStringId(R.string.signin_account_picker_bottom_sheet_benefits_subtitle)
                .build();
    }

    @Override
    boolean shouldHideSecondaryButton() {
        return true;
    }

    @Override
    boolean shouldHideDismissButton() {
        return false;
    }

    @Override
    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.HISTORY_PAGE_PROMO_CTA_STRING_VARIATION)) {
            return mContext.getString(R.string.signin_continue);
        } else {
            return mContext.getString(R.string.signin_promo_turn_on);
        }
    }

    @Override
    void recordImpression() {
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_LAST_SHOWN_TIME,
                        System.currentTimeMillis());
        ChromeSharedPreferences.getInstance().incrementInt(mPromoShowCountPreferenceName);
    }

    @Override
    boolean isMaxImpressionsReached() {
        return getPromoShownCount() >= MAX_IMPRESSIONS;
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
        // The history page promo should always be hidden in CCT for privacy concern: it's hard to
        // clarify that the history sync promo affect only Chrome and not the app that launched the
        // CCT.
        if (mIsCreatedInCct) {
            return PromoState.NONE;
        }

        if (ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_DECLINED, false)) {
            return PromoState.NONE;
        }

        final long currentTime = System.currentTimeMillis();
        final long lastShownTime =
                ChromeSharedPreferences.getInstance()
                        .readLong(
                                ChromePreferenceKeys.SIGNIN_PROMO_HISTORY_PAGE_LAST_SHOWN_TIME, 0L);
        if (mPromoState == PromoState.NONE
                && currentTime > lastShownTime
                && currentTime - lastShownTime < MIN_DELAY_BETWEEN_IMPRESSIONS_MS) {
            // Hide promo if it was last shown less than two weeks ago and is not currently showing.
            // (If the promo is already showing, the delay should have been checked before the promo
            // was first shown to the user, and a new last shown time should have been recorded
            // afterward.)
            return PromoState.NONE;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        if (!identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            // Hide promo for signed-out users.
            return PromoState.NONE;
        }
        final HistorySyncHelper historySyncHelper = HistorySyncHelper.getForProfile(mProfile);
        return historySyncHelper.shouldSuppressHistorySync() || historySyncHelper.isDeclinedOften()
                ? PromoState.NONE
                : PromoState.HISTORY_SYNC;
    }
}
