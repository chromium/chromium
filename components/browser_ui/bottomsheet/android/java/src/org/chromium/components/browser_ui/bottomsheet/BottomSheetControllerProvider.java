// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import org.jni_zero.CalledByNative;

import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the details associated with binding a {@link BottomSheetController} to user
 * data on a {@link WindowAndroid}.
 */
public class BottomSheetControllerProvider {
    /** An interface that allows a controller to be associated with an unowned data host. */
    interface Unowned extends BottomSheetController, UnownedUserData {}

    /** The key used to bind the controller to the unowned data host. */
    private static final UnownedUserDataKey<Unowned> KEY = new UnownedUserDataKey<>(Unowned.class);

    /**
     * Get the shared {@link BottomSheetController} from the provided {@link WindowAndroid}.
     *
     * @param windowAndroid The window to pull the controller from.
     * @return A shared instance of a {@link BottomSheetController}.
     */
    @CalledByNative
    public static BottomSheetController from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    static void attach(WindowAndroid windowAndroid, Unowned controller) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), controller);
    }

    static void detach(Unowned controller) {
        KEY.detachFromAllHosts(controller);
    }
}
