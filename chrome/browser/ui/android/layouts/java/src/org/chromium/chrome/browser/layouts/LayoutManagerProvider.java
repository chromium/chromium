// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the details associated with binding a {@link LayoutManager} to user data on a
 * {@link WindowAndroid}.
 */
public class LayoutManagerProvider {
    /** An interface that allows a LayoutManager to be associated with an unowned data host. */
    interface Unowned extends LayoutManager, UnownedUserData {}

    /** The key used to bind the LayoutManager to the unowned data host. */
    private static final UnownedUserDataKey<Unowned> KEY = new UnownedUserDataKey<>(Unowned.class);

    /**
     * Get the shared {@link LayoutManager} from the provided {@link WindowAndroid}.
     * @param windowAndroid The window to pull the LayoutManager from.
     * @return A shared instance of a {@link LayoutManager}.
     */
    public static LayoutManager from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    static void attach(WindowAndroid windowAndroid, Unowned layoutManager) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), layoutManager);
    }

    static void detach(Unowned layoutManager) {
        KEY.detachFromAllHosts(layoutManager);
    }
}
