// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** Binds an {@link AdaptiveUiController} to a {@link WindowAndroid}. */
@NullMarked
final class AdaptiveUiControllerProvider {
    private static final UnownedUserDataKey<AdaptiveUiController> KEY = new UnownedUserDataKey<>();

    private AdaptiveUiControllerProvider() {}

    static @Nullable AdaptiveUiController from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attaches {@code controller} to {@code windowAndroid} so {@link AdaptiveUiController#from}
     * returns it.
     *
     * @param windowAndroid The window to attach to.
     * @param controller The controller for that window.
     */
    static void attach(WindowAndroid windowAndroid, AdaptiveUiController controller) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), controller);
    }

    /**
     * Detaches {@code controller} from every window it was attached to.
     *
     * @param controller The controller to detach.
     */
    static void detach(AdaptiveUiController controller) {
        KEY.detachFromAllHosts(controller);
    }
}
