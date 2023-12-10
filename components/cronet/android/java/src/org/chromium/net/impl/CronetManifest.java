// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.net.impl.CronetLogger.CronetSource;

/**
 * Utilities for working with Cronet Android manifest flags.
 *
 * Cronet manifest flags must be defined within a service definition named after {@link
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
    static final String ENABLE_TELEMETRY_META_DATA_KEY = "android.net.http.EnableTelemetry";

    // DO NOT ENABLE this manifest flag in production apps. The code gated behind this flag is not
    // ready yet.
    // TODO: remove the "Experimental" prefix once the code for reading HTTP flags is ready.
    @VisibleForTesting
    public static final String READ_HTTP_FLAGS_META_DATA_KEY =
            "android.net.http.EXPERIMENTAL_ReadHttpFlags";

    /**
     * @return True if telemetry should be enabled, based on the {@link
     * #ENABLE_TELEMETRY_META_DATA_KEY} meta-data entry in the Android manifest.
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
     * @return True if HTTP flags (typically used for experiments) should be enabled, based on the
     * {@link #READ_HTTP_FLAGS_META_DATA_KEY} meta-data entry in the Android manifest.
     * @see HttpFlagsLoader
     */
    public static boolean shouldReadHttpFlags(Context context) {
        // TODO: switch the default to true once we confirm the HTTP flags system is working as
        // intended.
        return getMetaData(context).getBoolean(READ_HTTP_FLAGS_META_DATA_KEY, /* default= */ false);
    }

    /**
     * @return The meta-data contained within the Cronet meta-data holder service definition in the
     * Android manifest, or an empty Bundle if there is no such definition. Never returns null.
     */
    private static Bundle getMetaData(Context context) {
        ServiceInfo serviceInfo;
        try {
            serviceInfo =
                    context.getPackageManager()
                            .getServiceInfo(
                                    new ComponentName(context, META_DATA_HOLDER_SERVICE_NAME),
                                    PackageManager.GET_META_DATA
                                            | PackageManager.MATCH_DISABLED_COMPONENTS
                                            | PackageManager.MATCH_DIRECT_BOOT_AWARE
                                            | PackageManager.MATCH_DIRECT_BOOT_UNAWARE);
        } catch (PackageManager.NameNotFoundException e) {
            serviceInfo = null;
        }
        return serviceInfo != null ? serviceInfo.metaData : new Bundle();
    }
}
