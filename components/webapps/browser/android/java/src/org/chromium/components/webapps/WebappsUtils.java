// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.os.Build;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.webapk.lib.client.WebApkValidator;

import java.util.List;

/**
 * Contains utilities for Web Apps and homescreen shortcuts.
 */
public class WebappsUtils {
    private static final String INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";

    // True when Android O's ShortcutManager.requestPinShortcut() is supported.
    private static boolean sIsRequestPinShortcutSupported;

    // True when it is already checked if ShortcutManager.requestPinShortcut() is supported.
    private static boolean sCheckedIfRequestPinShortcutSupported;

    /**
     * Creates an intent that will add a shortcut to the home screen.
     * @param title          Title of the shortcut.
     * @param icon           Image that represents the shortcut.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     * @return Intent for the shortcut.
     */
    public static Intent createAddToHomeIntent(String title, Bitmap icon, Intent shortcutIntent) {
        Intent i = new Intent(INSTALL_SHORTCUT);
        i.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        i.putExtra(Intent.EXTRA_SHORTCUT_NAME, title);
        i.putExtra(Intent.EXTRA_SHORTCUT_ICON, icon);
        return i;
    }

    /**
     * Utility method to check if a shortcut can be added to the home screen.
     * @return if a shortcut can be added to the home screen under the current profile.
     */
    @SuppressLint("WrongConstant")
    public static boolean isAddToHomeIntentSupported() {
        if (isRequestPinShortcutSupported()) return true;
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        Intent i = new Intent(INSTALL_SHORTCUT);
        List<ResolveInfo> receivers =
                pm.queryBroadcastReceivers(i, PackageManager.GET_INTENT_FILTERS);
        return !receivers.isEmpty();
    }

    /** Returns whether Android O's ShortcutManager.requestPinShortcut() is supported. */
    public static boolean isRequestPinShortcutSupported() {
        if (!sCheckedIfRequestPinShortcutSupported) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                checkIfRequestPinShortcutSupported();
            }
            sCheckedIfRequestPinShortcutSupported = true;
        }
        return sIsRequestPinShortcutSupported;
    }

    /**
     * Returns the package name of one of the WebAPKs which can handle {@link url}. Returns null if
     * there are no matches.
     */
    @CalledByNative
    private static String queryFirstWebApkPackage(String url) {
        return WebApkValidator.queryFirstWebApkPackage(ContextUtils.getApplicationContext(), url);
    }

    @TargetApi(Build.VERSION_CODES.O)
    private static void checkIfRequestPinShortcutSupported() {
        ShortcutManager shortcutManager =
                ContextUtils.getApplicationContext().getSystemService(ShortcutManager.class);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            sIsRequestPinShortcutSupported = shortcutManager.isRequestPinShortcutSupported();
        }
    }
}
