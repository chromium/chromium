// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility class for interacting with the Chrome Item Picker. */
@NullMarked
public final class ChromeItemPickerUtils {
    public static final String ACTIVITY_CLASS_NAME =
            "org.chromium.chrome.browser.chrome_item_picker.ChromeItemPickerActivity";

    /** Private constructor to prevent instantiation. */
    private ChromeItemPickerUtils() {}

    /**
     * Creates an Intent to launch the Chrome Item Picker.
     *
     * @param context The context used to create the Intent and set the class name.
     * @return An intent to launch the Chrome Item Picker or null if it cannot be found.
     */
    public static @Nullable Intent createChromeItemPickerIntent(Context context) {
        try {
            return new Intent(context, Class.forName(ACTIVITY_CLASS_NAME));
        } catch (ClassNotFoundException e) {
            return null;
        }
    }
}
