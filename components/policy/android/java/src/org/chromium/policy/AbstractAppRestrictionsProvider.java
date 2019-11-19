// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.StrictMode;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

/**
 * Retrieves app restrictions and provides them to the parent class as Bundles.
 *
 * Needs to be subclassed to specify how to retrieve the restrictions.
 */
public abstract class AbstractAppRestrictionsProvider extends PolicyProvider {
    private static final String TAG = "policy";

    /** {@link Bundle} holding the restrictions to be used during tests. */
    private static Bundle sTestRestrictions;

    private final Context mContext;
    private final BroadcastReceiver mAppRestrictionsChangedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            refresh();
        }
    };

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
     * {@code null} if it is not supported.
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

        mContext.registerReceiver(mAppRestrictionsChangedReceiver,
                new IntentFilter(changeIntentAction), null,
                new Handler(ThreadUtils.getUiThreadLooper()));
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
        long startTime = System.currentTimeMillis();
        final Bundle bundle = getApplicationRestrictions(mContext.getPackageName());
        recordStartTimeHistogram(startTime);
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
        if (getRestrictionChangeIntentAction() != null) {
            mContext.unregisterReceiver(mAppRestrictionsChangedReceiver);
        }
    }

    // Extracted to allow stubbing, since it calls a static that can't easily be stubbed
    @VisibleForTesting
    protected void recordStartTimeHistogram(long startTime) {
        // TODO(aberent): Re-implement once we understand why the previous implementation was giving
        // random crashes (https://crbug.com/535043)
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
        Log.d(TAG, "Test Restrictions: %s",
                (policies == null ? null : policies.keySet().toArray()));
        sTestRestrictions = policies;
    }
}
