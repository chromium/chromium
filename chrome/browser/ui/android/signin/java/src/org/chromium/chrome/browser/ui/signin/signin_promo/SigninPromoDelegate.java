// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.base.CoreAccountInfo;

/** A delegate object that provides necessary information to customize sign-in promo. */
public abstract class SigninPromoDelegate {
    protected final Context mContext;
    protected final Profile mProfile;
    protected final SigninAndHistorySyncActivityLauncher mLauncher;
    protected final Runnable mOnPromoVisibilityChange;
    private final AccountPickerBottomSheetStrings mBottomSheetStrings;

    protected SigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoVisibilityChange) {
        mContext = context;
        mProfile = profile;
        mLauncher = launcher;
        mOnPromoVisibilityChange = onPromoVisibilityChange;
        mBottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
    }

    /** Returns the title string for the promo. */
    abstract String getTitle();

    /** Returns the description string for the promo. */
    abstract String getDescription();

    /**
     * Returns the {@link SigninAndHistorySyncActivityLauncher.AccessPoint} that will be used for
     * sign-in for the promo.
     */
    abstract @SigninAndHistorySyncActivityLauncher.AccessPoint int getAccessPoint();

    /**
     * Called when dismiss button is clicked. Subclasses that want to hide promos in the future can
     * do it here.
     */
    abstract void onDismissButtonClicked();

    /**
     * Whether the promo can be shown. Subclasses should implement their particular show logic here.
     */
    abstract boolean canShowPromo(@Nullable CoreAccountInfo visibleAccount);

    boolean shouldHideSecondaryButton() {
        return false;
    }

    boolean shouldHideDismissButton() {
        return false;
    }

    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        return profileData == null
                ? mContext.getString(R.string.signin_promo_signin)
                : SigninUtils.getContinueAsButtonText(mContext, profileData);
    }

    String getTextForSecondaryButton() {
        return mContext.getString(R.string.signin_promo_choose_another_account);
    }

    @HistorySyncConfig.OptInMode
    int getHistoryOptInMode() {
        return HistorySyncConfig.OptInMode.NONE;
    }

    /** Subclasses that want to record impression counts should do them here. */
    void recordImpression() {}

    boolean isMaxImpressionsReached() {
        return false;
    }

    void onPrimaryButtonClicked() {
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                mBottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                getHistoryOptInMode())
                        .build();
        @Nullable
        Intent intent =
                mLauncher.createBottomSheetSigninIntentOrShowError(
                        mContext, mProfile, config, getAccessPoint());
        if (intent != null) {
            mContext.startActivity(intent);
        }
    }

    void onSecondaryButtonClicked() {
        assert !shouldHideSecondaryButton();

        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                mBottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                                getHistoryOptInMode())
                        .build();
        @Nullable
        Intent intent =
                mLauncher.createBottomSheetSigninIntentOrShowError(
                        mContext, mProfile, config, getAccessPoint());
        if (intent != null) {
            mContext.startActivity(intent);
        }
    }

    void onPromoVisibilityChange() {
        mOnPromoVisibilityChange.run();
    }
}
