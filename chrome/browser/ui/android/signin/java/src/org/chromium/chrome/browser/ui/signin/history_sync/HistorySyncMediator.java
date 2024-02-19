// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.PropertyModel;

class HistorySyncMediator {
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private final HistorySyncCoordinator.HistorySyncDelegate mDelegate;
    private final SigninManager mSigninManager;
    private final SyncService mSyncService;

    HistorySyncMediator(
            Context context, HistorySyncCoordinator.HistorySyncDelegate delegate, Profile profile) {
        mDelegate = delegate;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        mSigninManager = IdentityServicesProvider.get().getSigninManager(profile);
        mSyncService = SyncServiceFactory.getForProfile(profile);
        mModel =
                HistorySyncProperties.createModel(
                        mProfileDataCache.getProfileDataOrDefault(""),
                        this::onAcceptClicked,
                        this::onDeclineClicked,
                        this::onMoreClicked,
                        this::onSettingsClicked);
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onAcceptClicked() {
        mSyncService.setSelectedType(UserSelectableType.HISTORY, /* isTypeOn= */ true);
        mSyncService.setSelectedType(UserSelectableType.TABS, /* isTypeOn= */ true);
        mDelegate.dismiss();
    }

    private void onDeclineClicked() {
        mDelegate.dismiss();
    }

    private void onMoreClicked() {
        // TODO(crbug.com/1520791): Implement this method
    }

    private void onSettingsClicked() {
        // TODO(crbug.com/1520791): Implement this method
    }
}
