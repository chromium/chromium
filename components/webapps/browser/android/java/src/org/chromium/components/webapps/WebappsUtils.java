// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.annotation.WorkerThread;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.ui.widget.Toast;

import java.util.List;

/** Contains utilities for Web Apps and homescreen shortcuts. */
public class WebappsUtils {
    private static final String TAG = "WebappsUtils";

    private static final String INSTALL_SHORTCUT = "com.android.launcher.action.INSTALL_SHORTCUT";

    // True when Android O's ShortcutManager.requestPinShortcut() is supported.
    private static volatile boolean sIsRequestPinShortcutSupported;

    // True when it is already checked if ShortcutManager.requestPinShortcut() is supported.
    private static volatile boolean sCheckedIfRequestPinShortcutSupported;

    // Synchronization locks for thread-safe access to variables
    // sCheckedIfRequestPinShortcutSupported and sIsRequestPinShortcutSupported.
    private static final Object sLock = new Object();

    /**
     * Creates an intent that will add a shortcut to the home screen.
     *
     * @param title Title of the shortcut.
     * @param icon Image that represents the shortcut.
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
     * Request Android to add a shortcut to the home screen.
     *
     * @param id The generated GUID of the shortcut.
     * @param title Title of the shortcut.
     * @param icon Image that represents the shortcut.
     * @param isIconAdaptive Whether to create an Android Adaptive icon.
     * @param shortcutIntent Intent to fire when the shortcut is activated.
     */
    public static void addShortcutToHomescreen(
            String id, String title, Bitmap icon, boolean isIconAdaptive, Intent shortcutIntent) {
        if (isRequestPinShortcutSupported()) {
            addShortcutWithShortcutManager(id, title, icon, isIconAdaptive, shortcutIntent);
            return;
        }

        Intent intent = createAddToHomeIntent(title, icon, shortcutIntent);
        ContextUtils.getApplicationContext().sendBroadcast(intent);
        showAddedToHomescreenToast(title);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    public static void addShortcutWithShortcutManager(
            String id, String title, Bitmap bitmap, boolean isMaskableIcon, Intent shortcutIntent) {
        Context context = ContextUtils.getApplicationContext();

        if (bitmap == null) {
            Log.e(TAG, "Failed to find an icon for " + title + ", not adding.");
            return;
        }
        Icon icon =
                isMaskableIcon
                        ? Icon.createWithAdaptiveBitmap(bitmap)
                        : Icon.createWithBitmap(bitmap);

        ShortcutInfo shortcutInfo =
                new ShortcutInfo.Builder(context, id)
                        .setShortLabel(title)
                        .setLongLabel(title)
                        .setIcon(icon)
                        .setIntent(shortcutIntent)
                        .build();
        try {
            ShortcutManager shortcutManager = context.getSystemService(ShortcutManager.class);
            shortcutManager.requestPinShortcut(shortcutInfo, null);
        } catch (IllegalStateException e) {
            Log.d(
                    TAG,
                    "Could not create pinned shortcut: device is locked, or "
                            + "activity is backgrounded.");
        }
    }

    /** Show toast to alert user that the shortcut was added to the home screen. */
    private static void showAddedToHomescreenToast(final String title) {
        Context applicationContext = ContextUtils.getApplicationContext();
        String toastText = applicationContext.getString(R.string.added_to_homescreen, title);
        showToast(toastText);
    }

    public static void showToast(String text) {
        assert ThreadUtils.runningOnUiThread();
        Toast toast =
                Toast.makeText(ContextUtils.getApplicationContext(), text, Toast.LENGTH_SHORT);
        toast.show();
    }

    /**
     * Shows toast notifying user of the result of a WebAPK install if the installation was not
     * successful.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static void showWebApkInstallResultToast(@WebApkInstallResult int result) {
        Context applicationContext = ContextUtils.getApplicationContext();
        if (result == WebApkInstallResult.INSTALL_ALREADY_IN_PROGRESS) {
            showToast(applicationContext.getString(R.string.webapk_install_in_progress));
        } else if (result != WebApkInstallResult.SUCCESS) {
            showToast(applicationContext.getString(R.string.webapk_install_failed));
        }
    }

    /**
     * Utility method to check if a shortcut can be added to the home screen.
     *
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

    /** Prepares whether Android O's ShortcutManager.requestPinShortcut() is supported. */
    @WorkerThread
    public static void prepareIsRequestPinShortcutSupported() {
        isRequestPinShortcutSupported();
    }

    /** Returns whether Android O's ShortcutManager.requestPinShortcut() is supported. */
    public static boolean isRequestPinShortcutSupported() {
        if (!sCheckedIfRequestPinShortcutSupported) {
            synchronized (sLock) {
                if (!sCheckedIfRequestPinShortcutSupported) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        ShortcutManager shortcutManager =
                                ContextUtils.getApplicationContext()
                                        .getSystemService(ShortcutManager.class);
                        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                            sIsRequestPinShortcutSupported =
                                    shortcutManager != null
                                            && shortcutManager.isRequestPinShortcutSupported();
                        }
                    }
                    sCheckedIfRequestPinShortcutSupported = true;
                }
            }
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

    /**
     * Override whether shortcuts are considered supported for testing.
     *
     * @param supported Whether shortcuts are supported. Pass null to reset.
     */
    public static void setAddToHomeIntentSupportedForTesting(Boolean supported) {
        if (supported == null) {
            sCheckedIfRequestPinShortcutSupported = false;
            sIsRequestPinShortcutSupported = false;
        } else {
            sCheckedIfRequestPinShortcutSupported = true;
            sIsRequestPinShortcutSupported = supported.booleanValue();
        }
    }
}
