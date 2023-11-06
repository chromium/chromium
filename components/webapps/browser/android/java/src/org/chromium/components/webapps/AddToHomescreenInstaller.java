// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides functionality related to native Android apps for its C++ counterpart,
 * add_to_homescreen_installer.cc.
 */
class AddToHomescreenInstaller {
    private static final String TAG = "AddToHomescreen";

    @CalledByNative
    private static boolean installOrOpenNativeApp(WebContents webContents, AppData appData) {
        Context context = ContextUtils.getApplicationContext();
        Intent launchIntent;
        if (PackageUtils.isPackageInstalled(appData.packageName())) {
            launchIntent =
                    context.getPackageManager().getLaunchIntentForPackage(appData.packageName());
        } else {
            launchIntent = appData.installIntent();
        }

        if (launchIntent != null) {
            WindowAndroid window = webContents.getTopLevelNativeWindow();
            Context intentLauncher = window == null ? null : window.getActivity().get();
            if (intentLauncher != null) {
                try {
                    intentLauncher.startActivity(launchIntent);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "Failed to install or open app : %s!", appData.packageName(), e);
                    return false;
                }
            }
        }

        return true;
    }
}
