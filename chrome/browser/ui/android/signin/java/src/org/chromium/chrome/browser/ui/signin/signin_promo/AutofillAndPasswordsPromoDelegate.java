// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

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
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** {@link SigninPromoDelegate} for the Autofill and Passwords sign-in promo. */
@NullMarked
public class AutofillAndPasswordsPromoDelegate extends SigninPromoDelegate {

    /** Indicates the type of content that should be shown in the visible promo. */
    @IntDef({PromoState.NONE, PromoState.SIGNIN})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PromoState {
        /** No promo should be shown. */
        int NONE = 0;

        /** The promo content should promote sign-in. Shown to signed-out user. */
        int SIGNIN = 1;
    }

    public static final int MAX_IMPRESSIONS = 20;

    private final String mPromoShowCountPreferenceName;
    private @PromoState int mPromoState = PromoState.NONE;

    public AutofillAndPasswordsPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange) {
        super(context, profile, launcher, onPromoStateChange);

        mPromoShowCountPreferenceName =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.AUTOFILL_AND_PASSWORDS);
    }

    @Override
    String getTitle() {
        return mContext.getString(R.string.signin_promo_title_autofill_and_passwords);
    }

    @Override
    String getDescription(@Nullable String accountEmail) {
        return mContext.getString(R.string.signin_promo_description_autofill_and_passwords);
    }

    @Override
    @SigninPreferencesManager.SigninPromoAccessPointId
    String getAccessPointName() {
        return SigninPreferencesManager.SigninPromoAccessPointId.AUTOFILL_AND_PASSWORDS;
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.SETTINGS_AUTOFILL_AND_PASSWORDS;
    }

    @Override
    void permanentlyDismissPromo() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS_DISMISSED, true);
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
    void recordImpression() {
        ChromeSharedPreferences.getInstance().incrementInt(mPromoShowCountPreferenceName);
    }

    @Override
    int getPromoShownCount() {
        return ChromeSharedPreferences.getInstance().readInt(mPromoShowCountPreferenceName);
    }

    @Override
    boolean isMaxImpressionsReached() {
        return getPromoShownCount() >= MAX_IMPRESSIONS;
    }

    @Override
    @ColorInt
    int getAccountPickerBackgroundColor() {
        return SemanticColorUtils.getColorSurface(mContext);
    }

    @Override
    boolean isSeamlessSigninAllowed() {
        // TODO (crbug.com/482994749): Support seamless Sign-in.
        return false;
    }

    private @PromoState int computePromoState() {
        if (wasPromoDismissed() || isMaxImpressionsReached()) {
            return PromoState.NONE;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        if (identityManager.hasPrimaryAccount()) {
            return PromoState.NONE;
        }

        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        assumeNonNull(signinManager);
        return signinManager.isSigninAllowed() ? PromoState.SIGNIN : PromoState.NONE;
    }

    private boolean wasPromoDismissed() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(
                        ChromePreferenceKeys.SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS_DISMISSED, false);
    }
}
