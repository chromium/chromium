// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.pm.ApplicationInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.net.NetLogCaptureMode;

/**
 * Helper class to manage Cronet debug flags set via system properties.
 *
 * <p>This class caches system properties values and will therefore not react to changes that occur
 * after the property has been read. The app needs to be restarted for any subsequent changes to be
 * picked up.
 */
class DebugFlags {
    private static final String TAG = "DebugFlags";

    @VisibleForTesting
    // LINT.IfChange(trace_netlog_property)
    static final String TRACE_NET_LOG_SYSTEM_PROPERTY_KEY = "debug.cronet.trace_netlog";

    // LINT.ThenChange(//components/cronet/android/test/javatests/src/org/chromium/net/CronetUrlRequestContextTest.java:trace_netlog_property)

    // These fields are not thread-safe.
    private static @NetLogCaptureMode Integer sTraceNetLogCaptureMode;
    private static Boolean sIsDebuggableBuild;

    private DebugFlags() {}

    static boolean isDebuggable() {
        if (sIsDebuggableBuild == null) {
            final var buildType = AndroidOsBuild.get().getType();
            sIsDebuggableBuild =
                    buildType.equals("userdebug")
                            || buildType.equals("eng")
                            || (ContextUtils.getApplicationContext().getApplicationInfo().flags
                                            & ApplicationInfo.FLAG_DEBUGGABLE)
                                    != 0;
        }
        return sIsDebuggableBuild;
    }

    static @NetLogCaptureMode int getTraceNetLogCaptureMode() {
        if (sTraceNetLogCaptureMode == null) {
            @NetLogCaptureMode int mode = NetLogCaptureMode.HEAVILY_REDACTED;
            var requested =
                    AndroidOsSystemProperties.get(
                            TRACE_NET_LOG_SYSTEM_PROPERTY_KEY, "heavily_redacted");
            if (requested.equals("heavily_redacted")) {
                mode = NetLogCaptureMode.HEAVILY_REDACTED;
            } else if (requested.equals("on")) {
                // Note DEFAULT is mapped to "on", not "default", to avoid confusion with regard to
                // the default value of the system property.
                mode = NetLogCaptureMode.DEFAULT;
            } else if (requested.equals("include_sensitive")) {
                mode = NetLogCaptureMode.INCLUDE_SENSITIVE;
            } else if (requested.equals("everything")) {
                mode = NetLogCaptureMode.EVERYTHING;
            } else {
                Log.w(
                        TAG,
                        "Unknown value for %s system property, ignoring: %s",
                        TRACE_NET_LOG_SYSTEM_PROPERTY_KEY,
                        requested);
            }

            if (mode > NetLogCaptureMode.HEAVILY_REDACTED && !isDebuggable()) {
                Log.w(
                        TAG,
                        "Ignoring requested Cronet trace netlog capture mode (%s=%s) because"
                                + " neither the device nor app are debuggable",
                        TRACE_NET_LOG_SYSTEM_PROPERTY_KEY,
                        requested);
                mode = NetLogCaptureMode.HEAVILY_REDACTED;
            }
            sTraceNetLogCaptureMode = mode;
        }
        return sTraceNetLogCaptureMode;
    }

    static void resetForTesting() {
        sTraceNetLogCaptureMode = null;
        sIsDebuggableBuild = null;
    }
}
