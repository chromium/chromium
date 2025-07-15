// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.net.impl.CronetLogger.CronetSource;

/**
 * Utilities for working with Cronet Android manifest flags.
 *
 * <p>Cronet manifest flags must be defined within a service definition named after {@link
 * #META_DATA_HOLDER_SERVICE_NAME} (the reason this is not defined at the application level is to
 * avoid scalability issues with PackageManager queries). For example, to enable telemetry, add the
 * following to {@code AndroidManifest.xml}:
 *
 * <pre>{@code
 * <manifest ...>
 *   ...
 *   <application ...>
 *     ...
 *     <service android:name="android.net.http.MetaDataHolder"
 *              android:enabled="false" android:exported="false">
 *       <meta-data android:name="android.net.http.EnableTelemetry"
 *                  android:value="true" />
 *     </service>
 *   </application>
 * </manifest>
 * }</pre>
 */
@VisibleForTesting
public final class CronetManifest {
    private CronetManifest() {}

    @VisibleForTesting
    static final String META_DATA_HOLDER_SERVICE_NAME = "android.net.http.MetaDataHolder";

    @VisibleForTesting
    public static final String ENABLE_TELEMETRY_META_DATA_KEY = "android.net.http.EnableTelemetry";

    @VisibleForTesting
    public static final String READ_HTTP_FLAGS_META_DATA_KEY = "android.net.http.ReadHttpFlags";

    @VisibleForTesting
    public static final String USE_PERFETTO_META_DATA_KEY = "android.net.http.UsePerfetto";

    /**
     * @return True if telemetry should be enabled, based on the {@link
     *     #ENABLE_TELEMETRY_META_DATA_KEY} meta-data entry in the Android manifest.
     */
    public static boolean isAppOptedInForTelemetry(Context context, CronetSource source) {
        boolean telemetryIsDefaultEnabled =
                source == CronetSource.CRONET_SOURCE_PLATFORM
                        || source == CronetSource.CRONET_SOURCE_PLAY_SERVICES;
        return getMetaData(context)
                .getBoolean(
                        ENABLE_TELEMETRY_META_DATA_KEY, /* default= */ telemetryIsDefaultEnabled);
    }

    /**
     * Same as above, for the case where the source is not known (e.g. because one has not been
     * selected yet). If you know the source, use the other method as it gives a more accurate
     * result.
     */
    public static boolean isAppOptedInForTelemetry(Context context) {
        // Look for in-app native Cronet; if we can find it, assume the app is going to use its own
        // Cronet, and set the telemetry default to disabled in this case. Note this is merely a
        // heuristic, as the app could decide to ignore its own Cronet and use one from another
        // source. But we have no way to know, so we err on the safe side. See also
        // https://crbug.com/430895740.
        boolean hasNativeCronet = true;
        try {
            Class.forName(
                    "org.chromium.net.impl.NativeCronetEngineBuilderImpl",
                    /* initialize= */ false,
                    CronetManifest.class.getClassLoader());
        } catch (ClassNotFoundException e) {
            hasNativeCronet = false;
        }
        return getMetaData(context)
                .getBoolean(ENABLE_TELEMETRY_META_DATA_KEY, /* default= */ !hasNativeCronet);
    }

    /**
     * @return True if HTTP flags (typically used for experiments) should be enabled, based on the
     *     {@link #READ_HTTP_FLAGS_META_DATA_KEY} meta-data entry in the Android manifest.
     * @see HttpFlagsLoader
     */
    public static boolean shouldReadHttpFlags(Context context) {
        return getMetaData(context).getBoolean(READ_HTTP_FLAGS_META_DATA_KEY, /* default= */ true);
    }

    public static boolean shouldUsePerfetto(Context context) {
        return getMetaData(context).getBoolean(USE_PERFETTO_META_DATA_KEY, /* default= */ true);
    }

    private static final Object sLock = new Object();

    // Leaking this is fine because this is storing an application context which lives for the
    // entire process duration anyway.
    @SuppressLint("StaticFieldLeak")
    private static Context sLastContext;

    private static Bundle sMetaData;

    /**
     * @return The meta-data contained within the Cronet meta-data holder service definition in the
     *     Android manifest, or an empty Bundle if there is no such definition. Never returns null.
     */
    private static Bundle getMetaData(Context context) {
        // Make sure we don't create a memory leak by only caching the application context, not a
        // local short-lived context.
        context = context.getApplicationContext();
        synchronized (sLock) {
            // If we are being asked for the meta-data again for the same Context, assume the answer
            // will be the same and serve a cached result. This is deemed safe because manifests are
            // not supposed to change over the lifetime of the app, and this makes the code
            // considerably more efficient because PackageManager calls are expensive (they involve
            // an IPC to the system server). See also https://crbug.com/346546533.
            if (context != sLastContext) {
                try (var traceEvent =
                        ScopedSysTraceEvent.scoped("CronetManifest#getMetaData fetching info")) {
                    ServiceInfo serviceInfo;
                    try {
                        serviceInfo =
                                context.getPackageManager()
                                        .getServiceInfo(
                                                new ComponentName(
                                                        context, META_DATA_HOLDER_SERVICE_NAME),
                                                PackageManager.GET_META_DATA
                                                        | PackageManager.MATCH_DISABLED_COMPONENTS
                                                        | PackageManager.MATCH_DIRECT_BOOT_AWARE
                                                        | PackageManager.MATCH_DIRECT_BOOT_UNAWARE);
                    } catch (PackageManager.NameNotFoundException | NullPointerException e) {
                        // TODO(b/331573772): Consider removing this NPE check once we can check for
                        // CRONET_SOURCE_FAKE when creating logger.
                        serviceInfo = null;
                    }
                    sMetaData =
                            serviceInfo != null && serviceInfo.metaData != null
                                    ? serviceInfo.metaData
                                    : new Bundle();
                    sLastContext = context;
                }
            }
            assert sMetaData != null;
            return sMetaData;
        }
    }

    @VisibleForTesting
    public static void resetCache() {
        sMetaData = null;
        sLastContext = null;
    }
}
