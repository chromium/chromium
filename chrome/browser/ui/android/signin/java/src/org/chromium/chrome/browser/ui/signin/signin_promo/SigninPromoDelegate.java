// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** A delegate object that provides necessary information to customize sign-in promo. */
public class SigninPromoDelegate {
    /* Provides primary button text for the sign-in promo. */
    public interface PrimaryButtonTextProvider {
        String get(Context context, @Nullable DisplayableProfileData profileData);
    }

    private final Context mContext;
    private final Profile mProfile;
    private final @SigninAndHistorySyncActivityLauncher.AccessPoint int mAccessPoint;
    private final @HistorySyncConfig.OptInMode int mHistoryOptInMode;
    private final @StringRes int mTitle;
    private final @StringRes int mDescription;
    private final boolean mShouldHideSecondaryButton;
    private final boolean mShouldHideDismissButton;
    private final PrimaryButtonTextProvider mPrimaryButtonTextProvider;
    private final AccountPickerBottomSheetStrings mBottomSheetStrings;
    private final SigninAndHistorySyncActivityLauncher mLauncher;

    /* Delegate for sign-in promo in bookmark manager. */
    public static SigninPromoDelegate forBookmarkManager(
            Context context, Profile profile, SigninAndHistorySyncActivityLauncher launcher) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        return new SigninPromoDelegate(
                context,
                profile,
                SigninAccessPoint.BOOKMARK_MANAGER,
                HistorySyncConfig.OptInMode.NONE,
                R.string.signin_promo_title_bookmarks,
                R.string.signin_promo_description_bookmarks,
                /* shouldHideSecondaryButton= */ false,
                /* shouldHideDismissButton= */ false,
                SigninPromoDelegate::primaryButtonTextForBookmarkAndNtp,
                bottomSheetStrings,
                launcher);
    }

    /* Delegate for sign-in promo in ntp feed top promo. */
    public static SigninPromoDelegate forNtpFeedTopPromo(
            Context context, Profile profile, SigninAndHistorySyncActivityLauncher launcher) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        return new SigninPromoDelegate(
                context,
                profile,
                SigninAccessPoint.NTP_FEED_TOP_PROMO,
                HistorySyncConfig.OptInMode.NONE,
                R.string.signin_promo_title_ntp_feed_top_promo,
                R.string.signin_promo_description_ntp_feed_top_promo,
                /* shouldHideSecondaryButton= */ false,
                /* shouldHideDismissButton= */ false,
                SigninPromoDelegate::primaryButtonTextForBookmarkAndNtp,
                bottomSheetStrings,
                launcher);
    }

    /* Delegate for sign-in promo in recent tabs. */
    public static SigninPromoDelegate forRecentTabs(
            Context context, Profile profile, SigninAndHistorySyncActivityLauncher launcher) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        return new SigninPromoDelegate(
                context,
                profile,
                SigninAccessPoint.RECENT_TABS,
                HistorySyncConfig.OptInMode.REQUIRED,
                R.string.signin_promo_title_recent_tabs,
                R.string.signin_promo_description_recent_tabs,
                /* shouldHideSecondaryButton= */ true,
                /* shouldHideDismissButton= */ true,
                (context1, profileData) -> context1.getString(R.string.signin_promo_turn_on),
                bottomSheetStrings,
                launcher);
    }

    public String getTitle() {
        return mContext.getString(mTitle);
    }

    public String getDescription() {
        return mContext.getString(mDescription);
    }

    public boolean shouldHideSecondaryButton() {
        return mShouldHideSecondaryButton;
    }

    public boolean shouldHideDismissButton() {
        return mShouldHideDismissButton;
    }

    public String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        return mPrimaryButtonTextProvider.get(mContext, profileData);
    }

    public String getTextForSecondaryButton() {
        return mContext.getString(R.string.signin_promo_choose_another_account);
    }

    public void onPrimaryButtonClicked() {
        mLauncher.createBottomSheetSigninIntentOrShowError(
                mContext,
                mProfile,
                mBottomSheetStrings,
                BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                        .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                mHistoryOptInMode,
                mAccessPoint,
                /* selectedCoreAccountId= */ null);
    }

    public void onSecondaryButtonClicked() {
        assert !mShouldHideSecondaryButton;
        mLauncher.createBottomSheetSigninIntentOrShowError(
                mContext,
                mProfile,
                mBottomSheetStrings,
                BottomSheetSigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                BottomSheetSigninAndHistorySyncCoordinator.WithAccountSigninMode
                        .CHOOSE_ACCOUNT_BOTTOM_SHEET,
                mHistoryOptInMode,
                mAccessPoint,
                /* selectedCoreAccountId= */ null);
    }

    private static String primaryButtonTextForBookmarkAndNtp(
            Context context, @Nullable DisplayableProfileData profileData) {
        return profileData == null
                ? context.getString(R.string.signin_promo_signin)
                : SigninUtils.getContinueAsButtonText(context, profileData);
    }

    private SigninPromoDelegate(
            Context context,
            Profile profile,
            @SigninAndHistorySyncActivityLauncher.AccessPoint int accessPoint,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @StringRes int title,
            @StringRes int description,
            boolean shouldHideSecondaryButton,
            boolean shouldHideDismissButton,
            PrimaryButtonTextProvider provider,
            AccountPickerBottomSheetStrings bottomSheetStrings,
            SigninAndHistorySyncActivityLauncher launcher) {
        mContext = context;
        mProfile = profile;
        mAccessPoint = accessPoint;
        mHistoryOptInMode = historyOptInMode;
        mTitle = title;
        mDescription = description;
        mShouldHideSecondaryButton = shouldHideSecondaryButton;
        mShouldHideDismissButton = shouldHideDismissButton;
        mPrimaryButtonTextProvider = provider;
        mBottomSheetStrings = bottomSheetStrings;
        mLauncher = launcher;
    }
}
