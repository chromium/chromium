// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

/**
 * A helper class that stores Chrome cached flag values to allow them to be used in //components.
 *
 * <p>TODO(crbug.com/40266922): Remove this class after code changes allow for //components to
 * access cached flags.
 */
public class BrowserUiUtilsCachedFlags {

    /** The singleton instance for this class. */
    private static BrowserUiUtilsCachedFlags sInstance;

    private boolean mAsyncNotificationManager;

    /** Returns the singleton instance, creating one if needed. */
    public static BrowserUiUtilsCachedFlags getInstance() {
        if (sInstance == null) {
            sInstance = new BrowserUiUtilsCachedFlags();
        }
        return sInstance;
    }

    /** Sets whether to use async notiication manager. */
    public void setAsyncNotificationManagerFlag(boolean value) {
        mAsyncNotificationManager = value;
    }

    public boolean getAsyncNotificationManagerFlag() {
        return mAsyncNotificationManager;
    }
}
