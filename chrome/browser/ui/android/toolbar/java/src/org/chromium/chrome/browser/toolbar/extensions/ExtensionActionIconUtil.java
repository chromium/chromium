// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/** A utility class for handling extension action icons. */
@NullMarked
public class ExtensionActionIconUtil {
    /**
     * Retrieves the icon for a given extension action.
     *
     * @param context The context to use for accessing resources.
     * @param extensionActionsBridge The JNI bridge to the extension actions.
     * @param actionId The ID of the extension action to get the icon for.
     * @param tabId The ID of the tab to get the icon for.
     * @param webContents The webContents of the tab.
     * @return The icon for the extension action.
     */
    @Nullable
    public static Bitmap getActionIcon(
            Context context,
            ExtensionActionsBridge extensionActionsBridge,
            String actionId,
            int tabId,
            @Nullable WebContents webContents) {
        // The C++ rendering engine uses dp for its layout and size calculations. Therefore, we
        // convert the icon and badge dimensions from px to dp on Android before passing them to
        // the C++ layer. We also provide the device's display density to C++ to enable accurate
        // scaling and improve resolution.
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        float scaleFactor = display.getDipScale();
        int canvasWidthPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.extension_action_icon_canvas_width);
        int canvasHeightPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.extension_action_icon_canvas_height);
        int canvasWidthDp = DisplayUtil.pxToDp(display, canvasWidthPx);
        int canvasHeightDp = DisplayUtil.pxToDp(display, canvasHeightPx);

        return extensionActionsBridge.getActionIcon(
                actionId, tabId, webContents, canvasWidthDp, canvasHeightDp, scaleFactor);
    }
}
