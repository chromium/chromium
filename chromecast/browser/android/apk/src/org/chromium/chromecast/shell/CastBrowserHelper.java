// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.net.NetworkChangeNotifier;

/** Static, one-time initialization for the browser process. */
public class CastBrowserHelper {
    private static final String TAG = "CastBrowserHelper";

    private static boolean sIsBrowserInitialized;

    public static void initializeBrowserAsync(Context context, Intent intent) {
        if (sIsBrowserInitialized) {
            return;
        }

        Log.d(TAG, "Performing one-time browser initialization asynchronously");

        CastCommandLineHelper.initCommandLine(intent);
        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        LibraryLoader.getInstance().ensureInitialized();

        Log.d(TAG, "Loading BrowserStartupController...");
        BrowserStartupController.getInstance()
                .startBrowserProcessesAsync(
                        LibraryProcessType.PROCESS_BROWSER,
                        /* startGpuProcess= */ false,
                        /* startMinimalBrowser= */ false,
                        new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess() {
                                Log.e(TAG, "Browser initialization succeeded");
                                NetworkChangeNotifier.init();
                                // Cast shell always expects to receive notifications to track
                                // network state.
                                NetworkChangeNotifier.registerToReceiveNotificationsAlways();
                                sIsBrowserInitialized = true;
                            }

                            @Override
                            public void onFailure() {
                                Log.e(TAG, "Browser initialization failed");
                                sIsBrowserInitialized = false;
                            }
                        });
    }
}
