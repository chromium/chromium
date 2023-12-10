// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.url.GURL;

/**
 * Used for communicating with the Paint Preview delegate for requesting new bitmaps and forwarding
 * click events.
 */
public interface PlayerCompositorDelegate {
    /** An interface that creates an instance of {@link PlayerCompositorDelegate}. */
    interface Factory {
        PlayerCompositorDelegate create(
                NativePaintPreviewServiceProvider service,
                GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback);

        PlayerCompositorDelegate createForCaptureResult(
                NativePaintPreviewServiceProvider service,
                long nativeCaptureResultPtr,
                GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback);
    }

    /** Contains a callback for communication from native. */
    interface CompositorListener {
        /**
         * Called when the Paint Preview compositor is ready.
         *
         * @param rootFrameGuid The GUID for the root frame.
         * @param frameGuids Contains all frame GUIDs that are in this hierarchy.
         * @param frameContentSize Contains the content size for each frame. In native, this is
         * called scroll extent. The order corresponds to {@code frameGuids}. The content width and
         * height for the ith frame in {@code frameGuids} are respectively in the {@code 2*i} and
         * {@code 2*i+1} indices of {@code frameContentSize}.
         * @param scrollOffsets Contains the initial scroll offsets for each frame. The order
         * corresponds to {@code frameGuids}. The offset in x and y for the ith frame in
         * {@code frameGuids} are respectively in the {@code 2*i} and {@code 2*i+1} indices of
         * {@code scrollOffsets}.
         * @param subFramesCount Contains the number of sub-frames for each frame. The order
         * corresponds
         * to {@code frameGuids}. The number of sub-frames for the {@code i}th frame in {@code
         * frameGuids} is {@code subFramesCount[i]}.
         * @param subFrameGuids Contains the GUIDs of all sub-frames. The GUID for the {@code j}th
         * sub-frame of {@code frameGuids[i]} will be at {@code subFrameGuids[k]}, where {@code k}
         * is: <pre> int k = j; for (int s = 0; s < i; s++) k += subFramesCount[s];
         * </pre>
         * @param subFrameClipRects Contains clip rect values for each sub-frame. Each clip rect
         * value comes in a series of four consecutive integers that represent x, y, width, and
         * height. The clip rect values for the {@code j}th sub-frame of {@code frameGuids[i]} will
         * be at {@code subFrameGuids[4*k]}, {@code subFrameGuids[4*k+1]} ,
         * {@code subFrameGuids[4*k+2]}, and {@code subFrameGuids[4*k+3]}, where {@code k} has the
         * same value as above.
         * @param pageScaleFactor The initial scale factor of the page.
         * @param nativeAxTree Native pointer to the accessibility tree snapshot. The implementer
         * of this method will be the owner of this object and should delete it once it's used.
         */
        void onCompositorReady(
                UnguessableToken rootFrameGuid,
                UnguessableToken[] frameGuids,
                int[] frameContentSize,
                int[] scrollOffsets,
                int[] subFramesCount,
                UnguessableToken[] subFrameGuids,
                int[] subFrameClipRects,
                float pageScaleFactor,
                long nativeAxTree);
    }

    /**
     * Sets a runnable that is invoked when we are under memory pressure.
     * @param runnable The runnable to be invoked when under memory pressure.
     */
    void addMemoryPressureListener(Runnable runnable);

    /**
     * Requests a new bitmap for a frame from the Paint Preview compositor.
     * @param frameGuid The GUID of the frame.
     * @param clipRect The {@link Rect} for which the bitmap is requested. Note: this is relative
     * to the captured content.
     * @param scaleFactor The scale factor at which the bitmap should be rastered.
     * @param bitmapCallback The callback that receives the bitmap once it's ready. Won't get called
     * if there are any errors.
     * @param errorCallback Gets notified if there are any errors. Won't get called otherwise.
     * @return an int representing the ID for the bitmap request. Can be used with {@link
     * cancelBitmapRequest(int)} to cancel the request if possible.
     */
    int requestBitmap(
            UnguessableToken frameGuid,
            Rect clipRect,
            float scaleFactor,
            Callback<Bitmap> bitmapCallback,
            Runnable errorCallback);

    /**
     * Requests a new bitmap for a frame from the Paint Preview compositor if {@link mainFrameMode}
     * was passed as true as a parameter in the {@link Factory}
     * @param clipRect The {@link Rect} for which the bitmap is requested. Note: this is relative
     * to the captured content.
     * @param scaleFactor The scale factor at which the bitmap should be rastered.
     * @param bitmapCallback The callback that receives the bitmap once it's ready. Won't get called
     * if there are any errors.
     * @param errorCallback Gets notified if there are any errors. Won't get called otherwise.
     * @return an int representing the ID for the bitmap request. Can be used with {@link
     * cancelBitmapRequest(int)} to cancel the request if possible.
     */
    int requestBitmap(
            Rect clipRect,
            float scaleFactor,
            Callback<Bitmap> bitmapCallback,
            Runnable errorCallback);

    /**
     * Cancels the bitmap request for the provided ID.
     * @param requestID the request to try to cancel.
     * @return true on successful cancellation.
     */
    boolean cancelBitmapRequest(int requestId);

    /** Cancels all outstanding bitmap requests. */
    void cancelAllBitmapRequests();

    /**
     * Sends a click event for a frame to native for link hit testing.
     * @param frameGuid The GUID of the frame.
     * @param x The x coordinate of the click event, relative to the frame.
     * @param y The y coordinate of the click event, relative to the frame.
     * @return The URL that was clicked on. Null if there are no URLs.
     */
    GURL onClick(UnguessableToken frameGuid, int x, int y);

    /**
     * Gets the Root Frame Offsets for scroll matching.
     * @return The coordinates of the root frame offset.
     */
    default Point getRootFrameOffsets() {
        return new Point();
    }

    /**
     * Sets whether to compress the directory when closing the player.
     * @param compressOnClose Whether to compress the directory when closing.
     */
    default void setCompressOnClose(boolean compressOnClose) {}

    /** Called when PlayerCompositorDelegate needs to be destroyed. */
    default void destroy() {}
}
