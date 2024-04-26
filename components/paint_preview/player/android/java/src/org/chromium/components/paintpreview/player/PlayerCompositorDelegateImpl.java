// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * This class and its native counterpart (player_compositor_delegate.cc) communicate with the Paint
 * Preview compositor.
 */
@JNINamespace("paint_preview")
public class PlayerCompositorDelegateImpl implements PlayerCompositorDelegate {
    private static final int LOW_MEMORY_THRESHOLD_KB = 2048;

    private CompositorListener mCompositorListener;
    private long mNativePlayerCompositorDelegate;
    private List<Runnable> mMemoryPressureListeners = new ArrayList<>();

    public PlayerCompositorDelegateImpl(
            NativePaintPreviewServiceProvider service,
            long nativeCaptureResultPtr,
            GURL url,
            String directoryKey,
            boolean mainFrameMode,
            @NonNull CompositorListener compositorListener,
            Callback<Integer> compositorErrorCallback) {
        mCompositorListener = compositorListener;
        if (service != null && service.getNativeBaseService() != 0) {
            TraceEvent.begin("PlayerCompositorDelegateImplJni.initialize()");
            mNativePlayerCompositorDelegate =
                    PlayerCompositorDelegateImplJni.get()
                            .initialize(
                                    this,
                                    service.getNativeBaseService(),
                                    nativeCaptureResultPtr,
                                    url.getSpec(),
                                    directoryKey,
                                    mainFrameMode,
                                    compositorErrorCallback,
                                    SysUtils.amountOfPhysicalMemoryKB() < LOW_MEMORY_THRESHOLD_KB);
            TraceEvent.end("PlayerCompositorDelegateImplJni.initialize()");
        }
        // TODO(crbug.com/40106234): Handle initialization errors when
        // mNativePlayerCompositorDelegate == 0.
    }

    @CalledByNative
    void onCompositorReady(
            @JniType("base::UnguessableToken") UnguessableToken rootFrameGuid,
            @JniType("std::vector<base::UnguessableToken>") UnguessableToken[] frameGuids,
            @JniType("std::vector") int[] frameContentSize,
            @JniType("std::vector") int[] scrollOffsets,
            @JniType("std::vector") int[] subFramesCount,
            @JniType("std::vector<base::UnguessableToken>") UnguessableToken[] subFrameGuids,
            @JniType("std::vector") int[] subFrameClipRects,
            float pageScaleFactor,
            long nativeAxTree) {
        mCompositorListener.onCompositorReady(
                rootFrameGuid,
                frameGuids,
                frameContentSize,
                scrollOffsets,
                subFramesCount,
                subFrameGuids,
                subFrameClipRects,
                pageScaleFactor,
                nativeAxTree);
    }

    @CalledByNative
    void onModerateMemoryPressure() {
        for (Runnable listener : mMemoryPressureListeners) {
            listener.run();
        }
    }

    @Override
    public void addMemoryPressureListener(Runnable runnable) {
        mMemoryPressureListeners.add(runnable);
    }

    @Override
    public int requestBitmap(
            UnguessableToken frameGuid,
            Rect clipRect,
            float scaleFactor,
            Callback<Bitmap> bitmapCallback,
            Runnable errorCallback) {
        if (mNativePlayerCompositorDelegate == 0) {
            return -1;
        }

        return PlayerCompositorDelegateImplJni.get()
                .requestBitmap(
                        mNativePlayerCompositorDelegate,
                        frameGuid,
                        bitmapCallback,
                        errorCallback,
                        scaleFactor,
                        clipRect.left,
                        clipRect.top,
                        clipRect.width(),
                        clipRect.height());
    }

    @Override
    public int requestBitmap(
            Rect clipRect,
            float scaleFactor,
            Callback<Bitmap> bitmapCallback,
            Runnable errorCallback) {
        return requestBitmap(null, clipRect, scaleFactor, bitmapCallback, errorCallback);
    }

    @Override
    public boolean cancelBitmapRequest(int requestId) {
        if (mNativePlayerCompositorDelegate == 0) {
            return false;
        }

        return PlayerCompositorDelegateImplJni.get()
                .cancelBitmapRequest(mNativePlayerCompositorDelegate, requestId);
    }

    @Override
    public void cancelAllBitmapRequests() {
        if (mNativePlayerCompositorDelegate == 0) {
            return;
        }

        PlayerCompositorDelegateImplJni.get()
                .cancelAllBitmapRequests(mNativePlayerCompositorDelegate);
    }

    @Override
    public GURL onClick(UnguessableToken frameGuid, int x, int y) {
        if (mNativePlayerCompositorDelegate == 0) {
            return null;
        }

        String url =
                PlayerCompositorDelegateImplJni.get()
                        .onClick(mNativePlayerCompositorDelegate, frameGuid, x, y);
        if (TextUtils.isEmpty(url)) return null;

        return new GURL(url);
    }

    @Override
    public Point getRootFrameOffsets() {
        if (mNativePlayerCompositorDelegate == 0) {
            return new Point();
        }

        int[] offsets =
                PlayerCompositorDelegateImplJni.get()
                        .getRootFrameOffsets(mNativePlayerCompositorDelegate);
        return new Point(offsets[0], offsets[1]);
    }

    @Override
    public void setCompressOnClose(boolean compressOnClose) {
        if (mNativePlayerCompositorDelegate == 0) {
            return;
        }

        PlayerCompositorDelegateImplJni.get()
                .setCompressOnClose(mNativePlayerCompositorDelegate, compressOnClose);
    }

    @Override
    public void destroy() {
        if (mNativePlayerCompositorDelegate == 0) {
            return;
        }

        PlayerCompositorDelegateImplJni.get().destroy(mNativePlayerCompositorDelegate);
        mNativePlayerCompositorDelegate = 0;
    }

    @NativeMethods
    interface Natives {
        long initialize(
                PlayerCompositorDelegateImpl caller,
                long nativePaintPreviewBaseService,
                long captureResultPtr,
                String urlSpec,
                String directoryKey,
                boolean mainFrameMode,
                Callback<Integer> compositorErrorCallback,
                boolean isLowMemory);

        void destroy(long nativePlayerCompositorDelegateAndroid);

        int requestBitmap(
                long nativePlayerCompositorDelegateAndroid,
                @JniType("std::optional<base::UnguessableToken>") UnguessableToken frameGuid,
                Callback<Bitmap> bitmapCallback,
                Runnable errorCallback,
                float scaleFactor,
                int clipX,
                int clipY,
                int clipWidth,
                int clipHeight);

        boolean cancelBitmapRequest(long nativePlayerCompositorDelegateAndroid, int requestId);

        void cancelAllBitmapRequests(long nativePlayerCompositorDelegateAndroid);

        String onClick(
                long nativePlayerCompositorDelegateAndroid,
                @JniType("std::optional<base::UnguessableToken>") UnguessableToken frameGuid,
                int x,
                int y);

        int[] getRootFrameOffsets(long nativePlayerCompositorDelegateAndroid);

        void setCompressOnClose(
                long nativePlayerCompositorDelegateAndroid, boolean compressOnClose);
    }
}
