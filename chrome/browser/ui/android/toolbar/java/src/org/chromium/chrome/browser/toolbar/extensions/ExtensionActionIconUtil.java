// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/** A utility class for handling extension action icons. */
@NullMarked
public class ExtensionActionIconUtil {
    /**
     * The dimension and scaling calculations required to render extension action icons in the C++
     * layer.
     */
    private static class ExtensionIconSpec {
        public final int widthDp;
        public final int heightDp;
        public final float scaleFactor;

        /* The C++ rendering engine uses dp for its layout and size calculations. Therefore, we
         * convert the icon and badge dimensions from px to dp on Android before passing them to
         * the C++ layer. We also provide the device's display density to C++ to enable accurate
         * scaling and improve resolution. */
        public ExtensionIconSpec(Context context, @Nullable WindowAndroid windowAndroid) {
            DisplayAndroid display =
                    (windowAndroid != null && windowAndroid.getDisplay() != null)
                            ? windowAndroid.getDisplay()
                            : DisplayAndroid.getNonMultiDisplay(context);

            int widthPx =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.extension_action_icon_canvas_width);
            int heightPx =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.extension_action_icon_canvas_height);

            if (display != null) {
                this.scaleFactor = display.getDipScale();
                this.widthDp = DisplayUtil.pxToDp(display, widthPx);
                this.heightDp = DisplayUtil.pxToDp(display, heightPx);
            } else {
                // Fallback for testing or cases where display is not available.
                this.scaleFactor = 1.0f;
                this.widthDp = widthPx;
                this.heightDp = heightPx;
            }
        }
    }

    /**
     * Retrieves the icon for a given extension action with {@link ExtensionsToolbarBridge}.
     *
     * @param context The context to use for accessing resources.
     * @param windowAndroid The WindowAndroid to use for display metrics.
     * @param extensionsToolbarBridge The JNI bridge to the extension actions.
     * @param actionId The ID of the extension action to get the icon for.
     * @param webContents The webContents of the tab.
     * @return The icon for the extension action.
     */
    @Nullable
    public static Bitmap getIcon(
            Context context,
            @Nullable WindowAndroid windowAndroid,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            String actionId,
            @Nullable WebContents webContents) {
        ExtensionIconSpec spec = new ExtensionIconSpec(context, windowAndroid);
        return extensionsToolbarBridge.getIcon(
                actionId, webContents, spec.widthDp, spec.heightDp, spec.scaleFactor);
    }
}
