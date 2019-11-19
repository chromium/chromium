// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Intent;
import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chromecast.base.Itertools;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Helper class for handling command line flags passed to the Cast Service through Intent extras.
 */
public class CastCommandLineHelper {
    private static final String TAG = "CastCmdLineHelper";
    private static final String COMMAND_LINE_FLAGS_PREF = "cast_command_line_args";

    private static final AtomicBoolean sCommandLineInitialized = new AtomicBoolean(false);

    private CastCommandLineHelper() {}

    /**
     * Parses whitelisted command line args from the provided Intent's extras and saves them to
     * persistent storage. No-op if the provided Intent is null.
     */
    public static void saveCommandLineArgsFromIntent(
            Intent intent, Iterable<String> commandLineArgs) {
        if (intent == null) return;

        // Store the command line args in a JSON object to safely preserve the separation between
        // switch name and value while fitting SharedPreferences' restricted set of supported types.
        JSONObject cmdLineArgs = new JSONObject();
        for (String swtch : commandLineArgs) {
            // getStringExtra() returning null doesn't necessarily indicate the extra doesn't exist.
            // It could be a null String extra, e.g. inserted with "--esn" from the command line or
            // putExtra(name, (String) null)). Null values map to a switch with no value.
            if (!intent.hasExtra(swtch)) continue;

            String value = intent.getStringExtra(swtch);
            try {
                cmdLineArgs.put(swtch, (value == null) ? JSONObject.NULL : value);
            } catch (JSONException e) {
                Log.e(TAG, "failed to add arg to JSON object: %s=%s", swtch, value);
                continue;
            }
        }

        // If no matching extras were found, we still overwrite the preference to ensure that old
        // args aren't applied.
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().putString(COMMAND_LINE_FLAGS_PREF, cmdLineArgs.toString()).apply();
    }

    /**
     * Reads command line args from persistent storage and initializes the CommandLine with those
     * args. Does not initialize CommandLine if it has been already been done.
     */
    public static void initCommandLineWithSavedArgs(CommandLineInitializer commandLineInitializer) {
        // CommandLine is a singleton, so check whether CastCommandLineHelper has initialized it
        // already and do nothing if so. We keep track of this in a static variable so we can still
        // assert that something else doesn't generally initialize the CommandLine before us.
        if (!sCommandLineInitialized.compareAndSet(false, true)) {
            Log.i(TAG, "command line already initialized by us, skipping");
            return;
        }
        assert !CommandLine.isInitialized();

        final JSONObject args = loadArgsFromPrefs(ContextUtils.getAppSharedPreferences());

        // Let the injected delegate do the standard command line initialization.
        final CommandLine cmdline = commandLineInitializer.initCommandLine();

        // If SharedPreferences contains any command line args previously saved from an Intent,
        // apply them as defaults, i.e. if a value isn't already present in the CommandLine
        // singleton. Values may already be present if loaded by the Application.
        for (String swtch : Itertools.fromIterator(args.keys())) {
            String value;
            try {
                value = args.isNull(swtch) ? null : args.getString(swtch);
            } catch (JSONException e) {
                Log.e(TAG, "failed to get string value for switch '%s'", swtch);
                continue;
            }

            if (!cmdline.hasSwitch(swtch)) {
                Log.d(TAG, "appending command line arg: %s=%s", swtch, value);
                cmdline.appendSwitchWithValue(swtch, value);
            } else {
                Log.w(TAG, "skipped command line arg from intent, value already present: %s=%s",
                        swtch, value);
            }
        }
    }

    private static JSONObject loadArgsFromPrefs(SharedPreferences prefs) {
        final String argsJson = prefs.getString(COMMAND_LINE_FLAGS_PREF, null);
        if (argsJson == null) {
            Log.i(TAG, "no saved command line args.");
            return new JSONObject();
        }
        try {
            return new JSONObject(argsJson);
        } catch (JSONException e) {
            Log.e(TAG, "failed to parse cmd line args stored in shared prefs: %s", e);
            return new JSONObject();
        }
    }

    @VisibleForTesting
    static void resetSavedArgsForTest() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit().remove(COMMAND_LINE_FLAGS_PREF).apply();
    }

    @VisibleForTesting
    static void resetCommandLineForTest() {
        CommandLine.reset();
        sCommandLineInitialized.set(false);
    }

    /**
     * Delegate interface for initCommandLineWithSavedArgs().
     *
     * The CommandLineInitializer is responsible for initializing the CommandLine singleton.
     * CommandLine.isInitialized() must be true after this interface's method is called.
     *
     * TODO(sanfin): This is a workaround for an upstream refactor at go/chromium-cl/794031.
     * Consider refactoring CastCommandLineHelper and other scattered CommandLine logic to depend
     * less on singletons and other code smells.
     */
    public interface CommandLineInitializer { public CommandLine initCommandLine(); }
}
