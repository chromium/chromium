// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

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
import org.chromium.chrome.browser.ui.signin.SigninSurveyController;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A delegate object that provides necessary information to customize sign-in promo. */
@NullMarked
public abstract class SigninPromoDelegate {
    protected final Context mContext;
    protected final Profile mProfile;
    protected final SigninAndHistorySyncActivityLauncher mLauncher;
    protected final Runnable mOnPromoVisibilityChange;
    protected @PromoLoadingState int mPromoLoadingState = PromoLoadingState.NOT_LOADING;

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

    /** Indicates the loading state of the seamless sign-in promo. */
    @IntDef({PromoLoadingState.NOT_LOADING, PromoLoadingState.LOADING})
    @Retention(RetentionPolicy.SOURCE)
    @interface PromoLoadingState {
        int NOT_LOADING = 0;
        int LOADING = 1;
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
    abstract void permanentlyDismissPromo();

    /**
     * Whether the promo can be shown.
     *
     * <p>If a condition affecting the promo's content changes, refreshPromoState should be called
     * before calling this method.
     */
    abstract boolean canShowPromo();

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

    /** Returns the background color for the account picker in seamless sign-in layout `compact` */
    abstract @ColorInt int getAccountPickerBackgroundColor();

    /** Returns whether this entry point supports seamless sign-in. */
    boolean isSeamlessSigninAllowed() {
        return SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
    }

    AccountPickerBottomSheetStrings getBottomSheetStrings() {
        return new AccountPickerBottomSheetStrings.Builder(
                        mContext.getString(R.string.signin_account_picker_bottom_sheet_title))
                .build();
    }

    boolean shouldHideSecondaryButton() {
        return false;
    }

    boolean canBeDismissedPermanently() {
        return true;
    }

    boolean shouldDisplaySignedInLayout() {
        return false;
    }

    String getTextForPrimaryButton(@Nullable DisplayableProfileData profileData) {
        @SigninFeatureMap.SeamlessSigninStringType
        int seamlessSigninStringType = SigninFeatureMap.getInstance().getSeamlessSigninStringType();
        if (mPromoLoadingState == PromoLoadingState.LOADING
                && seamlessSigninStringType
                        != SigninFeatureMap.SeamlessSigninStringType.NON_SEAMLESS) {
            return mContext.getString(R.string.signin_account_picker_bottom_sheet_signin_title);
        }
        if (seamlessSigninStringType == SigninFeatureMap.SeamlessSigninStringType.CONTINUE_BUTTON) {
            if (profileData != null && !TextUtils.isEmpty(profileData.getGivenName())) {
                return mContext.getString(
                        R.string.sync_promo_continue_as, profileData.getGivenName());
            }
            if (profileData != null && !TextUtils.isEmpty(profileData.getFullName())) {
                return mContext.getString(
                        R.string.sync_promo_continue_as, profileData.getFullName());
            }
            return mContext.getString(R.string.sync_promo_continue);
        } else if (seamlessSigninStringType
                == SigninFeatureMap.SeamlessSigninStringType.SIGNIN_BUTTON) {
            if (profileData != null && !TextUtils.isEmpty(profileData.getGivenName())) {
                return mContext.getString(
                        R.string.signin_promo_sign_in_as, profileData.getGivenName());
            }
            if (profileData != null && !TextUtils.isEmpty(profileData.getFullName())) {
                return mContext.getString(
                        R.string.signin_promo_sign_in_as, profileData.getFullName());
            }
            return mContext.getString(R.string.signin_promo_sign_in);
        }
        if (profileData == null) {
            return mContext.getString(R.string.signin_promo_signin);
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

    /** Called by the mediator when the sign-in flow starts. */
    void onFlowStarted() {
        mPromoLoadingState = PromoLoadingState.LOADING;
    }

    /** Called by the mediator when the sign-in flow terminates (regardless of the outcome). */
    void onFlowCompleted() {
        mPromoLoadingState = PromoLoadingState.NOT_LOADING;
    }

    boolean shouldDisplayLoadingState() {
        return mPromoLoadingState == PromoLoadingState.LOADING;
    }

    /**
     * This primary button handler can, for instance, initiate a sign-in flow for signed-out users
     * or enable history and tabs sync for signed-in users, depending on the promo's context.
     *
     * @param visibleAccount The {@link CoreAccountInfo} of the account displayed in the promo, or
     *     {@code null} if no account is currently available on the device.
     */
    void onPrimaryButtonClicked(@Nullable CoreAccountInfo visibleAccount) {
        @Nullable Intent intent =
                mLauncher.createBottomSheetSigninIntentOrShowError(
                        mContext,
                        mProfile,
                        getConfigForPrimaryButtonClick(visibleAccount),
                        getAccessPoint());
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

        @Nullable Intent intent =
                mLauncher.createBottomSheetSigninIntentOrShowError(
                        mContext, mProfile, getConfigForSecondaryButtonClick(), getAccessPoint());
        if (intent != null) {
            mContext.startActivity(intent);
        }
    }

    void onPromoVisibilityChange() {
        mOnPromoVisibilityChange.run();
    }

    /** Returns a survey trigger if a signin survey should be shown after the promo. */
    @Nullable
    @SigninSurveyController.SigninSurveyType
    Integer getSurveyTriggerType() {
        return null;
    }

    // TODO(https://crbug.com/474294917): Remove this.
    /** Returns true if the delegate should handle the primary button click. */
    boolean shouldOverridePrimaryButtonClick() {
        return !SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
    }

    // TODO(https://crbug.com/474294917): Remove this.
    /** Returns true if the delegate should handle the secondary button click. */
    boolean shouldOverrideSecondaryButtonClick() {
        return !SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN);
    }

    /** Returns the configuration for the flow started by the secondary button. */
    BottomSheetSigninAndHistorySyncConfig getConfigForPrimaryButtonClick(
            @Nullable CoreAccountInfo visibleAccount) {
        return isSeamlessSigninAllowed() && visibleAccount != null
                ? getConfigForSeamlessSignin(visibleAccount)
                : getConfigForCollapsedBottomSheet();
    }

    /** Returns the configuration for the flow started by the secondary button. */
    BottomSheetSigninAndHistorySyncConfig getConfigForSecondaryButtonClick() {
        return getConfigForExpandedBottomSheet(isSeamlessSigninAllowed());
    }

    String getHistorySyncOptInTitle() {
        return mContext.getString(R.string.history_sync_title);
    }

    String getHistorySyncOptInSubtitle() {
        return mContext.getString(R.string.history_sync_subtitle);
    }

    private BottomSheetSigninAndHistorySyncConfig getConfigForCollapsedBottomSheet() {
        return getBaseConfigBuilder(WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET).build();
    }

    private BottomSheetSigninAndHistorySyncConfig getConfigForSeamlessSignin(
            CoreAccountInfo visibleAccount) {
        return getBaseConfigBuilder(WithAccountSigninMode.SEAMLESS_SIGNIN)
                .useSeamlessWithAccountSignin(visibleAccount.getId())
                .build();
    }

    private BottomSheetSigninAndHistorySyncConfig getConfigForExpandedBottomSheet(
            boolean shownSigninSnackbar) {
        return getBaseConfigBuilder(WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET)
                .shouldShowSigninSnackbar(shownSigninSnackbar)
                .build();
    }

    private BottomSheetSigninAndHistorySyncConfig.Builder getBaseConfigBuilder(
            @WithAccountSigninMode int mode) {
        @Nullable Integer surveyType = getSurveyTriggerType();
        BottomSheetSigninAndHistorySyncConfig.Builder config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                        getBottomSheetStrings(),
                        NoAccountSigninMode.BOTTOM_SHEET,
                        mode,
                        getHistoryOptInMode(),
                        getHistorySyncOptInTitle(),
                        getHistorySyncOptInSubtitle());
        if (surveyType != null) {
            config.signinSurveyType(surveyType);
        }
        return config;
    }
}
