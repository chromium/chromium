// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import androidx.annotation.Nullable;
import androidx.annotation.StringDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

        @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        @Nullable
        DisplayableProfileData profileData =
                visibleAccount == null
                        ? null
                        : mProfileDataCache.getProfileDataOrDefault(visibleAccount.getEmail());
        mModel =
                SigninPromoProperties.createModel(
                        profileData,
                        this::onPrimaryButtonClicked,
                        this::onSecondaryButtonClicked,
                        this::onDismissButtonClicked,
                        delegate.getTitle(),
                        delegate.getDescription(),
                        delegate.getTextForPrimaryButton(profileData),
                        delegate.getTextForSecondaryButton(),
                        profileData == null || delegate.shouldHideSecondaryButton(),
                        delegate.shouldHideDismissButton());
        mMaxImpressionReached = mDelegate.isMaxImpressionsReached();
        mShouldShowPromo = canShowPromo();

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
        recordEventHistogram(Event.SHOWN);
        mDelegate.recordImpression();
        mWasImpressionRecorded = true;
    }

    boolean canShowPromo() {
        return !mMaxImpressionReached && mDelegate.canShowPromo(getVisibleAccount());
    }

    /** Implements {@link IdentityManager.Observer} */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        updateModel(getVisibleAccount());
        updateState();
    }

    /** Implements {@link SyncService.SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        // Only update state as no change to visible account happened.
        updateState();
    }

    /** Implements {@link AccountsChangeObserver} */
    @Override
    public void onCoreAccountInfosChanged() {
        updateModel(getVisibleAccount());
        updateState();
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        @Nullable CoreAccountInfo visibleAccount = getVisibleAccount();
        if (visibleAccount != null && !visibleAccount.getEmail().equals(accountEmail)) {
            return;
        }
        updateModel(visibleAccount);
        updateState();
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
        updateState();
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
                profileData == null || mDelegate.shouldHideDismissButton());
    }

    private void updateState() {
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
                    AccountUtils.getDefaultCoreAccountInfoIfFulfilled(
                            mAccountManagerFacade.getCoreAccountInfos());
        }
        return visibleAccount;
    }

    private void recordEventHistogram(@Event String actionType) {
        RecordHistogram.recordExactLinearHistogram(
                "Signin.SyncPromo." + actionType + ".Count." + mDelegate.getAccessPointName(),
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT),
                MAX_TOTAL_PROMO_SHOW_COUNT);
    }
}
