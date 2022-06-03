// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.security_interstitials;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/** Helper class for captive portal related methods on Android. */
@JNINamespace("security_interstitials")
public class CaptivePortalHelper {
    private static final String DEFAULT_PORTAL_CHECK_URL =
            "http://connectivitycheck.gstatic.com/generate_204";

    public static void setCaptivePortalCertificateForTesting(String spkiHash) {
        CaptivePortalHelperJni.get().setCaptivePortalCertificateForTesting(spkiHash);
    }

    public static void setOSReportsCaptivePortalForTesting(boolean osReportsCaptivePortal) {
        CaptivePortalHelperJni.get().setOSReportsCaptivePortalForTesting(osReportsCaptivePortal);
    }

    @CalledByNative
    private static String getCaptivePortalServerUrl() {
        // Since Android N MR2 it is possible that a captive portal was detected with a
        // different URL than getCaptivePortalServerUrl(). By default, Android uses the URL from
        // getCaptivePortalServerUrl() first, but there are also two additional fallback HTTP
        // URLs to probe if the first HTTP probe does not find anything. Using the default URL
        // is acceptable as the return value is only used by the captive portal interstitial.
        try {
            Context context = ContextUtils.getApplicationContext();
            ConnectivityManager connectivityManager =
                    (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
            Method getCaptivePortalServerUrlMethod =
                    connectivityManager.getClass().getMethod("getCaptivePortalServerUrl");
            return (String) getCaptivePortalServerUrlMethod.invoke(connectivityManager);
        } catch (NoSuchMethodException e) {
            // To avoid crashing, return the default portal check URL on Android.
            return DEFAULT_PORTAL_CHECK_URL;
        } catch (IllegalAccessException e) {
            return DEFAULT_PORTAL_CHECK_URL;
        } catch (InvocationTargetException e) {
            return DEFAULT_PORTAL_CHECK_URL;
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    @CalledByNative
    private static void reportNetworkConnectivity() {
        // Call reportNetworkConnectivity on all networks, including the current network.
        Context context = ContextUtils.getApplicationContext();
        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            for (Network network : connectivityManager.getAllNetworks()) {
                // Try both true and false for |hasConnectivity|, that's what reportBadNetwork does.
                // See
                // https://github.com/aosp-mirror/platform_frameworks_base/blob/master/core/java/android/net/ConnectivityManager.java#L2463
                // for details.
                connectivityManager.reportNetworkConnectivity(network, true);
                connectivityManager.reportNetworkConnectivity(network, false);
            }
            return;
        }

        try {
            Class<?> networkClass = Class.forName("android.net.Network");
            Method reportNetworkConnectivityMethod =
                    connectivityManager.getClass().getMethod("reportNetworkConnectivity",
                            Class.forName("android.net.Network"), boolean.class);
            Method getAllNetworksMethod =
                    connectivityManager.getClass().getMethod("getAllNetworks");

            for (Object obj : (Object[]) getAllNetworksMethod.invoke(connectivityManager)) {
                // Try both true and false for |hasConnectivity|, as above.
                reportNetworkConnectivityMethod.invoke(
                        connectivityManager, new Object[] {networkClass.cast(obj), true});
                reportNetworkConnectivityMethod.invoke(
                        connectivityManager, new Object[] {networkClass.cast(obj), false});
            }
        } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException
                | ClassNotFoundException e) {
            // Ignore and do nothing.
        }
    }

    private CaptivePortalHelper() {}

    @NativeMethods
    interface Natives {
        void setCaptivePortalCertificateForTesting(String spkiHash);
        void setOSReportsCaptivePortalForTesting(boolean osReportsCaptivePortal);
    }
}
