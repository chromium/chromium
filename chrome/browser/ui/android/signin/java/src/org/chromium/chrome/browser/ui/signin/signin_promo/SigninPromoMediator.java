// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import androidx.annotation.StringDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.ProfileDataUtils;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninPromoAction;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

@NullMarked
final class SigninPromoMediator
        implements IdentityManager.Observer,
                SyncService.SyncStateChangedListener,
                ProfileDataCache.Observer {
    private static final int MAX_TOTAL_PROMO_SHOW_COUNT = 100;

    /** Strings used for promo event count histograms. */
    // LINT.IfChange(Event)
    @StringDef({Event.CONTINUED, Event.DISMISSED, Event.SIGNIN_UNDONE, Event.SHOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface Event {
        String CONTINUED = "Continued";
        String DISMISSED = "Dismissed";
        String SIGNIN_UNDONE = "SigninUndone";
        String SHOWN = "Shown";
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/signin/histograms.xml:SigninPromoAction)

    /**
     * The delegate interface for {@link SigninPromoMediator}. Provides methods for navigation
     * actions started by the promo.
     */
    interface Delegate {
        /**
         * Starts the sign-in flow with the given configuration.
         *
         * @param config The configuration for the sign-in and history sync opt-in flow.
         */
        void startSigninFlow(BottomSheetSigninAndHistorySyncConfig config);
    }

    private final IdentityManager mIdentityManager;
    private final SigninManager mSigninManager;
    private final @Nullable SyncService mSyncService;
    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoDelegate mPromoDelegate;
    private final Delegate mMediatorDelegate;
    private final PropertyModel mModel;
    private final boolean mMaxImpressionReached;

    private boolean mShouldShowPromo;
    private boolean mWasImpressionRecorded;

    SigninPromoMediator(
            IdentityManager identityManager,
            SigninManager signinManager,
            @Nullable SyncService syncService,
            ProfileDataCache profileDataCache,
            SigninPromoDelegate promoDelegate,
            Delegate mediatorDelegate) {
        mIdentityManager = identityManager;
        mSigninManager = signinManager;
        mSyncService = syncService;
        mProfileDataCache = profileDataCache;
        mPromoDelegate = promoDelegate;
        mMediatorDelegate = mediatorDelegate;

        DisplayableProfileData visibleAccount = getVisibleAccount();

        mModel =
                SigninPromoProperties.createModel(
                        /* profileData= */ visibleAccount,
                        /* onPrimaryButtonClicked= */ () -> {},
                        /* onSecondaryButtonClicked= */ () -> {},
                        /* onDismissButtonClicked= */ () -> {},
                        /* titleString= */ "",
                        /* descriptionString= */ "",
                        /* primaryButtonString= */ "",
                        /* secondaryButtonString= */ "",
                        /* shouldSuppressSecondaryButton= */ false,
                        /* shouldHideDismissButton= */ false,
                        /* shouldShowAccountPicker= */ true,
                        /* shouldShowHeaderWithAvatar= */ false,
                        /* shouldShowLoadingState= */ false,
                        /* accountPickerBackground= */ mPromoDelegate
                                .getAccountPickerBackgroundColor());
        mMaxImpressionReached = mPromoDelegate.isMaxImpressionsReached();
        mPromoDelegate.refreshPromoState(visibleAccount);
        mShouldShowPromo = canShowPromo();
        if (mShouldShowPromo) {
            updateModel(visibleAccount);
        }

        mIdentityManager.addObserver(this);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }
        mProfileDataCache.addObserver(this);
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }
        mIdentityManager.removeObserver(this);
    }

    void recordImpression() {
        if (mWasImpressionRecorded) {
            // Impressions are recorded only once per coordinator lifecycle.
            return;
        }
        @SigninPromoAction
        int promoAction =
                getVisibleAccount() == null
                        ? SigninPromoAction.NEW_ACCOUNT_NO_EXISTING_ACCOUNT
                        : SigninPromoAction.WITH_DEFAULT;
        SigninMetricsUtils.logSigninOffered(promoAction, mPromoDelegate.getAccessPoint());

        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        recordEventHistogram(Event.SHOWN);
        mPromoDelegate.recordImpression();
        mWasImpressionRecorded = true;
    }

    boolean canShowPromo() {
        if (!mProfileDataCache.getAccounts().isFulfilled()
                || !mSigninManager.didAccountsFetchSucceed()) {
            // If accounts are not available in ProfileDataCache yet, then don't shown the
            // promo.
            return false;
        }

        return !mMaxImpressionReached && mPromoDelegate.canShowPromo();
    }

    void onSigninUndone() {
        recordEventHistogram(Event.SIGNIN_UNDONE);
        if (mPromoDelegate.canBeDismissedPermanently()) {
            mPromoDelegate.permanentlyDismissPromo();
            refreshPromoContent(/* wasVisibleAccountUpdated= */ false);
        }
    }

    /** Implements {@link IdentityManager.Observer} */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        boolean wasVisibleAccountUpdated =
                eventDetails.getEventTypeFor() != PrimaryAccountChangeEvent.Type.NONE;
        refreshPromoContent(wasVisibleAccountUpdated);
    }

    /** Implements {@link SyncService.SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        refreshPromoContent(/* wasVisibleAccountUpdated= */ false);
    }

    /** Implements {@link ProfileDataCache.Observer} */
    @Override
    public void onAccountsUpdated(List<DisplayableProfileData> accounts) {
        refreshPromoContent(/* wasVisibleAccountUpdated= */ true);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        @Nullable DisplayableProfileData visibleAccount = getVisibleAccount();
        if (visibleAccount != null
                && !visibleAccount.getAccountId().equals(profileData.getAccountId())) {
            return;
        }
        refreshPromoContent(/* wasVisibleAccountUpdated= */ true);
    }

    /** Called when sign-in flow starts. */
    public void onFlowStarted() {
        mPromoDelegate.onFlowStarted();
        updateLoadingState();
    }

    /** Called when the sign-in flow terminates (regardless of the outcome). */
    public void onFlowCompleted() {
        mPromoDelegate.onFlowCompleted();
        updateLoadingState();
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onPrimaryButtonClicked(@Nullable DisplayableProfileData profileData) {
        recordEventHistogram(Event.CONTINUED);
        if (mPromoDelegate.shouldOverridePrimaryButtonClick()) {
            mPromoDelegate.onPrimaryButtonClicked(profileData);
        } else {
            mMediatorDelegate.startSigninFlow(
                    mPromoDelegate.getConfigForPrimaryButtonClick(profileData));
        }
    }

    private void onSecondaryButtonClicked() {
        recordEventHistogram(Event.CONTINUED);
        if (mPromoDelegate.shouldOverrideSecondaryButtonClick()) {
            mPromoDelegate.onSecondaryButtonClicked();
        } else {
            mMediatorDelegate.startSigninFlow(mPromoDelegate.getConfigForSecondaryButtonClick());
        }
    }

    private void onDismissButtonClicked() {
        assert mPromoDelegate.canBeDismissedPermanently();
        recordEventHistogram(Event.DISMISSED);
        mPromoDelegate.permanentlyDismissPromo();
        refreshPromoContent(/* wasVisibleAccountUpdated= */ false);
    }

    private void refreshPromoContent(boolean wasVisibleAccountUpdated) {
        boolean wasPromoContentChanged = mPromoDelegate.refreshPromoState(getVisibleAccount());
        if (wasPromoContentChanged) {
            updateVisibility();
        }
        if (mShouldShowPromo && (wasVisibleAccountUpdated || wasPromoContentChanged)) {
            updateModel(getVisibleAccount());
        }
    }

    private void updateModel(@Nullable DisplayableProfileData profileData) {
        mModel.set(SigninPromoProperties.PROFILE_DATA, profileData);
        mModel.set(
                SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON,
                profileData == null || mPromoDelegate.shouldHideSecondaryButton());
        mModel.set(
                SigninPromoProperties.ON_PRIMARY_BUTTON_CLICKED,
                (unusedView) -> onPrimaryButtonClicked(profileData));
        mModel.set(
                SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED,
                (unusedView) -> onSecondaryButtonClicked());
        mModel.set(
                SigninPromoProperties.ON_DISMISS_BUTTON_CLICKED,
                (unusedView) -> onDismissButtonClicked());
        mModel.set(SigninPromoProperties.TITLE_TEXT, mPromoDelegate.getTitle());
        mModel.set(
                SigninPromoProperties.DESCRIPTION_TEXT,
                mPromoDelegate.getDescription(
                        profileData == null ? null : profileData.getAccountEmail()));
        mModel.set(
                SigninPromoProperties.PRIMARY_BUTTON_TEXT,
                mPromoDelegate.getTextForPrimaryButton(profileData));
        mModel.set(
                SigninPromoProperties.SECONDARY_BUTTON_TEXT,
                mPromoDelegate.getTextForSecondaryButton());
        mModel.set(
                SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON,
                !mPromoDelegate.canBeDismissedPermanently());
        mModel.set(
                SigninPromoProperties.SHOULD_SHOW_ACCOUNT_PICKER,
                profileData != null && !mPromoDelegate.shouldDisplaySignedInLayout());
        mModel.set(
                SigninPromoProperties.SHOULD_SHOW_HEADER_WITH_AVATAR,
                mPromoDelegate.shouldDisplaySignedInLayout());
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            mModel.set(
                    SigninPromoProperties.SHOULD_SHOW_LOADING_STATE,
                    mPromoDelegate.shouldDisplayLoadingState());
            mModel.set(
                    SigninPromoProperties.SELECTED_ACCOUNT_VIEW_BACKGROUND,
                    mPromoDelegate.getAccountPickerBackgroundColor());
        }
    }

    private void updateLoadingState() {
        if (!SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
                || !mPromoDelegate.canShowPromo()) {
            return;
        }
        DisplayableProfileData profileData = getVisibleAccount();
        mModel.set(
                SigninPromoProperties.SHOULD_SHOW_LOADING_STATE,
                mPromoDelegate.shouldDisplayLoadingState());
        mModel.set(
                SigninPromoProperties.PRIMARY_BUTTON_TEXT,
                mPromoDelegate.getTextForPrimaryButton(profileData));
        mModel.set(
                SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON,
                !mPromoDelegate.canBeDismissedPermanently()
                        || mPromoDelegate.shouldDisplayLoadingState());
    }

    private void updateVisibility() {
        boolean shouldShowPromo = canShowPromo();
        if (mShouldShowPromo == shouldShowPromo) {
            return;
        }
        mShouldShowPromo = shouldShowPromo;
        mPromoDelegate.onPromoVisibilityChange();
    }

    /**
     * Return the account that is intended to be displayed to the user within the sign-in promo. If
     * the user is not signed into Chrome (no primary account), checks for the default Google
     * account configured on the Android device. Returns null if there are no accounts on the
     * device.
     */
    private @Nullable DisplayableProfileData getVisibleAccount() {
        @Nullable AccountInfo primaryAccount = mIdentityManager.getPrimaryAccountInfo();
        if (primaryAccount != null) {
            return mProfileDataCache.getById(primaryAccount.getId());
        }
        return ProfileDataUtils.getFirstIfFulfilledAndNotEmpty(mProfileDataCache.getAccounts());
    }

    private void recordEventHistogram(@Event String actionType) {
        RecordHistogram.recordExactLinearHistogram(
                "Signin.SyncPromo." + actionType + ".Count." + mPromoDelegate.getAccessPointName(),
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT),
                MAX_TOTAL_PROMO_SHOW_COUNT);

        if (!Event.SHOWN.equals(actionType)) {
            RecordHistogram.recordExactLinearHistogram(
                    "Signin.Promo.ImpressionsUntil."
                            + actionType
                            + "."
                            + mPromoDelegate.getAccessPointName(),
                    mPromoDelegate.getPromoShownCount(),
                    MAX_TOTAL_PROMO_SHOW_COUNT);
        }
    }
}
