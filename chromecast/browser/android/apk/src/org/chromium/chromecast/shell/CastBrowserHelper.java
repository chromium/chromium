// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;

import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
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
    private static final String COMMAND_LINE_FILE = "castshell-command-line";
    private static final String CAST_BROWSER_LIB = "cast_browser_android";

    private static boolean sIsBrowserInitialized;

    /**
     * Starts the browser process synchronously, returning success or failure. If the browser has
     * already started, immediately returns true without performing any more initialization.
     * This may only be called on the UI thread.
     *
     * @return whether or not the process started successfully
     */
    public static void initializeBrowser(Context context) {
        if (sIsBrowserInitialized) return;

        Log.d(TAG, "Performing one-time browser initialization");

        // Initializing the command line must occur before loading the library.
        CastCommandLineHelper.initCommandLineWithSavedArgs(() -> {
            CommandLineInitUtil.initCommandLine(COMMAND_LINE_FILE);
            return CommandLine.getInstance();
        });

        DeviceUtils.addDeviceSpecificUserAgentSwitch();

        if (BundleUtils.isBundle()) {
            // CommandLine.java doesn't expect there to be two copies of //base and it ignores
            // the second attempt to initialize the native command line.
            // LibraryLoader.ensureInitialized() is not called because it loads the main
            // shared lib (libcast_service) and initializes the native command line in its copy of
            // //base, which leaves the native command line in the libcast_browser_android's copy of
            // //base uninitialized. The end state of the following two lines is
            // libcast_browser_android's native command line is initialized and libcast_service's
            // copy is uninitialized.
            System.loadLibrary(CAST_BROWSER_LIB);
            CommandLine.enableNativeProxy();
        } else {
            LibraryLoader.getInstance().ensureInitialized();
        }

        Log.d(TAG, "Loading BrowserStartupController...");
        BrowserStartupController.getInstance().setDisableLibraryLoadForCast(true);
        BrowserStartupController.getInstance().startBrowserProcessesSync(
                LibraryProcessType.PROCESS_BROWSER, /*singleProcess=*/false);
        NetworkChangeNotifier.init();
        // Cast shell always expects to receive notifications to track network state.
        NetworkChangeNotifier.registerToReceiveNotificationsAlways();
        sIsBrowserInitialized = true;
    }
}
