// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.graphics.Color;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;

/**
 * Contains all of the command line switches that are specific to Chromecast on
 * on Android.
 */
public abstract class CastSwitches {
    private static final String TAG = "CastSwitches";

    // Background color to use when chromium hasn't rendered anything yet. This will often be
    // displayed briefly when loading a Cast app. Format is a #ARGB in hex. (Black: #FF000000,
    // Green: #FF009000, and so on)
    public static final String CAST_APP_BACKGROUND_COLOR = "cast-app-background-color";

    public static int getSwitchValueColor(String switchName, int defaultValue) {
        String colorString = CommandLine.getInstance().getSwitchValue(switchName);
        try {
            if (colorString != null) {
                return Color.parseColor(colorString);
            }
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Invalid value for %s (%s). Using default.",
                    CastSwitches.CAST_APP_BACKGROUND_COLOR, colorString);
        }
        return defaultValue;
    }

    // Prevent instantiation
    private CastSwitches() {}
}
