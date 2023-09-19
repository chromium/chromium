// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import static androidx.annotation.VisibleForTesting.PRIVATE;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Optional;

/**
 * Queries sWAA(Supplemental Web and App Activity) status to enable page insights sheet
 * of Google bottom bar feature. The queried state is cached in a persistent storage,
 * remaining valid for a specified period of time. Queries are made in a periodic fashion
 * to keep it in sync with the actual status.
 */
public class PageInsightsSwaaChecker {
    @VisibleForTesting(otherwise = PRIVATE)
    static final long REFRESH_PERIOD_MS = DateUtils.MINUTE_IN_MILLIS * 5;

    @VisibleForTesting(otherwise = PRIVATE)
    static final long EXTRA_QUERY_INTERVAL_MS = DateUtils.SECOND_IN_MILLIS;

    private static final long[] RETRY_PERIODS_MS = new long[] {5 * DateUtils.SECOND_IN_MILLIS,
            10 * DateUtils.SECOND_IN_MILLIS, 30 * DateUtils.SECOND_IN_MILLIS};

    @VisibleForTesting(otherwise = PRIVATE)
    static final int MSG_REFRESH = 37; // randomly chosen msg ID

    @VisibleForTesting(otherwise = PRIVATE)
    static final int MSG_RETRY = 41; // randomly chosen msg ID

    private final Runnable mActivateCallback;
    private final Profile mProfile;

    private Handler mHandler;
    private Supplier<Long> mElapsedRealtime;
    private int mRetryCount;

    PageInsightsSwaaChecker(Profile profile, Runnable activateCallback) {
        mElapsedRealtime = System::currentTimeMillis;
        mProfile = profile;
        mActivateCallback = activateCallback;
        mHandler = new Handler(Looper.getMainLooper()) {
            @Override
            public void handleMessage(Message msg) {
                if (msg == null) return;
                if (msg.what == MSG_REFRESH || msg.what == MSG_RETRY) {
                    if (msg.what == MSG_REFRESH) mRetryCount = 0;
                    sendQuery();
                    scheduleRetry();
                }
            }
        };
    }

    /** Reset sWAA info when it gets invalidated, or in preparation for the new profile. */
    static void invalidateCache() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.removeKey(ChromePreferenceKeys.SWAA_TIMESTAMP);
        prefs.removeKey(ChromePreferenceKeys.SWAA_STATUS);
    }

    /**
     * Start periodic querying of supplemental Web and App Activity setting.
     */
    void start() {
        boolean swaaEnabled = isSwaaEnabled().orElse(false);
        if (!swaaEnabled) {
            mHandler.sendEmptyMessage(MSG_REFRESH);
            // The very first sWAA query to the server often returns false. To get around it,
            // Send another query in quick succession if not cached or the current state is false.
            // TODO(crbug/1482474): Figure out why this happens and remove this extra query.
            mHandler.sendEmptyMessageDelayed(MSG_REFRESH, EXTRA_QUERY_INTERVAL_MS);
        } else {
            // Invoke activate callback if sWAA bit is on. This speeds up the intantiation of PIH.
            mActivateCallback.run();
            if (!isUpdateScheduled()) {
                mHandler.sendEmptyMessageDelayed(
                        MSG_REFRESH, REFRESH_PERIOD_MS - timeSinceLastUpdateMs());
            }
        }
    }

    /**
     * Stop periodic querying of supplemental Web and App Activity setting.
     */
    void stop() {
        removeAllMessages();
    }

    private void removeAllMessages() {
        mHandler.removeMessages(MSG_REFRESH);
        mHandler.removeMessages(MSG_RETRY);
    }

    private long timeSinceLastUpdateMs() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        long lastUpdateMs = prefs.readLong(ChromePreferenceKeys.SWAA_TIMESTAMP, 0);
        assert lastUpdateMs != 0 : "There has been no sWAA update before.";
        return mElapsedRealtime.get() - lastUpdateMs;
    }

    private void sendQuery() {
        PageInsightsSwaaCheckerJni.get().queryStatus(this, mProfile);
        mHandler.sendEmptyMessageDelayed(MSG_REFRESH, REFRESH_PERIOD_MS);
    }

    private void scheduleRetry() {
        mHandler.removeMessages(MSG_RETRY);
        if (mRetryCount < RETRY_PERIODS_MS.length) {
            mHandler.sendEmptyMessageDelayed(MSG_RETRY, RETRY_PERIODS_MS[mRetryCount]);
            mRetryCount++;
        } else {
            // Stop retrying.
            mRetryCount = 0;
        }
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void onSwaaResponse(boolean enabled) {
        mHandler.removeMessages(MSG_RETRY);
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.writeLong(ChromePreferenceKeys.SWAA_TIMESTAMP, mElapsedRealtime.get());
        prefs.writeBoolean(ChromePreferenceKeys.SWAA_STATUS, enabled);
        if (enabled) mActivateCallback.run();
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

    @VisibleForTesting(otherwise = PRIVATE)
    boolean isUpdateScheduled() {
        return mHandler.hasMessages(MSG_REFRESH);
    }

    @VisibleForTesting(otherwise = PRIVATE)
    boolean isRetryScheduled() {
        return mHandler.hasMessages(MSG_RETRY);
    }

    void onSignedIn() {
        removeAllMessages();
        invalidateCache();
        mHandler.sendEmptyMessage(MSG_REFRESH);
    }

    void onSignedOut() {
        removeAllMessages();
        invalidateCache();
    }

    void setElapsedRealtimeSupplierForTesting(Supplier<Long> elapsedRealtime) {
        mElapsedRealtime = elapsedRealtime;
    }

    Handler getHandlerForTesting() {
        return mHandler;
    }

    void setHandlerForTesting(Handler handler) {
        mHandler = handler;
    }

    @NativeMethods
    public interface Natives {
        void queryStatus(PageInsightsSwaaChecker checker, Profile profile);
    }
}
