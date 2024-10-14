// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninUtils;

/** A delegate object that provides necessary information to customize sign-in promo. */
public class SigninPromoDelegate {
    /* Provides primary button text for the sign-in promo. */
    public interface PrimaryButtonTextProvider {
        String get(Context context, @Nullable DisplayableProfileData profileData);
    }

    private final Context mContext;
    private final @StringRes int mTitle;
    private final @StringRes int mDescription;
    private final boolean mShouldHideSecondaryButton;
    private final boolean mShouldHideDismissButton;
    private final PrimaryButtonTextProvider mPrimaryButtonTextProvider;

    /* Delegate for sign-in promo in bookmark manager. */
    public static SigninPromoDelegate forBookmarkManager(Context context) {
        return new SigninPromoDelegate(
                context,
                R.string.signin_promo_title_bookmarks,
                R.string.signin_promo_description_bookmarks,
                /* shouldHideSecondaryButton= */ false,
                /* shouldHideDismissButton= */ false,
                SigninPromoDelegate::primaryButtonTextForBookmarkAndNtp);
    }

    /* Delegate for sign-in promo in ntp feed top promo. */
    public static SigninPromoDelegate forNtpFeedTopPromo(Context context) {
        return new SigninPromoDelegate(
                context,
                R.string.signin_promo_title_ntp_feed_top_promo,
                R.string.signin_promo_description_ntp_feed_top_promo,
                /* shouldHideSecondaryButton= */ false,
                /* shouldHideDismissButton= */ false,
                SigninPromoDelegate::primaryButtonTextForBookmarkAndNtp);
    }

    /* Delegate for sign-in promo in recent tabs. */
    public static SigninPromoDelegate forRecentTabs(Context context) {
        return new SigninPromoDelegate(
                context,
                R.string.signin_promo_title_recent_tabs,
                R.string.signin_promo_description_recent_tabs,
                /* shouldHideSecondaryButton= */ true,
                /* shouldHideDismissButton= */ true,
                (context1, profileData) -> context1.getString(R.string.signin_promo_turn_on));
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

    private static String primaryButtonTextForBookmarkAndNtp(
            Context context, @Nullable DisplayableProfileData profileData) {
        return profileData == null
                ? context.getResources().getString(R.string.signin_promo_signin)
                : SigninUtils.getContinueAsButtonText(context, profileData);
    }

    private SigninPromoDelegate(
            Context context,
            @StringRes int title,
            @StringRes int description,
            boolean shouldHideSecondaryButton,
            boolean shouldHideDismissButton,
            PrimaryButtonTextProvider provider) {
        mContext = context;
        mTitle = title;
        mDescription = description;
        mShouldHideSecondaryButton = shouldHideSecondaryButton;
        mShouldHideDismissButton = shouldHideDismissButton;
        mPrimaryButtonTextProvider = provider;
    }
}
