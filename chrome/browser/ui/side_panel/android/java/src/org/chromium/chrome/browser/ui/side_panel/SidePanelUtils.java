// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Utility class for Side Panel. */
@NullMarked
public class SidePanelUtils {
    private static final String TAG = "SidePanel";

    /**
     * Logs a message with a consistent tag and a class-specific prefix.
     *
     * @param tag A prefix for the log message, should be the class name.
     * @param message The message to log.
     * @param args Optional arguments to include in the log, appended to the end of the log message.
     */
    public static void log(String tag, String message, @Nullable Object... args) {
        if (!ChromeFeatureList.sEnableAndroidSidePanelLogs.isEnabled()) {
            return;
        }

        String content = tag + ":" + message;
        String optionalSuffix = "";
        if (args != null && args.length > 0 && !message.contains("%")) {
            StringBuilder sb = new StringBuilder();
            sb.append(" [");
            for (int i = 0; i < args.length; i++) {
                sb.append(args[i]);
                if (i < args.length - 1) {
                    sb.append(", ");
                }
            }
            sb.append("]");
            optionalSuffix = sb.toString();
        }

        Log.e(TAG, content + optionalSuffix);
    }
}
