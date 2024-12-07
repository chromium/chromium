// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/** {@link SigninPromoDelegate} for bookmark signin promo. */
public class BookmarkSigninPromoDelegate extends SigninPromoDelegate {
    @VisibleForTesting static final int MAX_IMPRESSIONS_BOOKMARKS = 20;

    private final String mPromoShowCountPreferenceName;

    public BookmarkSigninPromoDelegate(
            Context context,
            Profile profile,
            SigninAndHistorySyncActivityLauncher launcher,
            Runnable onPromoStateChange) {
        super(context, profile, launcher, onPromoStateChange);

        mPromoShowCountPreferenceName =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SyncPromoAccessPointId.BOOKMARKS);
    }

    @Override
    String getTitle() {
        return mContext.getString(R.string.signin_promo_title_bookmarks);
    }

    @Override
    String getDescription() {
        return mContext.getString(R.string.signin_promo_description_bookmarks);
    }

    @Override
    @SigninAccessPoint
    int getAccessPoint() {
        return SigninAccessPoint.BOOKMARK_MANAGER;
    }

    @Override
    void onDismissButtonClicked() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, true);
    }

    @Override
    boolean canShowPromo(@Nullable CoreAccountInfo visibleAccount) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                || !signinManager.isSigninAllowed()) {
            return false;
        }
        if (syncService
                .getSelectedTypes()
                .containsAll(
                        Set.of(UserSelectableType.BOOKMARKS, UserSelectableType.READING_LIST))) {
            return false;
        }

        boolean isTypeManagedByPolicy =
                syncService.isTypeManagedByPolicy(UserSelectableType.BOOKMARKS);
        boolean isPromoDismissed =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.SIGNIN_PROMO_BOOKMARKS_DECLINED, false);

        return !isTypeManagedByPolicy && !isPromoDismissed;
    }

    @Override
    void recordImpression() {
        ChromeSharedPreferences.getInstance().incrementInt(mPromoShowCountPreferenceName);
    }

    @Override
    boolean isMaxImpressionsReached() {
        return ChromeSharedPreferences.getInstance().readInt(mPromoShowCountPreferenceName)
                >= MAX_IMPRESSIONS_BOOKMARKS;
    }
}
