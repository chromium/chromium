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
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninPromoAction;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
final class SigninPromoMediator
        implements IdentityManager.Observer,
                SyncService.SyncStateChangedListener,
                AccountsChangeObserver,
                ProfileDataCache.Observer {
    private static final int MAX_TOTAL_PROMO_SHOW_COUNT = 100;

    /** Strings used for promo event count histograms. */
    // LINT.IfChange(Event)
    @StringDef({Event.CONTINUED, Event.DISMISSED, Event.SHOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface Event {
        String CONTINUED = "Continued";
        String DISMISSED = "Dismissed";
        String SHOWN = "Shown";
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/signin/histograms.xml:SigninPromoAction)

    private final IdentityManager mIdentityManager;
    private final SyncService mSyncService;
    private final AccountManagerFacade mAccountManagerFacade;
    private final ProfileDataCache mProfileDataCache;
    private final SigninPromoDelegate mDelegate;
    private final PropertyModel mModel;
    private final boolean mMaxImpressionReached;

    private boolean mShouldShowPromo;
    private boolean mWasImpressionRecorded;

    SigninPromoMediator(
            IdentityManager identityManager,
            SyncService syncService,
            AccountManagerFacade accountManagerFacade,
            ProfileDataCache profileDataCache,
            SigninPromoDelegate delegate) {
        mIdentityManager = identityManager;
        mSyncService = syncService;
        mAccountManagerFacade = accountManagerFacade;
        mProfileDataCache = profileDataCache;
        mDelegate = delegate;

        CoreAccountInfo visibleAccount = getVisibleAccount();
        DisplayableProfileData profileData =
                visibleAccount == null
                        ? null
                        : mProfileDataCache.getProfileDataOrDefault(visibleAccount.getEmail());

        mModel =
                SigninPromoProperties.createModel(
                        profileData, () -> {}, () -> {}, () -> {}, "", "", "", "", false, false);
        mMaxImpressionReached = mDelegate.isMaxImpressionsReached();
        mDelegate.refreshPromoState(visibleAccount);
        mShouldShowPromo = canShowPromo();
        if (mShouldShowPromo) {
            updateModel(visibleAccount);
        }

        mIdentityManager.addObserver(this);
        mSyncService.addSyncStateChangedListener(this);
        mAccountManagerFacade.addObserver(this);
        mProfileDataCache.addObserver(this);
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
        mSyncService.removeSyncStateChangedListener(this);
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
        SigninMetricsUtils.logSigninOffered(promoAction, mDelegate.getAccessPoint());

        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        recordEventHistogram(Event.SHOWN);
        mDelegate.recordImpression();
        mWasImpressionRecorded = true;
    }

    boolean canShowPromo() {
        if (!mAccountManagerFacade.getAccounts().isFulfilled()
                || !mAccountManagerFacade.didAccountFetchSucceed()) {
            // If accounts are not available in AccountManagerFacade yet, then don't shown the
            // promo.
            return false;
        }

        return !mMaxImpressionReached && mDelegate.canShowPromo();
    }

    /** Implements {@link IdentityManager.Observer} */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        boolean wasVisibleAccountUpdated =
                eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)
                        != PrimaryAccountChangeEvent.Type.NONE;
        refreshPromoContent(wasVisibleAccountUpdated);
    }

    /** Implements {@link SyncService.SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        refreshPromoContent(/* wasVisibleAccountUpdated= */ false);
    }

    /** Implements {@link AccountsChangeObserver} */
    @Override
    public void onCoreAccountInfosChanged() {
        refreshPromoContent(/* wasVisibleAccountUpdated= */ true);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        if (visibleAccount != null && !visibleAccount.getEmail().equals(accountEmail)) {
            return;
        }
        refreshPromoContent(/* wasVisibleAccountUpdated= */ true);
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onPrimaryButtonClicked() {
        recordEventHistogram(Event.CONTINUED);
        mDelegate.onPrimaryButtonClicked();
    }

    private void onSecondaryButtonClicked() {
        recordEventHistogram(Event.CONTINUED);
        mDelegate.onSecondaryButtonClicked();
    }

    private void onDismissButtonClicked() {
        recordEventHistogram(Event.DISMISSED);
        mDelegate.onDismissButtonClicked();
        refreshPromoContent(/* wasVisibleAccountUpdated= */ false);
    }

    private void refreshPromoContent(boolean wasVisibleAccountUpdated) {
        boolean wasPromoContentChanged = mDelegate.refreshPromoState(getVisibleAccount());
        if (wasPromoContentChanged) {
            updateVisibility();
        }
        if (mShouldShowPromo && (wasVisibleAccountUpdated || wasPromoContentChanged)) {
            updateModel(getVisibleAccount());
        }
    }

    private void updateModel(@Nullable CoreAccountInfo visibleAccount) {
        @Nullable
        DisplayableProfileData profileData =
                visibleAccount == null
                        ? null
                        : mProfileDataCache.getProfileDataOrDefault(visibleAccount.getEmail());
        mModel.set(SigninPromoProperties.PROFILE_DATA, profileData);
        mModel.set(
                SigninPromoProperties.SHOULD_HIDE_SECONDARY_BUTTON,
                profileData == null || mDelegate.shouldHideSecondaryButton());
        mModel.set(
                SigninPromoProperties.ON_PRIMARY_BUTTON_CLICKED,
                (unusedView) -> onPrimaryButtonClicked());
        mModel.set(
                SigninPromoProperties.ON_SECONDARY_BUTTON_CLICKED,
                (unusedView) -> onSecondaryButtonClicked());
        mModel.set(
                SigninPromoProperties.ON_DISMISS_BUTTON_CLICKED,
                (unusedView) -> onDismissButtonClicked());
        mModel.set(SigninPromoProperties.TITLE_TEXT, mDelegate.getTitle());
        mModel.set(SigninPromoProperties.DESCRIPTION_TEXT, mDelegate.getDescription());
        mModel.set(
                SigninPromoProperties.PRIMARY_BUTTON_TEXT,
                mDelegate.getTextForPrimaryButton(profileData));
        mModel.set(
                SigninPromoProperties.SECONDARY_BUTTON_TEXT, mDelegate.getTextForSecondaryButton());
        mModel.set(
                SigninPromoProperties.SHOULD_HIDE_DISMISS_BUTTON,
                mDelegate.shouldHideDismissButton());
    }

    private void updateVisibility() {
        boolean shouldShowPromo = canShowPromo();
        if (mShouldShowPromo == shouldShowPromo) {
            return;
        }
        mShouldShowPromo = shouldShowPromo;
        mDelegate.onPromoVisibilityChange();
    }

    private @Nullable CoreAccountInfo getVisibleAccount() {
        @Nullable
        CoreAccountInfo visibleAccount =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (visibleAccount == null) {
            visibleAccount =
                    AccountUtils.getDefaultAccountIfFulfilled(mAccountManagerFacade.getAccounts());
        }
        return visibleAccount;
    }

    private void recordEventHistogram(@Event String actionType) {
        RecordHistogram.recordExactLinearHistogram(
                "Signin.SyncPromo." + actionType + ".Count." + mDelegate.getAccessPointName(),
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT),
                MAX_TOTAL_PROMO_SHOW_COUNT);

        if (!Event.SHOWN.equals(actionType)) {
            RecordHistogram.recordExactLinearHistogram(
                    "Signin.Promo.ImpressionsUntil."
                            + actionType
                            + "."
                            + mDelegate.getAccessPointName(),
                    mDelegate.getPromoShownCount(),
                    MAX_TOTAL_PROMO_SHOW_COUNT);
        }
    }
}
