// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.security_interstitials;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/** Helper class for captive portal related methods on Android. */
@JNINamespace("security_interstitials")
public class CaptivePortalHelper {
    private static final String DEFAULT_PORTAL_CHECK_URL =
            "http://connectivitycheck.gstatic.com/generate_204";

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

    @CalledByNative
    private static void reportNetworkConnectivity() {
        // Call reportNetworkConnectivity on all networks, including the current network.
        Context context = ContextUtils.getApplicationContext();
        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);

        for (Network network : connectivityManager.getAllNetworks()) {
            // Try both true and false for |hasConnectivity|, that's what reportBadNetwork does.
            // See
            // https://github.com/aosp-mirror/platform_frameworks_base/blob/master/core/java/android/net/ConnectivityManager.java#L2463
            // for details.
            connectivityManager.reportNetworkConnectivity(network, true);
            connectivityManager.reportNetworkConnectivity(network, false);
        }
    }

    private CaptivePortalHelper() {}

    @NativeMethods
    interface Natives {
        void setOSReportsCaptivePortalForTesting(boolean osReportsCaptivePortal);
    }
}
