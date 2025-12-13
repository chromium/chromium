// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.base.CoreAccountInfo;

/** A delegate object that provides necessary information to customize sign-in promo. */
@NullMarked
public abstract class SigninPromoDelegate {
    protected final Context mContext;
    protected final Profile mProfile;
    protected final SigninAndHistorySyncActivityLauncher mLauncher;
    protected final Runnable mOnPromoVisibilityChange;

    protected SigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoVisibilityChange) {
        mContext = context;
        mProfile = profile;
        mLauncher = launcher;
        mOnPromoVisibilityChange = onPromoVisibilityChange;
    }

    /** Returns the title string for the promo. */
    abstract String getTitle();

    /** Returns the description string for the promo. */
    abstract String getDescription(@Nullable String accountEmail);

    /** Returns the access point name to be recorded in promo histograms. */
    abstract @SigninPreferencesManager.SigninPromoAccessPointId String getAccessPointName();

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
     * Whether the promo can be shown.
     *
     * <p>If a condition affecting the promo's content changes, refreshPromoState should be called
     * before calling this method.
     */
    abstract boolean canShowPromo();

    /** Returns whether this entry point supports seamless sign-in. */
    abstract boolean isSeamlessSigninAllowed();

    /** Returns the number of times where the promo is shown to the user, */
    abstract int getPromoShownCount();

    /**
     * Refresh the promo state including its content and visibility. This method is invoked by
     * SigninPromoMediator whenever observed state affecting promo content/visibility is updated
     * (e.g. the primary account, sync data types...).
     *
     * @param visibleAccount The account currently shown in the promo.
     * @return Whether the promo state has changed during the refresh. If it returns true, {@link
     *     SigninPromoCoordinator} will refresh the promo visibility and the whole promo content
     *     (e.g. title, description, buttons...) for a visible promo, by updating the promo's model
     *     with new values retrieved from the delegate.
     */
    abstract boolean refreshPromoState(@Nullable CoreAccountInfo visibleAccount);

    AccountPickerBottomSheetStrings getBottomSheetStrings() {
        return new AccountPickerBottomSheetStrings.Builder(
                        mContext.getString(R.string.signin_account_picker_bottom_sheet_title))
                .build();
    }

    boolean shouldHideSecondaryButton() {
        return false;
    }

    boolean shouldHideDismissButton() {
        return false;
    }

    boolean shouldDisplaySignedInLayout() {
        return false;
    }

    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        if (profileData == null) {
            if (seamlessSigninStringType
                    == SigninFeatureMap.SeamlessSigninStringType.NON_SEAMLESS) {
                return mContext.getString(R.string.signin_promo_signin);
            }
            return mContext.getString(R.string.sign_in_to_chrome);
        }
        if (seamlessSigninStringType == SigninFeatureMap.SeamlessSigninStringType.CONTINUE_BUTTON) {
            if (!TextUtils.isEmpty(profileData.getGivenName())) {
                return mContext.getString(
                        R.string.sync_promo_continue_as, profileData.getGivenName());
            }
            if (!TextUtils.isEmpty(profileData.getFullName())) {
                return mContext.getString(
                        R.string.sync_promo_continue_as, profileData.getFullName());
            }
            return mContext.getString(R.string.sync_promo_continue);
        } else if (seamlessSigninStringType
                == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
            if (!TextUtils.isEmpty(profileData.getGivenName())) {
                return mContext.getString(
                        R.string.signin_promo_sign_in_as, profileData.getGivenName());
            }
            if (!TextUtils.isEmpty(profileData.getFullName())) {
                return mContext.getString(
                        R.string.signin_promo_sign_in_as, profileData.getFullName());
            }
            return mContext.getString(R.string.signin_promo_sign_in);
        }
        return SigninUtils.getContinueAsButtonText(mContext, profileData);
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

    /**
     * This primary button handler can, for instance, initiate a sign-in flow for signed-out users
     * or enable history and tabs sync for signed-in users, depending on the promo's context.
     *
     * @param visibleAccount The {@link CoreAccountInfo} of the account displayed in the promo, or
     *     {@code null} if no account is currently available on the device.
     */
    void onPrimaryButtonClicked(@Nullable CoreAccountInfo visibleAccount) {
        BottomSheetSigninAndHistorySyncConfig config =
                isSeamlessSigninAllowed() && visibleAccount != null
                        ? getConfigForSeamlessSignin(visibleAccount)
                        : getConfigForCollapsedBottomSheet();
        @Nullable Intent intent =
                mLauncher.createBottomSheetSigninIntentOrShowError(
                        mContext, mProfile, config, getAccessPoint());
        if (intent != null) {
            mContext.startActivity(intent);
        }
    }

    /**
     * This secondary button handler enables a signed-out user with an account on the device to
     * choose a different account for sign-in. It is typically hidden for promos shown to signed-in
     * users.
     */
    void onSecondaryButtonClicked() {
        assert !shouldHideSecondaryButton();

        BottomSheetSigninAndHistorySyncConfig config =
                getConfigForExpandedBottomSheet(isSeamlessSigninAllowed());
        @Nullable Intent intent =
                mLauncher.createBottomSheetSigninIntentOrShowError(
                        mContext, mProfile, config, getAccessPoint());
        if (intent != null) {
            mContext.startActivity(intent);
        }
    }

    void onPromoVisibilityChange() {
        mOnPromoVisibilityChange.run();
    }

    private BottomSheetSigninAndHistorySyncConfig getConfigForCollapsedBottomSheet() {
        return new BottomSheetSigninAndHistorySyncConfig.Builder(
                        getBottomSheetStrings(),
                        NoAccountSigninMode.BOTTOM_SHEET,
                        WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        getHistoryOptInMode(),
                        mContext.getString(R.string.history_sync_title),
                        mContext.getString(R.string.history_sync_subtitle))
                .build();
    }

    private BottomSheetSigninAndHistorySyncConfig getConfigForSeamlessSignin(
            CoreAccountInfo visibleAccount) {
        return new BottomSheetSigninAndHistorySyncConfig.Builder(
                        getBottomSheetStrings(),
                        NoAccountSigninMode.BOTTOM_SHEET,
                        WithAccountSigninMode.SEAMLESS_SIGNIN,
                        getHistoryOptInMode(),
                        mContext.getString(R.string.history_sync_title),
                        mContext.getString(R.string.history_sync_subtitle))
                .useSeamlessWithAccountSignin(visibleAccount.getId())
                .build();
    }

    private BottomSheetSigninAndHistorySyncConfig getConfigForExpandedBottomSheet(
            boolean shownSigninSnackbar) {
        return new BottomSheetSigninAndHistorySyncConfig.Builder(
                        getBottomSheetStrings(),
                        NoAccountSigninMode.BOTTOM_SHEET,
                        WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                        getHistoryOptInMode(),
                        mContext.getString(R.string.history_sync_title),
                        mContext.getString(R.string.history_sync_subtitle))
                .shouldShowSigninSnackbar(shownSigninSnackbar)
                .build();
    }
}
