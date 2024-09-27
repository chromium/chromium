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

/**
 * Static, one-time initialization for the browser process.
 */
public class CastBrowserHelper {
    private static final String TAG = "CastBrowserHelper";

    private static boolean sIsBrowserInitialized;

    /**
     * Starts the browser process synchronously, returning success or failure. If the browser has
     * already started, immediately returns true without performing any more initialization.
     * This may only be called on the UI thread.
     *
     * TODO(sanfin): Remove this overload.
     */    
    public static void initializeBrowser(Context context) {
        initializeBrowser(context, null);
    }
    
    public static void initializeBrowser(Context context, Intent intent) {
        if (sIsBrowserInitialized) return;

        Log.d(TAG, "Performing one-time browser initialization");

        // Initializing the command line must occur before loading the library.
        CastCommandLineHelper.initCommandLine(intent);

        DeviceUtils.addDeviceSpecificUserAgentSwitch();
        LibraryLoader.getInstance().ensureInitialized();

        Log.d(TAG, "Loading BrowserStartupController...");
        BrowserStartupController.getInstance().startBrowserProcessesSync(
                LibraryProcessType.PROCESS_BROWSER, /*singleProcess=*/false,
                /*startGpuProcess=*/false);
        NetworkChangeNotifier.init();
        // Cast shell always expects to receive notifications to track network state.
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        sIsBrowserInitialized = true;
    }
}
