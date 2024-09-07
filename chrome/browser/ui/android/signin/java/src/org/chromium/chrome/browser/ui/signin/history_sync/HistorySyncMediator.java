// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper.ScreenMode;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.metrics.SyncButtonClicked;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncMediator implements ProfileDataCache.Observer, SigninManager.SignInStateObserver {
    private final PropertyModel mModel;
    private final String mAccountEmail;
    private final HistorySyncCoordinator.HistorySyncDelegate mDelegate;
    private final SigninManager mSigninManager;
    private final SyncService mSyncService;
    private final ProfileDataCache mProfileDataCache;
    private final @SigninAccessPoint int mAccessPoint;
    private final boolean mShouldSignOutOnDecline;
    private final HistorySyncHelper mHistorySyncHelper;

    HistorySyncMediator(
            Context context,
            HistorySyncCoordinator.HistorySyncDelegate delegate,
            Profile profile,
            @SigninAccessPoint int accessPoint,
            boolean showEmailInFooter,
            boolean shouldSignOutOnDecline,
            boolean mUseLandscapeLayout) {
        mAccessPoint = accessPoint;
        mDelegate = delegate;
        mShouldSignOutOnDecline = shouldSignOutOnDecline;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mSigninManager = IdentityServicesProvider.get().getSigninManager(profile);
        mSyncService = SyncServiceFactory.getForProfile(profile);
        mHistorySyncHelper = HistorySyncHelper.getForProfile(profile);
        mProfileDataCache.addObserver(this);
        mSigninManager.addSignInStateObserver(this);
        mAccountEmail =
                CoreAccountInfo.getEmailFrom(
                        mSigninManager
                                .getIdentityManager()
                                .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        // The history sync screen should never be created when the user is signed out.
        assert mAccountEmail != null;
        DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mAccountEmail);
        // When the email address is not displayable, fall back on the other string.
        String footerString =
                showEmailInFooter && profileData.hasDisplayableEmailAddress()
                        ? context.getString(R.string.history_sync_footer_with_email, mAccountEmail)
                        : context.getString(R.string.history_sync_footer_without_email);
        mModel =
                HistorySyncProperties.createModel(
                        profileData,
                        this::onAcceptClicked,
                        this::onDeclineClicked,
                        footerString,
                        mUseLandscapeLayout);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        if (!TextUtils.equals(mAccountEmail, accountEmail)) {
            return;
        }
        mModel.set(
                HistorySyncProperties.PROFILE_DATA,
                mProfileDataCache.getProfileDataOrDefault(accountEmail));
    }

    /** Implements {@link SigninManager.SignInStateObserver} */
    @Override
    public void onSignedOut() {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Aborted", mAccessPoint, SigninAccessPoint.MAX);
        mDelegate.dismissHistorySync();
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
        mSigninManager.removeSignInStateObserver(this);
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onAcceptClicked(View view) {
        int syncButtonType = mModel.get(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS);

        switch (syncButtonType) {
            case ScreenMode.RESTRICTED:
            case ScreenMode.DEADLINED:
                SigninMetricsUtils.logHistorySyncAcceptButtonClicked(
                        mAccessPoint, SyncButtonClicked.HISTORY_SYNC_OPT_IN_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNRESTRICTED:
                SigninMetricsUtils.logHistorySyncAcceptButtonClicked(
                        mAccessPoint, SyncButtonClicked.HISTORY_SYNC_OPT_IN_NOT_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNSUPPORTED:
            case ScreenMode.PENDING:
                throw new IllegalStateException("Unrecognized restriction status.");
        }
        mDelegate.maybeRecordFreProgress(MobileFreProgress.HISTORY_SYNC_ACCEPTED);

        mSyncService.setSelectedType(UserSelectableType.HISTORY, /* isTypeOn= */ true);
        mSyncService.setSelectedType(UserSelectableType.TABS, /* isTypeOn= */ true);
        mHistorySyncHelper.clearHistorySyncDeclinedPrefs();
        mDelegate.dismissHistorySync();
    }

    private void onDeclineClicked(View view) {
        int syncButtonType = mModel.get(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS);

        switch (syncButtonType) {
            case ScreenMode.RESTRICTED:
            case ScreenMode.DEADLINED:
                SigninMetricsUtils.logHistorySyncDeclineButtonClicked(
                        mAccessPoint, SyncButtonClicked.HISTORY_SYNC_CANCEL_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNRESTRICTED:
                SigninMetricsUtils.logHistorySyncDeclineButtonClicked(
                        mAccessPoint, SyncButtonClicked.HISTORY_SYNC_CANCEL_NOT_EQUAL_WEIGHTED);
                break;
            case ScreenMode.UNSUPPORTED:
            case ScreenMode.PENDING:
                throw new IllegalStateException("Unrecognized restriction status.");
        }
        mDelegate.maybeRecordFreProgress(MobileFreProgress.HISTORY_SYNC_DISMISSED);

        if (mShouldSignOutOnDecline) {
            mSigninManager.signOut(
                    SignoutReason.USER_DECLINED_HISTORY_SYNC_AFTER_DEDICATED_SIGN_IN);
        }
        mHistorySyncHelper.recordHistorySyncDeclinedPrefs();
        mDelegate.dismissHistorySync();
    }

    /**
     * This method will be called once MinorModeRestrictions has resolved. Buttons will be recreated
     * according to the restrictionStatus and onClickListeners are updated.
     */
    public void onMinorModeRestrictionStatusUpdated(
            @MinorModeHelper.ScreenMode int restrictionStatus) {
        mModel.set(HistorySyncProperties.MINOR_MODE_RESTRICTION_STATUS, restrictionStatus);
        mModel.set(HistorySyncProperties.ON_ACCEPT_CLICKED, this::onAcceptClicked);
        mModel.set(HistorySyncProperties.ON_DECLINE_CLICKED, this::onDeclineClicked);
    }
}
