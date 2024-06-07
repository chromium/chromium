// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Intent;

import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.Log;

/**
 * Helper class that serializes the command line arguments from Intent extra data.
 */
public class CastCommandLineHelper {
    private static final String TAG = "CastCommandLineHelper";
    // Default command line flags for `cast_browser` process.
    private static final String COMMAND_LINE_FILE = "castshell-command-line";
    private static final String CAST_COMMAND_LINE_PARAM_KEY =
            "com.google.android.apps.mediashell.CommandLineArgs";

    /**
     * Initializes the command line and set the arguments from the Intent extra data.
     *
     * @param intent Intent to use to load command line arguments from.
     */
    public static void initCommandLine(Intent intent) {
        CommandLineInitUtil.initCommandLine(COMMAND_LINE_FILE, null);

        if (intent == null) return;

        String[] commandLineArgs = intent.getStringArrayExtra(CAST_COMMAND_LINE_PARAM_KEY);
        if (commandLineArgs == null || commandLineArgs.length == 0) return;

        Log.d(TAG, "Applying command line arguments: count=%d", commandLineArgs.length);
        CommandLine.getInstance().appendSwitchesAndArguments(commandLineArgs);
    }

    /**
     * Store command line arguments to Intent's extra data.
     * @param intent Intent to store the command line arguments to.
     * @param commandLineArgs Command line arguments to store.
     */
    public static void setCommandLineArgs(Intent intent, String[] commandLineArgs) {
        assert (intent != null);

        if (commandLineArgs == null || commandLineArgs.length == 0) return;

        Log.d(TAG, "Setting command line arguments: count=%d", commandLineArgs.length);
        intent.putExtra(CAST_COMMAND_LINE_PARAM_KEY, commandLineArgs);
    }
}
