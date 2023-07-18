// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.util.SparseArray;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.ui.util.TokenHolder;

/**
 * Manages all the dynamic conditions used to activate page insights component:
 * - Chrome signin
 * - History Sync without a custom passphrase
 * - sWAA (supplemental Web and App Activity)
 *
 * sWAA checking task is delegated to {@link PageInsightsSwaaChecker} which talks to server
 * in a periodic basis.
 *
 * Used as a singleton object overlooking multiple PIH CCTs. Whenever a certain condition
 * is enabled, invokes a callback to attempt to instantiate the page insights component for
 * each CCT instance. Instantiation will succeed the first time when all the conditions
 * are met.
 */
public class PageInsightsActivator implements SignInStateObserver, SyncStateChangedListener {
    private static PageInsightsActivator sInstance;

    private final Profile mProfile;
    private final SparseArray<Runnable> mCallbacks = new SparseArray<>();

    private PageInsightsSwaaChecker mSwaaChecker;
    private TokenHolder mTokenHolder;

    public static PageInsightsActivator getForProfile(Profile profile) {
        boolean profileSwitched = sInstance != null && sInstance.mProfile != profile;
        if (sInstance == null || profileSwitched) {
            sInstance = new PageInsightsActivator(profile);
            if (profileSwitched) PageInsightsSwaaChecker.invalidateCache();
        }
        return sInstance;
    }

    /** Whether or not sWAA bit is enabled. */
    public static boolean isSwaaEnabled() {
        return sInstance != null ? sInstance.mSwaaChecker.isSwaaEnabled().orElse(false) : false;
    }

    private PageInsightsActivator(Profile profile) {
        mProfile = profile;
        mSwaaChecker = new PageInsightsSwaaChecker(profile, this::invokeCallbacks);
        mTokenHolder = new TokenHolder(() -> {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
            SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
            if (mTokenHolder.hasTokens()) {
                signinManager.addSignInStateObserver(PageInsightsActivator.this);
                syncService.addSyncStateChangedListener(PageInsightsActivator.this);
            } else {
                signinManager.removeSignInStateObserver(PageInsightsActivator.this);
                syncService.removeSyncStateChangedListener(PageInsightsActivator.this);
            }
        });
    }

    /**
     * Start checking page insights activation conditions.
     * @param callback Callback to run to activate page insights component.
     * @return a token to be used later to stop checking.
     */
    public int start(Runnable callback) {
        mSwaaChecker.start();
        int token = mTokenHolder.acquireToken();
        mCallbacks.put(token, callback);
        return token;
    }

    /**
     * Stop checking page insights activation conditions.
     * @param token A token identifying a CCT object to manage.
     */
    public void stop(int token) {
        mTokenHolder.releaseToken(token);
        mCallbacks.remove(token);
        if (!mTokenHolder.hasTokens()) {
            // Stop and delete the activator when the last PIH CCT is gone.
            mSwaaChecker.stop();
            sInstance = null;
        }
    }

    private void invokeCallbacks() {
        for (int i = 0; i < mCallbacks.size(); ++i) mCallbacks.valueAt(i).run();
    }

    // SignInStateObserver implementation

    @Override
    public void onSignedIn() {
        invokeCallbacks();
        mSwaaChecker.onSignedIn();
    }

    @Override
    public void onSignedOut() {
        mSwaaChecker.onSignedOut();
    }

    // SyncStateChangedListener implementation

    @Override
    public void syncStateChanged() {
        // Checks for Chrome History Sync without a custom passphrase.
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService.isSyncingUnencryptedUrls()) invokeCallbacks();
    }

    void setSwaaCheckerForTesting(PageInsightsSwaaChecker swaaChecker) {
        mSwaaChecker = swaaChecker;
    }

    PageInsightsSwaaChecker getSwaaCheckerForTesting() {
        return mSwaaChecker;
    }
}
