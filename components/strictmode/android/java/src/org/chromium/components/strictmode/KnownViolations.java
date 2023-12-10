// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.strictmode;

import static org.chromium.components.strictmode.Violation.DETECT_DISK_IO;
import static org.chromium.components.strictmode.Violation.DETECT_DISK_READ;
import static org.chromium.components.strictmode.Violation.DETECT_DISK_WRITE;
import static org.chromium.components.strictmode.Violation.DETECT_RESOURCE_MISMATCH;

import android.os.Build;

import java.util.Locale;

/**
 * Collection of known unfixable StrictMode violations. This list should stay in sync with the
 * list for other apps (http://go/chrome-known-violations-upstream). Add Chrome-specific exemptions
 * to {@link ChromeStrictMode}.
 */
public final class KnownViolations {
    public static ThreadStrictModeInterceptor.Builder addExemptions(
            ThreadStrictModeInterceptor.Builder builder) {
        applyManufacturer(builder);
        applyVendor(builder);
        applyPlatform(builder);
        return builder;
    }

    private static void applyManufacturer(ThreadStrictModeInterceptor.Builder exemptions) {
        String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.US);
        String model = Build.MODEL.toLowerCase(Locale.US);
        switch (manufacturer) {
            case "samsung":
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ | DETECT_DISK_WRITE,
                        "android.util.GeneralUtil#isSupportedGloveModeInternal");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "android.graphics.Typeface#SetAppTypeFace");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "android.graphics.Typeface#setAppTypeFace");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ,
                        "android.app.ApplicationPackageManager#queryIntentActivities");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "android.app.ActivityThread#parseCSCAppResource");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "android.app.ActivityThread#performLaunchActivity");
                exemptions.ignoreExternalMethod(DETECT_DISK_READ, "android.widget.Toast#makeText");
                exemptions.ignoreExternalMethod(DETECT_DISK_READ, "android.widget.Toast#show");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ,
                        "com.samsung.android.knox.custom.ProKioskManager#getProKioskState");
                if (model.equals("sm-g9350")) {
                    exemptions.ignoreExternalMethod(
                            DETECT_DISK_WRITE, "android.content.res.Resources#loadDrawable");
                }
                if (model.equals("sm-j700f") && Build.VERSION.SDK_INT == 23) {
                    exemptions.ignoreExternalMethod(
                            DETECT_DISK_IO, "android.content.res.Resources#loadDrawable");
                    exemptions.ignoreExternalMethod(
                            DETECT_DISK_WRITE, "android.app.ActivityThread#performLaunchActivity");
                }
                if (Build.VERSION.SDK_INT <= 27) {
                    exemptions.ignoreExternalMethod(
                            DETECT_DISK_READ,
                            "com.android.server.am.ActivityManagerService#startActivity");
                }
                break;
            case "oneplus":
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ | DETECT_DISK_WRITE,
                        "com.android.server.am.ActivityManagerService#checkProcessExist");
                break;
            case "vivo":
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "android.content.res.VivoResources#loadThemeValues");
                break;
            case "xiaomi":
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "com.android.internal.policy.PhoneWindow#getDecorView");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_WRITE, "miui.content.res.ThemeResourcesSystem#checkUpdate");
                exemptions.ignoreExternalMethod(
                        DETECT_DISK_READ, "android.util.BoostFramework#<init>");
                break;
            default:
                // fall through
        }
    }

    private static void applyVendor(ThreadStrictModeInterceptor.Builder exemptions) {
        exemptions.ignoreExternalMethod(DETECT_DISK_READ, "com.qualcomm.qti.Performance#<clinit>");
    }

    private static void applyPlatform(ThreadStrictModeInterceptor.Builder exemptions) {
        exemptions.ignoreExternalMethod(
                DETECT_DISK_READ, "com.android.messageformat.MessageFormat#formatNamedArgs");
        exemptions.ignoreExternalMethod(
                DETECT_RESOURCE_MISMATCH, "com.android.internal.widget.SwipeDismissLayout#init");
        exemptions.ignoreExternalMethod(DETECT_DISK_IO, "java.lang.ThreadGroup#uncaughtException");
        exemptions.ignoreExternalMethod(DETECT_DISK_IO, "android.widget.VideoView#openVideo");
        exemptions.ignoreExternalMethod(
                DETECT_DISK_IO,
                "com.android.server.inputmethod.InputMethodManagerService#startInputOrWindowGainedFocus");
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            exemptions.ignoreExternalMethod(
                    DETECT_DISK_WRITE,
                    "com.android.server.clipboard.HostClipboardMonitor#setHostClipboard");
        } else {
            exemptions.ignoreExternalMethod(
                    DETECT_DISK_WRITE, "android.content.ClipboardManager#setPrimaryClip");
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
            exemptions.ignoreExternalMethod(DETECT_DISK_READ, "dalvik.system.DexPathList#toString");
        }
    }

    private KnownViolations() {}
}
