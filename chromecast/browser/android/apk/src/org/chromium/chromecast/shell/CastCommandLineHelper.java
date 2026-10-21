// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Intent;

import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Helper class that serializes the command line arguments from Intent extra data. */
public class CastCommandLineHelper {
    private static final String TAG = "CastCmdLineHelper";
    // Default command line flags for `cast_browser` process.
    private static final String COMMAND_LINE_FILE = "castshell-command-line";
    private static final String CAST_COMMAND_LINE_PARAM_KEY =
            "com.google.android.apps.mediashell.CommandLineArgs";

    private static final Set<String> ALLOWED_SWITCHES =
            new HashSet<>(
                    Arrays.asList(
                            "enable-cast-receiver",
                            "cast-app-id",
                            "remote-debugging-port",
                            "v",
                            "enable-features",
                            "cast-core-service-endpoint",
                            "cast-core-runtime-id",
                            "runtime-service-path",
                            "enable-grpc-over-tcpip",
                            "runtime-auth-token",
                            "metrics-recording-only",
                            "force-video-overlays",
                            "javaless-renderers"));

    /**
     * Initializes the command line and set the arguments from the Intent extra data.
     *
     * @param intent Intent to use to load command line arguments from.
     */
    public static void initCommandLine(Intent intent) {
        CommandLineInitUtil.initCommandLine(COMMAND_LINE_FILE, null);

        if (intent == null) {
            return;
        }

        String[] commandLineArgs = intent.getStringArrayExtra(CAST_COMMAND_LINE_PARAM_KEY);
        if (commandLineArgs == null || commandLineArgs.length == 0) {
            return;
        }

        List<String> safeArgs = new ArrayList<>();
        for (String arg : commandLineArgs) {
            if (arg == null) {
                continue;
            }
            // Must start with at least one dash to be a switch.
            if (!arg.startsWith("-")) {
                // If it doesn't start with "-", it's a positional argument.
                // We block them by default for security, as we only expect allowed switches.
                continue;
            }

            // Handle "--" or "-" case (not a valid switch).
            if (arg.equals("-") || arg.equals("--")) {
                continue;
            }

            String switchName = arg.startsWith("--") ? arg.substring(2) : arg.substring(1);
            int equalsIndex = switchName.indexOf('=');
            if (equalsIndex > 0) {
                switchName = switchName.substring(0, equalsIndex);
            }

            if (ALLOWED_SWITCHES.contains(switchName)) {
                safeArgs.add(arg);
            }
        }

        Log.d(TAG, "Applying command line arguments: count=%d", safeArgs.size());
        CommandLine.getInstance().appendSwitchesAndArguments(safeArgs.toArray(new String[0]));
    }

    /**
     * Store command line arguments to Intent's extra data.
     *
     * @param intent Intent to store the command line arguments to.
     * @param commandLineArgs Command line arguments to store.
     */
    public static void setCommandLineArgs(Intent intent, String[] commandLineArgs) {
        assert (intent != null);

        if (commandLineArgs == null || commandLineArgs.length == 0) {
            return;
        }

        Log.d(TAG, "Setting command line arguments: count=%d", commandLineArgs.length);
        intent.putExtra(CAST_COMMAND_LINE_PARAM_KEY, commandLineArgs);
    }
}
