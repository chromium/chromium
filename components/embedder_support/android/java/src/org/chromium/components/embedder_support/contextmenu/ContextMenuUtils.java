// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;
import android.view.Window;
import android.webkit.URLUtil;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.mojom.MenuSourceType;

/** Provides utility methods for generating context menus. */
@NullMarked
public final class ContextMenuUtils {
    private ContextMenuUtils() {}

    /** Returns the title for the given {@link ContextMenuParams}. */
    public static String getTitle(ContextMenuParams params) {
        if (!TextUtils.isEmpty(params.getTitleText())) {
            return params.getTitleText();
        }
        if (!TextUtils.isEmpty(params.getLinkText())) {
            return params.getLinkText();
        }
        if (params.isImage() || params.isVideo() || params.isFile()) {
            return URLUtil.guessFileName(params.getSrcUrl().getSpec(), null, null);
        }
        return "";
    }

    /**
     * Get the suffix for the context menu type determined by the params. Histogram values should
     * match with the values defined in histogram_suffixes_list.xml under ContextMenuTypeAndroid
     *
     * @param params The list of params for the opened context menu.
     * @return A string value for the histogram suffix.
     */
    public static String getContextMenuTypeForHistogram(ContextMenuParams params) {
        if (params.isVideo()) {
            return "Video";
        } else if (params.isImage()) {
            return params.isAnchor() ? "ImageLink" : "Image";
        } else if (params.getOpenedFromHighlight()) {
            return "SharedHighlightingInteraction";
        } else if (params.isPage()) {
            return "Page";
        }
        assert params.isAnchor();
        return "Link";
    }

    /**
     * Checks whether anchored popup windows are supported in the given context.
     *
     * <p>If it is supported, the context menu should be displayed as a popup window. Otherwise, it
     * is shown as a dialog. Note that only contexts that are meaningfully associated with a display
     * should be used.
     *
     * @param context The application {@link Context}.
     * @return True if anchored popup windows are supported in the given context, false otherwise.
     * @see DeviceFormFactor#isNonMultiDisplayContextOnTablet(Context).
     */
    public static boolean isPopupSupported(@Nullable Context context) {
        if (context == null || !CommandLine.isInitialized()) return false;

        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                || CommandLine.getInstance()
                        .hasSwitch(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP);
    }

    /**
     * Checks if drag and drop is enabled for context menu.
     *
     * @param context The application {@link Context}.
     * @return True if drag and drop is enabled, false otherwise.
     */
    public static boolean isDragDropEnabled(Context context) {
        return ContentFeatureMap.isEnabled(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU)
                && isPopupSupported(context);
    }

    /**
     * Checks if the context menu should be displayed as a mouse driven popup or if it was opened
     * from a shared highlight.
     *
     * @param params The {@link ContextMenuParams} for the context menu.
     * @return True if the menu should be displayed as a popup, false otherwise.
     */
    public static boolean isMouseOrHighlightPopup(ContextMenuParams params) {
        return params.getSourceType() == MenuSourceType.MOUSE || params.getOpenedFromHighlight();
    }

    /**
     * Provides the trigger rectangle for the context menu, used by AnchoredPopupWindow.
     *
     * <p>Calculates the appropriate rectangle to anchor the context menu, taking into account
     * whether a drag operation is active.
     *
     * @param context The application {@link Context}.
     * @param webContents The {@link WebContents} instance.
     * @param params The {@link ContextMenuParams} for the context menu.
     * @param topContentOffsetPx The top content offset in pixels.
     * @param usePopupWindow Whether the menu should be displayed as a popup window.
     * @param containerView The container {@link View} for the context menu.
     * @return A {@link Rect} representing the anchor for the {@link android.widget.PopupWindow}.
     */
    public static Rect getContextMenuAnchorRect(
            Context context,
            Window window,
            WebContents webContents,
            ContextMenuParams params,
            float topContentOffsetPx,
            boolean usePopupWindow,
            View containerView) {
        Point touchPoint =
                getTouchPointCoordinates(
                        context, window, params, topContentOffsetPx, usePopupWindow, containerView);

        // If drag drop is enabled, the context menu needs to be anchored next to the drag shadow.
        // Otherwise, the Rect used to display the context menu dialog can be a single point.
        if (isDragDropEnabled(context)) {
            return computeDragShadowRect(webContents, touchPoint.x, touchPoint.y);
        } else {
            return new Rect(touchPoint.x, touchPoint.y, touchPoint.x, touchPoint.y);
        }
    }

    /**
     * Calculates the touch point coordinates for the context menu.
     *
     * <p>These coordinates are adjusted based on whether the menu is displayed as a popup and take
     * into account the container view's position and window offset.
     *
     * @param context The application {@link Context}.
     * @param params The {@link ContextMenuParams} for the context menu.
     * @param topContentOffsetPx The top content offset in pixels.
     * @param usePopupWindow Whether the menu should be displayed as a popup window.
     * @param containerView The container {@link View} for the context menu.
     * @return A point representing the [x, y] coordinates of the touch point.
     */
    private static Point getTouchPointCoordinates(
            Context context,
            Window window,
            ContextMenuParams params,
            float topContentOffsetPx,
            boolean usePopupWindow,
            View containerView) {
        final float density = context.getResources().getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;

        int x = (int) touchPointXPx;
        int y = (int) (touchPointYPx + topContentOffsetPx);

        // When context menu is a popup, the coordinates are expected to be screen coordinates as
        // they'll be used to calculate coordinates for PopupMenu#showAtLocation. This is required
        // for multi-window use cases as well.
        if (usePopupWindow) {
            int[] layoutScreenLocation = new int[2];
            containerView.getLocationOnScreen(layoutScreenLocation);
            x += layoutScreenLocation[0];
            y += layoutScreenLocation[1];

            // Also take the Window offset into account. This is necessary when a partial width/
            // height window hosts the activity.
            var attrs = window.getAttributes();
            x += attrs.x;
            y += attrs.y;
        }

        return new Point(x, y);
    }

    /**
     * Provides the rectangle used to display the context menu when a drag operation is in progress.
     *
     * <p>If a drag operation is not active, a single-point {@link Rect} is returned at the given
     * touch coordinates.
     *
     * @param webContents The {@link WebContents} instance.
     * @param centerX The X coordinate of the touch event.
     * @param centerY The Y coordinate of the touch event.
     * @return A {@link Rect} representing the anchor for the context menu.
     */
    @VisibleForTesting
    public static Rect computeDragShadowRect(WebContents webContents, int centerX, int centerY) {
        ViewAndroidDelegate viewAndroidDelegate = webContents.getViewAndroidDelegate();

        if (viewAndroidDelegate != null) {
            DragStateTracker dragStateTracker = viewAndroidDelegate.getDragStateTracker();
            if (dragStateTracker != null && dragStateTracker.isDragStarted()) {
                int shadowHeight = dragStateTracker.getDragShadowHeight();
                int shadowWidth = dragStateTracker.getDragShadowWidth();

                int left = centerX - shadowWidth / 2;
                int right = centerX + shadowWidth / 2;
                int top = centerY - shadowHeight / 2;
                int bottom = centerY + shadowHeight / 2;

                return new Rect(left, top, right, bottom);
            }
        }

        return new Rect(centerX, centerY, centerX, centerY);
    }
}
