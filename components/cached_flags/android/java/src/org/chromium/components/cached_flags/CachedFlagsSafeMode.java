// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.cached_flags;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.base.cached_flags.ValuesReturned;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.BuildConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Controls Safe Mode for {@link CachedFlag}.
 *
 * <p>Safe Mode is a mechanism that allows Chrome to prevent crashes gated behind flags used before
 * native from becoming a crash loop that cannot be recovered from by disabling the experiment.
 */
public class CachedFlagsSafeMode {
    private static final String TAG = "Flags";
    private static final int CRASH_STREAK_TO_ENTER_SAFE_MODE = 2;

    private static final String SAFE_VALUES_FILE =
            "org.chromium.chrome.browser.flags.SafeModeValues";

    @VisibleForTesting
    static final String PREF_SAFE_VALUES_VERSION = "Chrome.Flags.SafeValuesVersion";

    private Boolean mSafeModeExperimentForcedForTesting;

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @VisibleForTesting
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        Behavior.UNKNOWN,
        Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
        Behavior.ENGAGED_WITH_SAFE_VALUES,
        Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES,
        Behavior.ENGAGED_WITHOUT_SAFE_VALUES
    })
    @interface Behavior {
        int UNKNOWN = 0;
        int NOT_ENGAGED_BELOW_THRESHOLD = 1;
        int ENGAGED_WITH_SAFE_VALUES = 2;
        int ENGAGED_IGNORING_OUTDATED_SAFE_VALUES = 3;
        int ENGAGED_WITHOUT_SAFE_VALUES = 4;

        int NUM_ENTRIES = 5;
    }

    private AtomicInteger mBehavior = new AtomicInteger(Behavior.UNKNOWN);

    private AtomicBoolean mStartCheckpointWritten = new AtomicBoolean(false);
    private AtomicBoolean mEndCheckpointWritten = new AtomicBoolean(false);

    private static final CachedFlagsSafeMode sInstance = new CachedFlagsSafeMode();

    public static CachedFlagsSafeMode getInstance() {
        return sInstance;
    }

    // Singleton
    private CachedFlagsSafeMode() {}

    /**
     * Call right before any flag is checked. The first time this is called, check if safe mode
     * should be engaged, and engages it if necessary.
     */
    @AnyThread
    void onFlagChecked() {
        synchronized (mBehavior) {
            if (mBehavior.get() != Behavior.UNKNOWN) return;
            if (shouldEnterSafeMode()) {
                String cachedVersion =
                        getSafeValuePreferences().getString(PREF_SAFE_VALUES_VERSION, "");
                int behavior;
                if (cachedVersion.isEmpty()) {
                    behavior = Behavior.ENGAGED_WITHOUT_SAFE_VALUES;
                } else if (!cachedVersion.equals(VersionInfo.getProductVersion())) {
                    behavior = Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES;
                } else {
                    behavior = Behavior.ENGAGED_WITH_SAFE_VALUES;
                }
                mBehavior.set(behavior);
                RecordHistogram.recordEnumeratedHistogram(
                        "Variations.SafeModeCachedFlags.Engaged", behavior, Behavior.NUM_ENTRIES);
            } else {
                mBehavior.set(Behavior.NOT_ENGAGED_BELOW_THRESHOLD);
                RecordHistogram.recordEnumeratedHistogram(
                        "Variations.SafeModeCachedFlags.Engaged",
                        Behavior.NOT_ENGAGED_BELOW_THRESHOLD,
                        Behavior.NUM_ENTRIES);
            }
        }
    }

    /**
     * Call at an early point in the path that leads to caching flags. If onEndCheckpoint() does not
     * get called before the next run, this run will be considered a crash for purposes of counting
     * the crash streak and entering Safe Mode.
     */
    public void onStartOrResumeCheckpoint() {
        if (mEndCheckpointWritten.get()) {
            // Do not increment the streak if it was already reset.
            return;
        }
        if (mStartCheckpointWritten.getAndSet(true)) {
            // Limit to one increment per run.
            return;
        }

        CachedFlagsSharedPreferences.getInstance()
                .incrementInt(CachedFlagsSharedPreferences.FLAGS_CRASH_STREAK_BEFORE_CACHE);
        RecordHistogram.recordEnumeratedHistogram(
                "Variations.SafeModeCachedFlags.WillCache", mBehavior.get(), Behavior.NUM_ENTRIES);
    }

    /**
     * Call when aborting a path that leads to caching flags. Rolls back the crash streak
     * incremented in {@link #onStartOrResumeCheckpoint} but does not reset it.
     */
    public void onPauseCheckpoint() {
        if (mEndCheckpointWritten.get()) {
            // Do not change the streak if it was already reset.
            return;
        }
        if (!mStartCheckpointWritten.getAndSet(false)) {
            // Do not change the streak if it hasn't been incremented yet.
            return;
        }

        decreaseCrashStreak(1);
        RecordHistogram.recordEnumeratedHistogram(
                "Variations.SafeModeCachedFlags.Pause", mBehavior.get(), Behavior.NUM_ENTRIES);
    }

    private void decreaseCrashStreak(int decrement) {
        int currentStreak =
                CachedFlagsSharedPreferences.getInstance()
                        .readInt(CachedFlagsSharedPreferences.FLAGS_CRASH_STREAK_BEFORE_CACHE);
        assert currentStreak >= 0;

        int newStreak = currentStreak - decrement;
        if (newStreak < 0) newStreak = 0;
        CachedFlagsSharedPreferences.getInstance()
                .writeInt(CachedFlagsSharedPreferences.FLAGS_CRASH_STREAK_BEFORE_CACHE, newStreak);
    }

    /**
     * Call when all flags have been cached. Signals that the current configuration is safe. It will
     * be saved to be used in Safe Mode.
     */
    public void onEndCheckpoint() {
        if (mEndCheckpointWritten.getAndSet(true)) {
            // Limit to one reset per run.
            return;
        }

        if (isInSafeMode()) {
            CachedFlagsSharedPreferences.getInstance()
                    .writeInt(
                            CachedFlagsSharedPreferences.FLAGS_CRASH_STREAK_BEFORE_CACHE,
                            CRASH_STREAK_TO_ENTER_SAFE_MODE - 1);
        } else {
            decreaseCrashStreak(2);
        }

        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                try {
                    writeSafeValues();
                } catch (Exception e) {
                    Log.e(TAG, "Exception writing safe values.", e);
                    cancel(true);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void unused) {
                RecordHistogram.recordEnumeratedHistogram(
                        "Variations.SafeModeCachedFlags.Cached",
                        mBehavior.get(),
                        Behavior.NUM_ENTRIES);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private boolean isInSafeMode() {
        int behavior = mBehavior.get();
        return behavior == Behavior.ENGAGED_WITH_SAFE_VALUES
                || behavior == Behavior.ENGAGED_WITHOUT_SAFE_VALUES
                || behavior == Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES;
    }

    private boolean shouldEnterSafeMode() {
        if (BuildConfig.IS_FOR_TEST
                && (mSafeModeExperimentForcedForTesting == null
                        || !mSafeModeExperimentForcedForTesting)) {
            return false;
        }

        int safeModeRunsLeft =
                CachedFlagsSharedPreferences.getInstance()
                        .readInt(CachedFlagsSharedPreferences.FLAGS_SAFE_MODE_RUNS_LEFT, 0);
        assert safeModeRunsLeft <= 2;

        if (safeModeRunsLeft > 0) {
            CachedFlagsSharedPreferences.getInstance()
                    .writeInt(
                            CachedFlagsSharedPreferences.FLAGS_SAFE_MODE_RUNS_LEFT,
                            safeModeRunsLeft - 1);

            Log.e(TAG, "Enter Safe Mode for CachedFlags, %d runs left.", safeModeRunsLeft);
            return true;
        }

        int crashStreak =
                CachedFlagsSharedPreferences.getInstance()
                        .readInt(CachedFlagsSharedPreferences.FLAGS_CRASH_STREAK_BEFORE_CACHE, 0);
        RecordHistogram.recordExactLinearHistogram(
                "Variations.SafeModeCachedFlags.Streak.Crashes", crashStreak, 50);

        if (crashStreak >= CRASH_STREAK_TO_ENTER_SAFE_MODE) {
            // Run safe mode twice. This run will enter Safe Mode by returning true here. The next
            // run will enter Safe Mode by looking at the FLAGS_SAFE_MODE_RUNS_LEFT SharedPref.
            CachedFlagsSharedPreferences.getInstance()
                    .writeInt(CachedFlagsSharedPreferences.FLAGS_SAFE_MODE_RUNS_LEFT, 1);

            Log.e(TAG, "Enter Safe Mode for CachedFlags, crash streak is %d.", crashStreak);
            return true;
        } else {
            return false;
        }
    }

    @VisibleForTesting
    static SharedPreferences getSafeValuePreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(SAFE_VALUES_FILE, Context.MODE_PRIVATE);
    }

    private void writeSafeValues() {
        TraceEvent.begin("writeSafeValues");
        SharedPreferences.Editor editor = getSafeValuePreferences().edit();

        // Remove values from other versions, since leftover values from previous version are not
        // safe for the current one. Most will get overwritten, but there is no guarantee that all
        // will.
        editor.clear();

        ValuesReturned.dumpToSharedPreferences(editor);
        editor.putString(PREF_SAFE_VALUES_VERSION, VersionInfo.getProductVersion());
        editor.apply();
        TraceEvent.end("writeSafeValues");
    }

    Boolean isEnabled(String featureName, String preferenceName, boolean defaultValue) {
        switch (mBehavior.get()) {
            case Behavior.NOT_ENGAGED_BELOW_THRESHOLD:
                return null;
            case Behavior.ENGAGED_WITH_SAFE_VALUES:
                SharedPreferences prefs = getSafeValuePreferences();
                if (!prefs.contains(preferenceName)) {
                    return null;
                }
                return prefs.getBoolean(preferenceName, false);
            case Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES:
            case Behavior.ENGAGED_WITHOUT_SAFE_VALUES:
                return defaultValue;
            default:
                assert false;
                return null;
        }
    }

    Boolean getBooleanFieldTrialParam(String preferenceName, boolean defaultValue) {
        switch (mBehavior.get()) {
            case Behavior.NOT_ENGAGED_BELOW_THRESHOLD:
                return null;
            case Behavior.ENGAGED_WITH_SAFE_VALUES:
                SharedPreferences prefs = getSafeValuePreferences();
                if (!prefs.contains(preferenceName)) {
                    return null;
                }
                return prefs.getBoolean(preferenceName, false);
            case Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES:
            case Behavior.ENGAGED_WITHOUT_SAFE_VALUES:
                return defaultValue;
            default:
                assert false;
                return null;
        }
    }

    Integer getIntFieldTrialParam(String preferenceName, int defaultValue) {
        switch (mBehavior.get()) {
            case Behavior.NOT_ENGAGED_BELOW_THRESHOLD:
                return null;
            case Behavior.ENGAGED_WITH_SAFE_VALUES:
                SharedPreferences prefs = getSafeValuePreferences();
                if (!prefs.contains(preferenceName)) {
                    return null;
                }
                return prefs.getInt(preferenceName, 0);
            case Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES:
            case Behavior.ENGAGED_WITHOUT_SAFE_VALUES:
                return defaultValue;
            default:
                assert false;
                return null;
        }
    }

    Double getDoubleFieldTrialParam(String preferenceName, double defaultValue) {
        switch (mBehavior.get()) {
            case Behavior.NOT_ENGAGED_BELOW_THRESHOLD:
                return null;
            case Behavior.ENGAGED_WITH_SAFE_VALUES:
                SharedPreferences prefs = getSafeValuePreferences();
                if (!prefs.contains(preferenceName)) {
                    return null;
                }
                long ieee754LongValue = prefs.getLong(preferenceName, 0L);
                return Double.longBitsToDouble(ieee754LongValue);
            case Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES:
            case Behavior.ENGAGED_WITHOUT_SAFE_VALUES:
                return defaultValue;
            default:
                assert false;
                return null;
        }
    }

    String getStringFieldTrialParam(String preferenceName, String defaultValue) {
        switch (mBehavior.get()) {
            case Behavior.NOT_ENGAGED_BELOW_THRESHOLD:
                return null;
            case Behavior.ENGAGED_WITH_SAFE_VALUES:
                SharedPreferences prefs = getSafeValuePreferences();
                if (!prefs.contains(preferenceName)) {
                    return null;
                }
                return prefs.getString(preferenceName, null);
            case Behavior.ENGAGED_IGNORING_OUTDATED_SAFE_VALUES:
            case Behavior.ENGAGED_WITHOUT_SAFE_VALUES:
                return defaultValue;
            default:
                assert false;
                return null;
        }
    }

    @Behavior
    int getBehaviorForTesting() {
        return mBehavior.get();
    }

    void clearMemoryForTesting() {
        mBehavior.set(Behavior.UNKNOWN);
        mStartCheckpointWritten.set(false);
        mEndCheckpointWritten.set(false);
    }

    void enableForTesting() {
        mSafeModeExperimentForcedForTesting = true;
        ResettersForTesting.register(() -> mSafeModeExperimentForcedForTesting = null);
    }
}
