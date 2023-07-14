// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.ui.util.TokenHolder;

import java.util.Optional;

/**
 * Queries sWAA(Supplemental Web and App Activity) status to enable page insights sheet
 * of Google bottom bar feature. The queried state is cached in a persistent storage,
 * remaining valid for a specified period of time. Queries are made in a periodic fashion
 * to keep it in sync with the actual status.
 */
public class PageInsightsSwaaChecker implements SignInStateObserver {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final long REFRESH_PERIOD_MS = DateUtils.MINUTE_IN_MILLIS * 5;
    private static final int MSG_REFRESH = 37; // random msg ID

    private static PageInsightsSwaaChecker sInstance;

    private final Profile mProfile;
    private final Handler mHandler;

    private Supplier<Long> mElapsedRealtime;
    private TokenHolder mTokenHolder;

    public static PageInsightsSwaaChecker getForProfile(Profile profile) {
        boolean profileSwitched = sInstance != null && sInstance.mProfile != profile;
        if (sInstance == null || profileSwitched) {
            sInstance = new PageInsightsSwaaChecker(profile);
            if (profileSwitched) invalidateCache();
        }
        return sInstance;
    }

    private PageInsightsSwaaChecker(Profile profile) {
        mProfile = profile;
        mElapsedRealtime = SystemClock::elapsedRealtime;
        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                if (msg != null && msg.what == MSG_REFRESH) sendQuery();
            }
        };
        mTokenHolder = new TokenHolder(() -> {
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(mProfile);
            if (mTokenHolder.hasTokens()) {
                signinManager.addSignInStateObserver(PageInsightsSwaaChecker.this);
            } else {
                signinManager.removeSignInStateObserver(PageInsightsSwaaChecker.this);
            }
        });
    }

    static void invalidateCache() {
        // Reset sWAA info in preparation for the new profile.
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.removeKey(ChromePreferenceKeys.SWAA_TIMESTAMP);
        prefs.removeKey(ChromePreferenceKeys.SWAA_STATUS);
    }

    /**
     * Start periodic querying of supplemental Web and App Activity setting.
     * @return a token to be used later to stop the checker.
     */
    public int start() {
        Optional<Boolean> swaaStatus = isSwaaEnabled();
        if (!swaaStatus.isPresent()) {
            sendQuery();
        } else if (!isUpdateScheduled()) {
            mHandler.sendEmptyMessageDelayed(
                    MSG_REFRESH, REFRESH_PERIOD_MS - timeSinceLastUpdateMs());
        }
        return mTokenHolder.acquireToken();
    }

    public void stop(int token) {
        mTokenHolder.releaseToken(token);
        if (!mTokenHolder.hasTokens()) {
            // Stop and delete the checker when the last PIH CCT is gone.
            mHandler.removeMessages(MSG_REFRESH);
            sInstance = null;
        }
    }

    private long timeSinceLastUpdateMs() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        long lastUpdateMs = prefs.readLong(ChromePreferenceKeys.SWAA_TIMESTAMP, 0);
        assert lastUpdateMs != 0 : "There has been no sWAA update before.";
        return mElapsedRealtime.get() - lastUpdateMs;
    }

    private void sendQuery() {
        // TODO(b/282739536): Retry if we do not get a response (within a few seconds?).
        PageInsightsSwaaCheckerJni.get().queryStatus(this, mProfile);
        mHandler.sendEmptyMessageDelayed(MSG_REFRESH, REFRESH_PERIOD_MS);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void onSwaaResponse(boolean enabled) {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.writeLong(ChromePreferenceKeys.SWAA_TIMESTAMP, mElapsedRealtime.get());
        prefs.writeBoolean(ChromePreferenceKeys.SWAA_STATUS, enabled);
    }

    /**
     * Returns an {@link Optional} containing the sWAA status. Empty if the value has not
     * been cached or already expired.
     */
    @NonNull
    public Optional<Boolean> isSwaaEnabled() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        long lastUpdate = prefs.readLong(ChromePreferenceKeys.SWAA_TIMESTAMP, 0);
        if (lastUpdate != 0 && timeSinceLastUpdateMs() < REFRESH_PERIOD_MS) {
            return Optional.of(prefs.readBoolean(ChromePreferenceKeys.SWAA_STATUS, false));
        }
        return Optional.empty();
    }

    @VisibleForTesting
    boolean isUpdateScheduled() {
        return mHandler.hasMessages(MSG_REFRESH);
    }

    @Override
    public void onSignedIn() {
        mHandler.removeMessages(MSG_REFRESH);
        invalidateCache();
        sendQuery();
    }

    @Override
    public void onSignedOut() {
        mHandler.removeMessages(MSG_REFRESH);
        invalidateCache();
    }

    void setElapsedRealtimeSupplierForTesting(Supplier<Long> elapsedRealtime) {
        mElapsedRealtime = elapsedRealtime;
    }

    @NativeMethods
    public interface Natives {
        void queryStatus(PageInsightsSwaaChecker checker, Profile profile);
    }
}
