// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.app.privatecompute.PccSandboxManager;
import android.os.Build;
import android.os.PersistableBundle;
import android.os.Process;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.ScopedSysTraceEvent;

@JNINamespace("cronet")
final class CronetPccAuditLogger {
    private static final String TAG = "CronetPccAuditLogger";

    interface PccSandboxManagerDelegate {
        boolean isPrivateComputeServicesUid(int uid);

        void writeToAuditLog(PersistableBundle data);
    }

    private static volatile PccSandboxManagerDelegate sPccManagerDelegate =
            new PccSandboxManagerDelegate() {
                @Override
                @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
                public boolean isPrivateComputeServicesUid(int uid) {
                    PccSandboxManager manager = getManager();
                    return manager != null && manager.isPrivateComputeServicesUid(uid);
                }

                @Override
                @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
                public void writeToAuditLog(PersistableBundle data) {
                    PccSandboxManager manager = getManager();
                    if (manager != null) {
                        manager.writeToAuditLog(data);
                    }
                }

                @RequiresApi(Build.VERSION_CODES.CINNAMON_BUN)
                private PccSandboxManager getManager() {
                    return ContextUtils.getApplicationContext()
                            .getSystemService(PccSandboxManager.class);
                }
            };

    @Nullable private static Boolean sIsPrivateComputeUid;

    public static void setPccSandboxManagerDelegateForTesting(PccSandboxManagerDelegate delegate) {
        sPccManagerDelegate = delegate;
    }

    public static void setIsPrivateComputeUidForTesting(@Nullable Boolean isPrivateComputeUid) {
        sIsPrivateComputeUid = isPrivateComputeUid;
    }

    /**
     * Initializes the logger, which allows us to call getSystemService only once and off the
     * critical path.
     *
     * <p>This method should be called once at CronetEngine startup.
     */
    public static void initialize() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN
                || sIsPrivateComputeUid != null) return;

        try (var traceEvent = ScopedSysTraceEvent.scoped("CronetPccAuditLogger#initialize")) {
            int uid = Process.myUid();
            sIsPrivateComputeUid =
                    Process.isPrivateComputeCoreUid(uid)
                            || sPccManagerDelegate.isPrivateComputeServicesUid(uid);
        }
    }

    /**
     * Private compute core (PCC) requests are logged to the PCC audit log.
     *
     * <p>This method does nothing if the current UID is not a Private Compute Core or Services UID.
     * The audit mode needs to be enabled for the log to be written, but there is intentionally no
     * public API to check if the mode is enabled.
     */
    @CalledByNative
    public static void maybeWrite(@JniType("std::string") String url) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN) return;

        if (sIsPrivateComputeUid == null) {
            throw new IllegalStateException("CronetPccAuditLogger not initialized");
        }
        if (!sIsPrivateComputeUid) return;

        try (var traceEvent = ScopedSysTraceEvent.scoped("CronetPccAuditLogger#maybeWrite")) {
            PersistableBundle data = new PersistableBundle();
            data.putLong("timestamp", System.currentTimeMillis());
            data.putString("url", url);
            sPccManagerDelegate.writeToAuditLog(data);
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to write to PCC audit log", e);
        }
    }
}
