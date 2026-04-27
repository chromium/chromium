// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.StrictMode;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Retrieves app restrictions and provides them to the parent class as Bundles.
 *
 * <p>Needs to be subclassed to specify how to retrieve the restrictions.
 */
@NullMarked
public abstract class AbstractAppRestrictionsProvider extends PolicyProvider {
    private static final String TAG = "policy";

    // Delay after which a watchdog task logs a warning if registerReceiver() has not yet
    // returned. Picked at the system ANR window to surface IPC stalls that, in the original
    // synchronous implementation, would have caused user-visible jank.
    private static final long REGISTER_RECEIVER_WATCHDOG_MS = 15_000;

    /** {@link Bundle} holding the restrictions to be used during tests. */
    private static @Nullable Bundle sTestRestrictions;

    private final Context mContext;
    private final BroadcastReceiver mAppRestrictionsChangedReceiver =
            new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    refresh();
                }
            };

    // Register/unregister are posted to this runner to avoid blocking the UI thread with the
    // synchronous Binder IPC inside Context.registerReceiver(). Both operations are serialized
    // on the same sequence so stopListening() posted before register completes still runs after.
    private final SequencedTaskRunner mTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    // Set on the calling thread when stopListening()/destroy() is called. Read on mTaskRunner
    // after the async register IPC returns, so we can skip the post-register catch-up refresh
    // if the provider is already being torn down. The unregister task is queued behind the
    // register task on the same SequencedTaskRunner and will tear down the receiver itself.
    private volatile boolean mListeningStopped;

    /**
     * @param context The application context.
     */
    public AbstractAppRestrictionsProvider(Context context) {
        mContext = context;
    }

    /**
     * @return The restrictions for the provided package name, an empty bundle if they are not
     * available.
     */
    protected abstract Bundle getApplicationRestrictions(String packageName);

    /**
     * @return The intent action to listen to to be notified of restriction changes,
     * {@code null} if it is not supported. The action will/must be a protected broadcast action.
     */
    protected abstract String getRestrictionChangeIntentAction();

    /**
     * Start listening for restrictions changes. Does nothing if this is not supported by the
     * platform.
     */
    @Override
    public void startListeningForPolicyChanges() {
        String changeIntentAction = getRestrictionChangeIntentAction();
        if (changeIntentAction == null) return;

        AtomicBoolean registerSucceeded = new AtomicBoolean(false);

        mTaskRunner.postDelayedTask(
                () -> {
                    long startMs = SystemClock.elapsedRealtime();
                    ContextUtils.registerProtectedBroadcastReceiver(
                            mContext,
                            mAppRestrictionsChangedReceiver,
                            new IntentFilter(changeIntentAction),
                            new Handler(ThreadUtils.getUiThreadLooper()));
                    long durationMs = SystemClock.elapsedRealtime() - startMs;
                    registerSucceeded.set(true);
                    RecordHistogram.recordMediumTimesHistogram(
                            "Enterprise.Policy.AppRestrictionsRegisterReceiverTime", durationMs);
                    Log.i(TAG, "registerReceiver succeeded after %dms", durationMs);

                    // If stop/destroy was called while we were registering, skip the catch-up
                    // refresh. The unregister task is already queued behind us on the same
                    // SequencedTaskRunner and will tear down the receiver.
                    if (mListeningStopped) return;

                    // Catch any policy change that arrived between the synchronous
                    // refresh() at init time and the completion of registerReceiver().
                    PostTask.postTask(TaskTraits.UI_DEFAULT, () -> refresh());
                },
                0);

        // Watchdog: if registerReceiver() has not returned within the ANR window, log a
        // warning. Posted to UI_DEFAULT (a different queue from mTaskRunner) so the watchdog
        // is not serialized behind the register task itself.
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (!registerSucceeded.get()) {
                        Log.w(
                                TAG,
                                "registerReceiver still pending after %dms",
                                REGISTER_RECEIVER_WATCHDOG_MS);
                    }
                },
                REGISTER_RECEIVER_WATCHDOG_MS);
    }

    /**
     * Retrieve the restrictions. {@link #notifySettingsAvailable(Bundle)} will be called as a
     * result.
     */
    @Override
    public void refresh() {
        if (sTestRestrictions != null) {
            notifySettingsAvailable(sTestRestrictions);
            return;
        }

        // Because some policies are needed during startup this has to be synchronous. There is
        // no way of reading policies (or cached policies from a previous run) without doing
        // a disk read, so we have to disable strict mode here.
        StrictMode.ThreadPolicy policy = StrictMode.allowThreadDiskReads();
        final Bundle bundle = getApplicationRestrictions(mContext.getPackageName());
        StrictMode.setThreadPolicy(policy);

        notifySettingsAvailable(bundle);
    }

    @Override
    public void destroy() {
        stopListening();
        super.destroy();
    }

    /**
     * Stop listening for restrictions changes. Does nothing if this is not supported by the
     * platform.
     */
    public void stopListening() {
        if (getRestrictionChangeIntentAction() == null) return;

        mListeningStopped = true;
        mTaskRunner.postDelayedTask(
                () -> mContext.unregisterReceiver(mAppRestrictionsChangedReceiver), 0);
    }

    /**
     * Restrictions to be used during tests. Subsequent attempts to retrieve the restrictions will
     * return the provided bundle instead.
     *
     * Chrome and WebView tests are set up to use annotations for policy testing and reset the
     * restrictions to an empty bundle if nothing is specified. To stop using a test bundle,
     * provide {@code null} as value instead.
     */
    @VisibleForTesting
    public static void setTestRestrictions(Bundle policies) {
        Log.d(
                TAG,
                "Test Restrictions: %s",
                (policies == null ? null : policies.keySet().toArray()));
        sTestRestrictions = policies;
    }

    /** Returns whether any restrictions were set using {@link #setTestRestrictions}. */
    public static boolean hasTestRestrictions() {
        return sTestRestrictions != null;
    }
}
