// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import javax.annotation.Nonnull;

/**
 * This class and its native counterpart (player_compositor_delegate.cc) communicate with the Paint
 * Preview compositor.
 */
@JNINamespace("paint_preview")
class PlayerCompositorDelegateImpl implements PlayerCompositorDelegate {
    interface CompositorListener {
        void onCompositorReady(long rootFrameGuid, long[] frameGuids, int[] frameContentSize,
                int[] subFramesCount, long[] subFrameGuids, int[] subFrameClipRects);
    }

    private CompositorListener mCompositorListener;
    private long mNativePaintPreviewPlayerMediator;

    PlayerCompositorDelegateImpl(String url, @Nonnull CompositorListener compositorListener) {
        mCompositorListener = compositorListener;
        mNativePaintPreviewPlayerMediator =
                PlayerCompositorDelegateImplJni.get().initialize(this, url);
    }

    /**
     * Called by native when the Paint Preview compositor is ready.
     * @param rootFrameGuid The GUID for the root frame.
     * @param frameGuids Contains all frame GUIDs that are in this hierarchy.
     * @param frameContentSize Contains the content size for each frame. In native, this is called
     * scroll extent. The order corresponds to {@code frameGuids}. The content width and height for
     * the ith frame in {@code frameGuids} are respectively in the {@code 2*i} and {@code 2*i+1}
     * indices of {@code frameContentSize}.
     * @param subFramesCount Contains the number of sub-frames for each frame. The order
     * corresponds to {@code frameGuids}. The number of sub-frames for the {@code i}th frame in
     * {@code frameGuids} is {@code subFramesCount[i]}.
     * @param subFrameGuids Contains the GUIDs of all sub-frames. The GUID for the {@code j}th
     * sub-frame of {@code frameGuids[i]} will be at {@code subFrameGuids[k]}, where {@code k} is:
     * <pre>
     *     int k = j;
     *     for (int s = 0; s < i; s++) k += subFramesCount[s];
     * </pre>
     * @param subFrameClipRects Contains clip rect values for each sub-frame. Each clip rect value
     * comes in a series of four consecutive integers that represent x, y, width, and height.
     * The clip rect values for the {@code j}th sub-frame of {@code frameGuids[i]} will be at
     * {@code subFrameGuids[4*k]}, {@code subFrameGuids[4*k+1]} , {@code subFrameGuids[4*k+2]},
     * and {@code subFrameGuids[4*k+3]}, where {@code k} has the same value as above.
     */
    @CalledByNative
    void onCompositorReady(long rootFrameGuid, long[] frameGuids, int[] frameContentSize,
            int[] subFramesCount, long[] subFrameGuids, int[] subFrameClipRects) {
        mCompositorListener.onCompositorReady(rootFrameGuid, frameGuids, frameContentSize,
                subFramesCount, subFrameGuids, subFrameClipRects);
    }

    @Override
    public void requestBitmap(long frameGuid, Rect clipRect, float scaleFactor,
            Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
        if (mNativePaintPreviewPlayerMediator == 0) return;

        PlayerCompositorDelegateImplJni.get().requestBitmap(mNativePaintPreviewPlayerMediator,
                frameGuid, bitmapCallback, errorCallback, scaleFactor, clipRect.left, clipRect.top,
                clipRect.width(), clipRect.height());
    }

    @Override
    public void onClick(long frameGuid, Point point) {
        if (mNativePaintPreviewPlayerMediator == 0) return;

        PlayerCompositorDelegateImplJni.get().onClick(
                mNativePaintPreviewPlayerMediator, frameGuid, point.x, point.y);
    }

    void destroy() {
        if (mNativePaintPreviewPlayerMediator == 0) return;

        PlayerCompositorDelegateImplJni.get().destroy(mNativePaintPreviewPlayerMediator);
        mNativePaintPreviewPlayerMediator = 0;
    }

    @NativeMethods
    interface Natives {
        long initialize(PlayerCompositorDelegateImpl caller, String url);
        void destroy(long nativePlayerCompositorDelegateAndroid);
        void requestBitmap(long nativePlayerCompositorDelegateAndroid, long frameGuid,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback, float scaleFactor,
                int clipX, int clipY, int clipWidth, int clipHeight);
        void onClick(long nativePlayerCompositorDelegateAndroid, long frameGuid, int x, int y);
    }
}