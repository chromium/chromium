// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the details associated with binding a {@link PwaBottomSheetController}
 * to user data on a {@link WindowAndroid}.
 */
public class PwaBottomSheetControllerProvider {
    /** The key used to bind the controller to the unowned data host. */
    private static final UnownedUserDataKey<PwaBottomSheetController> KEY =
            new UnownedUserDataKey<>(PwaBottomSheetController.class);

    /**
     * Get the shared {@link PwaBottomSheetController} from the provided {@link
     * WindowAndroid}.
     * @param windowAndroid The window to pull the controller from.
     * @return A shared instance of a {@link PwaBottomSheetController}.
     */
    public static PwaBottomSheetController from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    static void attach(WindowAndroid windowAndroid, PwaBottomSheetController controller) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), controller);
    }

    static void detach(PwaBottomSheetController controller) {
        KEY.detachFromAllHosts(controller);
    }

    private static PwaBottomSheetController fromWebContents(WebContents webContents) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return from(window);
    }

    /**
     * Returns whether the bottom sheet installer can be shown.
     * @param webContents The WebContents the UI is associated with.
     */
    @CalledByNative
    private static boolean canShowPwaBottomSheetInstaller(WebContents webContents) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        return controller != null;
    }

    /**
     * Makes a request to show the PWA Bottom Sheet Installer UI.
     * @param webContents The WebContents the UI is associated with.
     * @param showExpanded Whether to show the UI in expanded mode or not.
     * @param icon The icon of the app represented by the UI.
     * @param isAdaptiveIcon Whether the app icon is adaptive or not.
     * @param title The title of the app represented by the UI.
     * @param origin The origin of the PWA app.
     * @param description The app description.
     * @param categories The categories this app falls under.
     */
    @CalledByNative
    private static void showPwaBottomSheetInstaller(long nativePwaBottomSheetController,
            WebContents webContents, boolean showExpanded, Bitmap icon, boolean isAdaptiveIcon,
            String title, String origin, String description, String categories) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.requestBottomSheetInstaller(nativePwaBottomSheetController,
                webContents.getTopLevelNativeWindow(), webContents, showExpanded, icon,
                isAdaptiveIcon, title, origin, description, categories);
    }
}
