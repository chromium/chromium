// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.components.webapps.WebappInstallSource;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the details associated with binding a {@link PwaBottomSheetController} to user
 * data on a {@link WindowAndroid}.
 */
public class PwaBottomSheetControllerProvider {
    /** The key used to bind the controller to the unowned data host. */
    private static final UnownedUserDataKey<PwaBottomSheetController> KEY =
            new UnownedUserDataKey<>(PwaBottomSheetController.class);

    /**
     * Get the shared {@link PwaBottomSheetController} from the provided {@link WindowAndroid}.
     *
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
     *
     * @param webContents The WebContents the UI is associated with.
     */
    @CalledByNative
    private static boolean canShowPwaBottomSheetInstaller(WebContents webContents) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        return controller != null && controller.canShowFor(webContents);
    }

    /**
     * Makes a request to show the PWA Bottom Sheet Installer UI.
     *
     * @param webContents The WebContents the UI is associated with.
     * @param icon The icon of the app represented by the UI.
     * @param isAdaptiveIcon Whether the app icon is adaptive or not.
     * @param title The title of the app represented by the UI.
     * @param origin The origin of the PWA app.
     * @param description The app description.
     */
    @CalledByNative
    private static void showPwaBottomSheetInstaller(
            long nativePwaBottomSheetController,
            WebContents webContents,
            Bitmap icon,
            boolean isAdaptiveIcon,
            String title,
            String origin,
            String description) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.requestBottomSheetInstaller(
                nativePwaBottomSheetController,
                webContents.getTopLevelNativeWindow(),
                webContents,
                icon,
                isAdaptiveIcon,
                title,
                origin,
                description);
    }

    /**
     * Makes a request to expand the PWA Bottom Sheet Installer UI.
     *
     * @param webContents The WebContents the UI is associated with.
     */
    @CalledByNative
    private static void expandPwaBottomSheetInstaller(WebContents webContents) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.expandBottomSheetInstaller();
    }

    /**
     * Returns whether the PWA Bottom Sheet Installer UI sheet exists and is visible.
     *
     * @param webContents The WebContents the UI is associated with.
     */
    @CalledByNative
    private static boolean doesBottomSheetExist(WebContents webContents) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        return (controller != null && controller.isBottomSheetVisible());
    }

    /**
     * Makes a request to update install source and maybe expand the PWA Bottom Sheet Installer UI.
     *
     * @param webContents The WebContents the UI is associated with.
     * @param installSource The source for triggering installation.
     * @param expandSheet Whether the Bottom Sheet Installer UI sheet should be expanded.
     */
    @CalledByNative
    private static void updateState(
            WebContents webContents, @WebappInstallSource int installSource, boolean expandSheet) {
        PwaBottomSheetController controller = fromWebContents(webContents);
        if (controller == null) return;
        controller.updateInstallSource(installSource);
        if (expandSheet) controller.expandBottomSheetInstaller();
    }
}
