// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

/**
 * A class responsible for binding and unbinding a {@link SnackbarManager} to a
 * {@link WindowAndroid}.
 */
public class SnackbarManagerProvider {
    /** The key for accessing this object on an {@link org.chromium.base.UnownedUserDataHost}. */
    private static final UnownedUserDataKey<SnackbarManager> KEY =
            new UnownedUserDataKey<>(SnackbarManager.class);

    /**
     * Get the activity's main {@link SnackbarManager} from the provided {@link WindowAndroid}.
     * @param window The window to get the manager from.
     * @return The activity's main {@link SnackbarManager}.
     */
    public static SnackbarManager from(WindowAndroid window) {
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * WARNING: Do not use this unless you know what you're doing!
     *
     * Make a snackbar manager available through the activity's window.
     * @param window A {@link WindowAndroid} to attach to.
     * @param manager The {@link SnackbarManager} to attach.
     */
    public static void attach(WindowAndroid window, SnackbarManager manager) {
        KEY.attachToHost(window.getUnownedUserDataHost(), manager);
    }

    /**
     * WARNING: Do not use this unless you know what you're doing!
     *
     * Detach the provided snackbar manager from any host it is associated with.
     * @param manager The {@link SnackbarManager} to detach.
     */
    public static void detach(SnackbarManager manager) {
        KEY.detachFromAllHosts(manager);
    }
}
