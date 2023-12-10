// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;

/** A simple implementation of {@link NativePaintPreviewServiceProvider} used in tests. */
@JNINamespace("paint_preview")
public class PaintPreviewTestService implements NativePaintPreviewServiceProvider {
    private static final String TAG = "PPTestService";
    private long mNativePaintPreviewBaseService;
    private long mNativePaintPreviewTestService;

    public PaintPreviewTestService(String path) {
        mNativePaintPreviewTestService = PaintPreviewTestServiceJni.get().getInstance(path);
        mNativePaintPreviewBaseService =
                PaintPreviewTestServiceJni.get().getBaseService(mNativePaintPreviewTestService);
    }

    @Override
    public long getNativeBaseService() {
        return mNativePaintPreviewBaseService;
    }

    public boolean createFramesForKey(String key, String url, FrameData rootFrameData) {
        if (mNativePaintPreviewTestService == 0) {
            Log.e(TAG, "No native service.");
            return false;
        }

        createFrames(rootFrameData, 0);

        boolean ret =
                PaintPreviewTestServiceJni.get()
                        .serializeFrames(mNativePaintPreviewTestService, key, url);

        if (!ret) {
            Log.e(TAG, "Native failed to setup files for testing.");
        }
        return ret;
    }

    private void createFrames(FrameData frameData, int id) {
        int[] childIds =
                PaintPreviewTestServiceJni.get()
                        .createSingleSkp(
                                mNativePaintPreviewTestService,
                                id,
                                frameData.getWidth(),
                                frameData.getHeight(),
                                frameData.getFlattenedLinkRects(),
                                frameData.getLinks(),
                                frameData.getFlattenedChildRects());

        FrameData[] childFrames = frameData.getChildFrames();
        assert childIds.length == childFrames.length;
        for (int i = 0; i < childIds.length; i++) {
            createFrames(childFrames[i], childIds[i]);
        }
    }

    @NativeMethods
    interface Natives {
        long getInstance(String path);

        long getBaseService(long nativePaintPreviewTestService);

        int[] createSingleSkp(
                long nativePaintPreviewTestService,
                int id,
                int width,
                int height,
                int[] flattenedLinkRects,
                String[] links,
                int[] flattenedChildRects);

        boolean serializeFrames(long nativePaintPreviewTestService, String key, String url);
    }
}
