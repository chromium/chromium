// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.application;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Workaround for the Android O framework bug described in https://crbug.com/809636 and
 * https://crbug.com/920692.
 *
 * If the application which uses WebView has a preloaded_fonts metadata key in its manifest, the
 * framework's font preloading code will attempt to use the application resource ID specified there
 * to do font preloading in the WebView renderer process, which will fail as the resource will not
 * exist in the WebView APK, resulting in a crash.
 *
 * If Chromium has a preloaded_fonts metadata key in its manifest and the framework's font
 * preloading code attempts to use load it from a renderer process, it will fail on a
 * SecurityException.
 *
 * However, Application.onCreate runs before the font preloading attempt, and can use reflection to
 * install a replacement implementation of IPackageManager into ActivityThread. This replacement
 * will filter out the preloaded_fonts metadata key from any returned results, avoiding the crash.
 */
public class FontPreloadingWorkaround {
    private static final String TAG = "FontWorkaround";
    private static final String FONT_PRELOADING_KEY = "preloaded_fonts";

    /**
     * Try to install the workaround if it's necessary and possible.
     * Must be called from Application.onCreate() to be effective or safe. Do not call from any
     * other thread or location.
     */
    public static void maybeInstallWorkaround(Context appContext) {
        // Only isolated renderer processes running on O devices need this workaround.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O
                || Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                || !ContextUtils.isIsolatedProcess()) {
            return;
        }

        try {
            // The workaround is only needed if the metadata key actually exists for the package
            // name associated with this application context.
            ApplicationInfo appInfo =
                    appContext
                            .getPackageManager()
                            .getApplicationInfo(
                                    appContext.getPackageName(), PackageManager.GET_META_DATA);
            if (appInfo.metaData == null || !appInfo.metaData.containsKey(FONT_PRELOADING_KEY)) {
                return;
            }

            // Retrieve required classes, methods, and fields.
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Method packageManagerGetter = activityThreadClass.getMethod("getPackageManager");
            Field packageManagerField = activityThreadClass.getDeclaredField("sPackageManager");
            packageManagerField.setAccessible(true);
            Class<?> packageManagerInterface = Class.forName("android.content.pm.IPackageManager");
            ClassLoader packageManagerClassLoader = packageManagerInterface.getClassLoader();

            // Retrieve the current IPackageManager instance.
            Object packageManager = packageManagerGetter.invoke(null);

            // Make the proxy.
            Object wrappedPackageManager =
                    Proxy.newProxyInstance(
                            packageManagerClassLoader,
                            new Class[] {packageManagerInterface},
                            new PackageManagerWrapper(packageManager));

            // Replace the real object with the proxy.
            packageManagerField.set(null, wrappedPackageManager);
        } catch (Exception e) {
            // If the reflection or other operations here failed, we can't install the workaround.
            // Just carry on, and hope that it's not necessary for the current device.
            Log.w(TAG, "Installing workaround failed, continuing without", e);
        }
    }

    private static class PackageManagerWrapper implements InvocationHandler {
        Object mRealPackageManager;

        public PackageManagerWrapper(Object realPackageManager) {
            mRealPackageManager = realPackageManager;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            Object result;
            try {
                result = method.invoke(mRealPackageManager, args);
                // If we're calling a method that returns ApplicationInfo, then remove the
                // font preloading metadata from the result to avoid a possible crash.
                if (method.getReturnType() == ApplicationInfo.class) {
                    ApplicationInfo appInfo = (ApplicationInfo) result;
                    if (appInfo.metaData != null) {
                        appInfo.metaData.remove(FONT_PRELOADING_KEY);
                    }
                }
            } catch (InvocationTargetException e) {
                throw e.getTargetException();
            } catch (IllegalAccessException e) {
                throw new RuntimeException("Reflection failed when proxying IPackageManager", e);
            }
            return result;
        }
    }
}
